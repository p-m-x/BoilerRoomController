#include <DNSServer.h>
#define DEBUG_WIFI

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include "ESPAsyncWebServer.h"

#define DEFAULT_INPUT_FIELD_LENGTH 20
#define HA_MQTT_DISCO_TOPIC_PREFIX_LENGTH 30

const String CONFIG_FILENAME = "/config.cfg";

struct PortalConfig
{
  char wifiSsid[DEFAULT_INPUT_FIELD_LENGTH + 1];
  char wifiPassword[DEFAULT_INPUT_FIELD_LENGTH + 1];
  char mqttHost[DEFAULT_INPUT_FIELD_LENGTH + 1];
  char mqttUsername[DEFAULT_INPUT_FIELD_LENGTH + 1];
  char mqttPassword[DEFAULT_INPUT_FIELD_LENGTH + 1];
  char configHaMqttDiscoveryTopicPrefix[HA_MQTT_DISCO_TOPIC_PREFIX_LENGTH + 1];
};

class Portal
{
public:
  Portal(uint16_t port);

  void startInAccessPointMode(const char *SSID);
  void startInStationMode(const char *SSID, const char *pass);
  bool begin();
  void loop();
  bool isWiFiConnected();
  PortalConfig getConfig();
  void clearWiFiCredentials();

private:
  PortalConfig _config;
  DNSServer _dnsServer;
  AsyncWebServer _server;
  unsigned long _deviceRestartTime = 0;
  bool _needRestart = false;
  bool _isWiFiConnected = false;
  String _randomHash = "";
  String indexTemplateProcessor(String arg);
  void loadConfig();
  void saveConfig();
  void startHTTPServer();
  void stopHTTPServer();

  // ESP WiFI evvents
  WiFiEventHandler _wifiConnectedHandler, _wifiDiscnnectedHandler, _wifIGotIPHandler, _wifiStationConnectedHandler, _wifiStationDisonnectedHandler;
  void onWiFiConnected(const WiFiEventStationModeConnected &event);
  void onWiFiDisconnect(const WiFiEventStationModeDisconnected &event);
  void onWiFIGotIP(const WiFiEventStationModeGotIP &event);
  void onWiFiStationConnected(const WiFiEventSoftAPModeStationConnected &event);
  void onWiFiStationDisonnected(const WiFiEventSoftAPModeStationDisconnected &event);

  // Endpoints handlers
  void handleScanWifiEndpoint(AsyncWebServerRequest *request);
  void handleRestartEndpoint(AsyncWebServerRequest *request);
  void handleUpdateConfigEndpoint(AsyncWebServerRequest *request);
};