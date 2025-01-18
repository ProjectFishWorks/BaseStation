#include "auth0DeviceAuthorization.h"
#include <HTTPClient.h>


MQTTLogin Auth0DeviceAuthorization::getMQTTLogin() {
    String baseURL = "https://dev-xk74bsamhi6vyn3g.ca.auth0.com";
    String clientId = "wakI3rj2wJbgMY9W1vKQAOi2jdtrGV6C";
    String audience = "emqx.projectfishworks.ca";

    String token = "";

    Serial.println("Requesting device code");

    HTTPClient http;
    http.begin(baseURL + "/oauth/device/code");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "client_id=" + String(clientId) + "&audience=" + String(audience);

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

        do{

            responseCode = http.POST(tokenPostData);

            if (responseCode > 0) {
                String tokenResponse = http.getString();
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
        token = http.getString();

    } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
    }

    http.end();

    String mqttClientId = "BaseStationClient-";
    mqttClientId += String(random(0xffff), HEX);

    return MQTTLogin(audience.c_str(), 8084, clientId.c_str(), "", token.c_str());

}

void Auth0DeviceAuthorization::requestDeviceCode(String baseURL, String clientId, String audience) {

    



}
