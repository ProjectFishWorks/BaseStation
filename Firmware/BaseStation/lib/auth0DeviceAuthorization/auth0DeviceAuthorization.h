#ifndef AUTH0DEVICEAUTHORIZATION_H
#define AUTH0DEVICEAUTHORIZATION_H

// Your code here

#include "deviceAuthorization.h"
#include "mqttLogin.h"
#include "ArduinoJson.h"

class Auth0DeviceAuthorization : public DeviceAuthorization {
public:
    Auth0DeviceAuthorization() = default;
    ~Auth0DeviceAuthorization() override = default;

    MQTTLogin getMQTTLogin(uint8_t forceNewLogin = false) override;

    //Code to run when the user is required to login on a external device
    std::function<void(const char* url, const char* code)> verificationCallback;


private:

    void requestDeviceCode(String baseURL, String clientId, String audience);

};

#endif // AUTH0DEVICEAUTHORIZATION_H