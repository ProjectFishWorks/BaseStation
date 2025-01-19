#include "auth0DeviceAuthorization.h"
#include <HTTPClient.h>
#include <LittleFS.h>


MQTTLogin Auth0DeviceAuthorization::getMQTTLogin(uint8_t forceNewLogin) {
    String baseURL = "https://dev-xk74bsamhi6vyn3g.ca.auth0.com";
    const char* clientId = "wakI3rj2wJbgMY9W1vKQAOi2jdtrGV6C";
    const char* audience = "emqx.projectfishworks.ca";

    Serial.println("Checking for token file");
    //Check if the token file exists
    if(LittleFS.exists("/mqtt-token.json") && !forceNewLogin){
        File tokenFile = LittleFS.open("/mqtt-token.json", "r");
        String tokenString = tokenFile.readString();
        tokenFile.close();

        JsonDocument tokenDoc;

        DeserializationError error = deserializeJson(tokenDoc, tokenString);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
        }

        //Get current time
        time_t now;
        time(&now);

        //Check if the token has expired
        if(tokenDoc["expires_at"].as<time_t>() > now){
            Serial.println("Token has not expired");
            return MQTTLogin(audience, 8883, clientId, "", tokenDoc["access_token"].as<String>());
        }else{
            Serial.println("Token has expired, requesting new access token using refresh token");
            HTTPClient http;
            http.begin(baseURL + "/oauth/token");
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");

            String postData = "client_id=" + String(clientId) + "&audience=" + String(audience) + "&grant_type=refresh_token" + "&refresh_token=" + tokenDoc["refresh_token"].as<String>();

            int httpResponseCode = http.POST(postData);

            if (httpResponseCode > 0) {
                String response = http.getString();

                Serial.println(httpResponseCode);
                Serial.println("Got new token");

                JsonDocument newTokenDoc;

                DeserializationError newTokenError = deserializeJson(newTokenDoc, response);

                if (newTokenError) {
                    Serial.print(F("deserializeJson() failed: "));
                    Serial.println(newTokenError.f_str());
                }

                //Get current time
                time_t now;
                time(&now);

                //Add an expires_at field to the token document
                newTokenDoc["expires_at"] = now + newTokenDoc["expires_in"].as<int>() - 10;

                //Write the token document to the token file
                Serial.println("Writing new token to file");

                tokenDoc["expires_at"] = now + newTokenDoc["expires_in"].as<int>();
                tokenDoc["access_token"] = newTokenDoc["access_token"];

                LittleFS.remove("/mqtt-token.json");

                File tokenFile = LittleFS.open("/mqtt-token.json", "w");
                tokenFile.print(tokenDoc.as<String>());
                tokenFile.close();

                return MQTTLogin(audience, 8883, clientId, "", newTokenDoc["access_token"].as<String>());

            } else {
                Serial.print("Error on sending POST: ");
                Serial.println(httpResponseCode);
            }

            http.end();

        }
    }else{
        Serial.println(forceNewLogin ? "Forced new login" : "No token file found");
    }

    Serial.println("No access token found, requesting new access token");
    Serial.println("Requesting device code");

    HTTPClient http;
    http.begin(baseURL + "/oauth/device/code");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "client_id=" + String(clientId) + "&audience=" + String(audience) + "&scope=offline_access";

    int httpResponseCode = http.POST(postData);
    

    if (httpResponseCode > 0) {
        String response = http.getString();

        Serial.println(httpResponseCode);
        Serial.println("Got device code");


        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
        }

        const char* device_code = doc["device_code"];
        const char* user_code = doc["user_code"];
        const char* verification_uri = doc["verification_uri"];
        const char* verification_uri_complete = doc["verification_uri_complete"];
        //int expires_in = doc["expires_in"];
        int interval = doc["interval"];

        Serial.println(device_code);
        Serial.println(user_code);
        Serial.println(verification_uri);
        Serial.println(verification_uri_complete);
        Serial.println(interval);


        if (verificationCallback) {
            verificationCallback(verification_uri_complete, user_code);
        }else{
            Serial.println("No verification callback set");
        }

        uint16_t responseCode = 0;

        http.begin(baseURL + "/oauth/token");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        String tokenPostData = "grant_type=urn:ietf:params:oauth:grant-type:device_code";
        tokenPostData += "&device_code=" + String(device_code);
        tokenPostData += "&client_id=" + String(clientId);
        
        String tokenResponse = "";

        do{

            responseCode = http.POST(tokenPostData);

            if (responseCode > 0) {
                tokenResponse = http.getString();
                Serial.println(tokenResponse);
            } else {
                Serial.print("Error on sending POST for token: ");
                Serial.println(responseCode);
            }

            delay(interval * 1000);
        }
        while(responseCode != 200);

        Serial.println("Got token");
        Serial.println(responseCode);
        Serial.println(http.getString());

        http.end();
        
        JsonDocument tokenDoc;

        DeserializationError tokenError = deserializeJson(tokenDoc, tokenResponse);

        if (tokenError) {
            Serial.print(F("Token Doc deserializeJson() failed: "));
            Serial.println(tokenError.f_str());
            return MQTTLogin(audience, 8883, clientId, "", "");
        }

        //Get current time
        time_t now;
        time(&now);
        
        //Add an expires_at field to the token document
        tokenDoc["expires_at"] = now + tokenDoc["expires_in"].as<int>() - 10;

        //Write the token document to the token file
        Serial.println("Writing token to file");

        if(LittleFS.exists("/mqtt-token.json")){
            LittleFS.remove("/mqtt-token.json");
        }

        File tokenFile = LittleFS.open("/mqtt-token.json", "w");
        tokenFile.print(tokenDoc.as<String>());
        tokenFile.close();


        return MQTTLogin(audience, 8883, clientId, "", tokenDoc["access_token"].as<String>());

    } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
    }

    String mqttClientId = "BaseStationClient-";
    mqttClientId += String(random(0xffff), HEX);

    return MQTTLogin(audience, 8883, clientId, "", "");

}

void Auth0DeviceAuthorization::requestDeviceCode(String baseURL, String clientId, String audience) {

    



}
