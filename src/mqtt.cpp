#include <mqtt.h>

MqttAdapter::MqttAdapter(WiFiClient wifiClient, const char* clientId)
{
    _client.setClient(wifiClient);
    _clientId = new char[strlen(clientId) + 1];
    strncpy(_clientId, clientId, strlen(clientId) + 1);
}

bool MqttAdapter::connect(const char* host, uint16_t port, const char* username, const char* password)
{
    if (_client.connected()) {
        return true;
    }

    strcpy(_mqttUsername, username);
    strcpy(_mqttPassword, password);

    _client.setServer(host, port);
    if (strlen(username) == 0) {
        username = NULL;
    }
    if (strlen(password) == 0) {
        password = NULL;
    }
    return _client.connect(_clientId, _mqttUsername, _mqttPassword);
}

bool MqttAdapter::isConnected()
{
    return _client.connected();
}

bool MqttAdapter::reconnect()
{
    if (_client.connected()) {
        return true;
    }
    return _client.connect(_clientId, _mqttUsername, _mqttPassword);
}

void MqttAdapter::loop()
{
    if (!_client.connected()) {
        reconnect();
    }
    _client.loop();
}