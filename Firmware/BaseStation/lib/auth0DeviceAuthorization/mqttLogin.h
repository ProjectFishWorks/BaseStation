#ifndef MQTTLOGIN_H
#define MQTTLOGIN_H

#include <Arduino.h>

class MQTTLogin {
public:

    MQTTLogin() = default;

    MQTTLogin(const char* server, uint16_t port, const char* clientId, const char* username, const char* password)
        : server(server), port(port), clientId(clientId), username(username), password(password) {}

    const char* getServer() const { return server; }
    uint16_t getPort() const { return port; }
    const char* getClientId() const { return clientId; }
    const char* getUsername() const { return username; }
    const char* getPassword() const { return password; }

private:
    const char* server;
    uint16_t port;
    const char* clientId;
    const char* username;
    const char* password;
};

#endif // MQTTLOGIN_H