#include <ESP8266WiFi.h>
#include <PubSubClient.h>

class MqttAdapter {
    public:
      MqttAdapter(WiFiClient wifiClient, const char* clientId);
      bool connect(const char* host, uint16_t port, const char* username, const char* password);
      bool isConnected();
      void loop();
    private:
        PubSubClient _client;
        char* _clientId;
        char* _mqttUsername;
        char* _mqttPassword;
        bool reconnect();
};