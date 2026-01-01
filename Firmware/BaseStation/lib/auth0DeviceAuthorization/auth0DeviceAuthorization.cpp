#include "auth0DeviceAuthorization.h"
#include <HTTPClient.h>
#include <LittleFS.h>

#include "JWT_RS256.h"


MQTTLogin Auth0DeviceAuthorization::getMQTTLogin(uint8_t forceNewLogin) {
    String baseURL = "https://dev-xk74bsamhi6vyn3g.ca.auth0.com";
    const char* clientId = "wakI3rj2wJbgMY9W1vKQAOi2jdtrGV6C";
    const char* audience = "emqx.projectfishworks.ca";

    Serial.println("Checking for token file");
    //Check if the token file exists
    if(LittleFS.exists(TOKEN_FILE) && !forceNewLogin){
        File tokenFile = LittleFS.open(TOKEN_FILE, "r");
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

                LittleFS.remove(TOKEN_FILE);

                File tokenFile = LittleFS.open(TOKEN_FILE, "w");
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

        if(LittleFS.exists(TOKEN_FILE)){
            LittleFS.remove(TOKEN_FILE);
        }

        File tokenFile = LittleFS.open(TOKEN_FILE, "w");
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

String Auth0DeviceAuthorization::getUserID() {

    if(LittleFS.exists(TOKEN_FILE)){
        File tokenFile = LittleFS.open(TOKEN_FILE, "r");
        String tokenString = tokenFile.readString();
        tokenFile.close();

        JsonDocument tokenDoc;

        DeserializationError error = deserializeJson(tokenDoc, tokenString);

        if (error) {
            Serial.print(F("token deserializeJson() failed: "));
            Serial.println(error.f_str());
            return "";
        }

        Serial.println("Parsing token");

        char* rsa_public_key = "-----BEGIN PUBLIC KEY-----\nMIIDHTCCAgWgAwIBAgIJIo+aXdGh5cv+MA0GCSqGSIb3DQEBCwUAMCwxKjAoBgNVBAMTIWRldi14azc0YnNhbWhpNnZ5bjNnLmNhLmF1dGgwLmNvbTAeFw0yNTAxMDkwNTM3MTdaFw0zODA5MTgwNTM3MTdaMCwxKjAoBgNVBAMTIWRldi14azc0YnNhbWhpNnZ5bjNnLmNhLmF1dGgwLmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMNGitWlzThJml941EslLw09Bd2cUsBXF+JSA39b28NGAElR46mnWyVPHBtkQrPojYG8pbQ6BZU45eUZHN0Vj+VYkCSEpAHlIK5pGbsAZQDqPxfk2bzyw6Aoe81peOyZXh3s/q/Qn6WOGOEaX/hrmyEECSAPf9XVpZGIIz6WAWzo6dz/UPrFe/3EPa2YfStoyeESmWs0mLbpGkihawbRmQ6ZzePqgueA013F3dHwiPxcUrkYOxb0UZYDUfZIg9k1hvo5dbGn0WAky/YLarp+4C9053a9SbuMeL1ogG7u/h8Zx3FEEvZah6gSVdXBgNMqIDwamZShcUvX75hxwoXSHwcCAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUSXpUrOo+t1rZnZldZyt3cPXvhqMwDgYDVR0PAQH/BAQDAgKEMA0GCSqGSIb3DQEBCwUAA4IBAQCareAd3qXNEUvodffTCpEZnHRDNfLFPv6ed1H5JkxKb1Y7aAt0tLGFWIa5D/Mx+00MYAzUOFTWjqIapVY1MxAvYze87UJ3JMqt7fFo8YOBG2AbKZx+FvGZ4/+Ou0oBAhcbxbaeO3Td8nPdGxDowwlN5m9MJRaNMC3q5EeFIVshtUuhiQWLRDd7k1oYEJ4U0SIqsz2ON6/x5pXnZA4+mistE4JDIoOsZLwfe5CD/U99PW1yAhTafeXrn5XocmktbKLUoGpiZVS0nlg53u+WHyPEFOdsCBPP9YJ7AH/GBdG5EXJM0B8VlbBpk+fhas+uaryjp73NW2SrtMLOdTUP38hi\n-----END PUBLIC KEY-----";

        JWT_RS256 jwt;

        jwt.rsa_public_key = rsa_public_key;

        Serial.println("Not verifying signature - implement this later");
        // bool isValid = jwt.tokenIsValid(tokenDoc["access_token"].as<String>());

        // if (isValid) {
        //     Serial.println("Token is valid!");
        // } else {
        //     Serial.println("Token is invalid.");
        // }

        ParsedToken parsedToken = jwt.parseToken(tokenDoc["access_token"].as<String>());

        Serial.println("Payload:");
        Serial.println(parsedToken.payload);

        JsonDocument jwtDoc;

        DeserializationError jwtError = deserializeJson(jwtDoc, parsedToken.payload);

        if (jwtError) {
            Serial.print(F("jwt deserializeJson() failed: "));
            Serial.println(jwtError.f_str());
            return "";
        }

        Serial.print("Sub:");
        Serial.println(jwtDoc["sub"].as<String>());

        return jwtDoc["sub"].as<String>();

        // unsigned char token[1024];
        // unsigned char tokenStringChar[2048];

        // strcpy((char*)token, tokenDoc["access_token"].as<String>().c_str());

        // Serial.println("Token:");

        // Serial.println(mbedtls_base64_decode(tokenStringChar, 2048, NULL, token, 20));


        // JsonDocument jwtDoc;

        // DeserializationError jwtError = deserializeJson(jwtDoc, tokenStringChar);

        // if (jwtError) {
        //     Serial.print(F("jwt deserializeJson() failed: "));
        //     Serial.println(jwtError.f_str());
        //     return "";
        // }

        // Serial.print("Payload:");
        // Serial.println(jwtDoc.as<String>()); 
        
        // return jwtDoc["sub"].as<String>();

    }

    Serial.println("No token file found");
    return "";
}
