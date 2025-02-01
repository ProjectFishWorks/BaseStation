#ifndef AUTH0DEVICEAUTHORIZATION_H
#define AUTH0DEVICEAUTHORIZATION_H

// Your code here

#include "deviceAuthorization.h"
#include "mqttLogin.h"
#include "ArduinoJson.h"

#include "mbedtls/base64.h"

#define TOKEN_FILE "/mqtt-token.json"

class Auth0DeviceAuthorization : public DeviceAuthorization {
public:
    Auth0DeviceAuthorization() = default;
    ~Auth0DeviceAuthorization() override = default;

    MQTTLogin getMQTTLogin(uint8_t forceNewLogin = false) override;

    String getUserID();

    //Code to run when the user is required to login on a external device
    std::function<void(const char* url, const char* code)> verificationCallback;


private:

};

#endif // AUTH0DEVICEAUTHORIZATION_H