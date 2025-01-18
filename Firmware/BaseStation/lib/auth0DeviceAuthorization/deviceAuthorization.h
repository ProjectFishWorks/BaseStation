#ifndef DEVICE_AUTHORIZATION_H
#define DEVICE_AUTHORIZATION_H

#include "mqttLogin.h"

class DeviceAuthorization {
public:
    DeviceAuthorization() = default;
    virtual ~DeviceAuthorization() = default;

    virtual MQTTLogin getMQTTLogin() = 0;
};

#endif // DEVICE_AUTHORIZATION_H