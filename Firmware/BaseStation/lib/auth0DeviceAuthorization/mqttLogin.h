#ifndef MQTTLOGIN_H
#define MQTTLOGIN_H

#include <Arduino.h>

class MQTTLogin {
public:

    MQTTLogin() = default;

    MQTTLogin(const char* server, uint16_t port, const char* clientId, const char* username, String password)
        : server(server), port(port), clientId(clientId), username(username), password(password) {}

    const char* getServer() { return server; }
    uint16_t getPort() { return port; }
    const char* getClientId() { return clientId; }
    const char* getUsername() { return username; }
    String getPassword() { return password; }

private:
    const char* server;
    uint16_t port;
    const char* clientId;
    const char* username;
    String password;
};

#endif // MQTTLOGIN_H