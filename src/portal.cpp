#include "portal.h"

Portal::Portal(uint16_t port)
  : _server(port)
{
}

bool Portal::begin()
{
  WiFi.persistent(false);
  WiFi.setOutputPower(20.5f);
  for (uint8_t i = 0; i < 15; i++) {
    _randomHash += String(random(0, 9));
  }

  Serial.println("Mounting FS...");
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return false;
  }

  loadConfig();
  return true;
}

void Portal::startInAccessPointMode(const char* SSID)
{
  Serial.println(F("Run in AP mode"));
  WiFi.mode(WIFI_AP);
  _wifiStationConnectedHandler = WiFi.onSoftAPModeStationConnected(std::bind(&Portal::onWiFiStationConnected, this, std::placeholders::_1));
  _wifiStationDisonnectedHandler = WiFi.onSoftAPModeStationDisconnected(std::bind(&Portal::onWiFiStationDisonnected, this, std::placeholders::_1));

  Serial.println(F("Starting AP..."));
  WiFi.softAP(SSID);
  _dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.print(F("SSID: "));
  Serial.println(WiFi.softAPSSID());
  Serial.print(F("IP: "));
  Serial.println(WiFi.softAPIP());

  Serial.print(F("Searching WiFi..."));
  uint8_t netCount = WiFi.scanNetworks(false);
  if (netCount > 0) {
    Serial.printf(" found %d networks\n", netCount);
    for (uint8_t i = 0; i < netCount; i++) {
      Serial.printf("%d# %s %ddB\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
  } else {
    Serial.println(F(" network not found!"));
  }
  startHTTPServer();
}

void Portal::startInStationMode(const char* SSID, const char* pass)
{
  Serial.println("Run in STATION mode");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  _wifiStationConnectedHandler = WiFi.onStationModeConnected(std::bind(&Portal::onWiFiConnected, this, std::placeholders::_1));
  _wifIGotIPHandler = WiFi.onStationModeGotIP(std::bind(&Portal::onWiFIGotIP, this, std::placeholders::_1));
  _wifiDiscnnectedHandler = WiFi.onStationModeDisconnected(std::bind(&Portal::onWiFiDisconnect, this, std::placeholders::_1));
  Serial.printf("Connecting to %s, with password %s\n", SSID, pass);
  WiFi.begin(SSID, pass);
  startHTTPServer();
}

void Portal::startHTTPServer()
{
  _server.serveStatic("/css", SPIFFS, "/www/css/");
  _server.serveStatic("/", SPIFFS, "/www/")
    .setTemplateProcessor(std::bind(&Portal::indexTemplateProcessor, this, std::placeholders::_1))
    .setDefaultFile("index.html");

  _server.serveStatic("/restart", SPIFFS, "/www/")
    .setDefaultFile("restart.html");

  _server.serveStatic("/config", SPIFFS, "/")
    .setDefaultFile("config.cfg");

  _server.on("/wifiNetworks", HTTP_GET, std::bind(&Portal::handleScanWifiEndpoint, this, std::placeholders::_1));
  _server.on("/restart", HTTP_POST, std::bind(&Portal::handleRestartEndpoint, this, std::placeholders::_1));
  _server.on("/update", HTTP_POST, std::bind(&Portal::handleUpdateConfigEndpoint, this, std::placeholders::_1));

  _server.begin();
  Serial.println("HTTP server started");
}

void Portal::stopHTTPServer()
{
  _server.end();
}

void Portal::onWiFiConnected(const WiFiEventStationModeConnected& event)
{
	Serial.println(F("Connected to AP"));
}

void Portal::onWiFIGotIP(const WiFiEventStationModeGotIP& event)
{
  _isWiFiConnected = true;
	Serial.print(F("IP: "));
	Serial.println(WiFi.localIP().toString());
}

void Portal::onWiFiDisconnect(const WiFiEventStationModeDisconnected& event)
{
  _isWiFiConnected = false;
	Serial.println(F("WiFi disconnected. Reconnecting..."));
}

void Portal::onWiFiStationConnected(const WiFiEventSoftAPModeStationConnected& event)
{
  Serial.print(F("Station connected "));
  Serial.print(F(" MAC: "));
  for (uint8_t i = 0; i < sizeof(event.mac); i++) {
    Serial.print(event.mac[i], HEX);
    if (i + 1 < sizeof(event.mac)) {
      Serial.print(F(":"));
    }
  }
  Serial.print(F(" AID: "));
  Serial.println(event.aid);
}

void Portal::onWiFiStationDisonnected(const WiFiEventSoftAPModeStationDisconnected& event)
{
  Serial.print(F("Station disconnected "));
  Serial.print(F(" MAC: "));
  for (uint8_t i = 0; i < sizeof(event.mac); i++) {
    Serial.print(event.mac[i], HEX);
    if (i + 1 < sizeof(event.mac)) {
      Serial.print(F(":"));
    }
  }
  Serial.print(F(" AID: "));
  Serial.println(event.aid);
}


void Portal::handleScanWifiEndpoint(AsyncWebServerRequest *request)
{
  String json = "[";
  int n = WiFi.scanComplete();
  if(n == -2){
    WiFi.scanNetworks(false, true);
  }
  if(n){
    for (int i = 0; i < n; ++i){
      if(i) json += ",";
      json += "{";
      json += "\"rssi\":"+String(WiFi.RSSI(i));
      json += ",\"ssid\":\""+WiFi.SSID(i)+"\"";
      json += ",\"mac\":\""+WiFi.BSSIDstr(i)+"\"";
      json += ",\"channel\":"+String(WiFi.channel(i));
      json += "}";
    }
    WiFi.scanDelete();
    if(WiFi.scanComplete() == -2){
      WiFi.scanNetworks(true, true);
    }
  }
  json += "]";
  request->send(200, "application/json", json);
  json = String();
}


void Portal::handleRestartEndpoint(AsyncWebServerRequest *request)
{
  Serial.print(F("Got restart request, hash: "));
  Serial.println(request->getParam("hash")->value());
  if (!_randomHash.isEmpty() && _randomHash.equals(request->getParam("hash")->value())) {
    Serial.println(F("Device is restarting..."));
    request->send(200);
    _deviceRestartTime = millis() + 1000;
    stopHTTPServer();
    return;
  }
  Serial.println(F("Incorrect security hash"));
  request->send(400);
}

void Portal::handleUpdateConfigEndpoint(AsyncWebServerRequest *request)
{
  if (!request->arg("wifi-ssid").equals(_config.wifiSsid) ||
      !request->arg("wifi-password").equals(_config.wifiPassword)) {
    _needRestart = true;
  }
  if (!request->arg("mqtt-host").equalsIgnoreCase(_config.mqttHost) ||
      !request->arg("mqtt-username").equals(_config.mqttUsername) ||
      !request->arg("mqtt-password").equals(_config.mqttPassword)) {
    _needRestart = true;
  }

  strcpy(_config.wifiSsid, request->arg("wifi-ssid").c_str());
  strcpy(_config.wifiPassword, request->arg("wifi-password").c_str());
  strcpy(_config.mqttHost, request->arg("mqtt-host").c_str());
  strcpy(_config.mqttUsername, request->arg("mqtt-username").c_str());
  strcpy(_config.mqttPassword, request->arg("mqtt-password").c_str());
  strcpy(_config.configHaMqttDiscoveryTopicPrefix, request->arg("config-ha--mqtt-disco-topic-prefix").c_str());

  File file = SPIFFS.open(CONFIG_FILENAME, "w");
  file.write((char *)&_config, sizeof(_config));
  file.close();

  request->redirect("/");
}

String Portal::indexTemplateProcessor(String arg) {
  if (arg == "RANDOM-HASH") return _randomHash;
  if (arg == "NEED-RESTART") return String(_needRestart);
  if (arg == "WIFI-SSID") return _config.wifiSsid;
  if (arg == "WIFI-PASSWORD") return _config.wifiPassword;
  if (arg == "MQTT-HOST") return _config.mqttHost;
  if (arg == "MQTT-USERNAME") return _config.mqttUsername;
  if (arg == "MQTT-PASSWORD") return _config.mqttPassword;
  if (arg == "CONFIG-HA-MQTT-DISCO-TOPIC") return _config.configHaMqttDiscoveryTopicPrefix;
  if (arg == "DEFAULT-INPUT-FIELD-LENGTH") return String(DEFAULT_INPUT_FIELD_LENGTH);
  if (arg == "HA-MQTT-DISCO-TOPIC-LENGTH") return String(HA_MQTT_DISCO_TOPIC_PREFIX_LENGTH);
  return "";
}

void Portal::loadConfig()
{
  Serial.println(F("Reading configuration"));

  if (!SPIFFS.exists(CONFIG_FILENAME)) {
    Serial.println(F("Config file not found, empty loaded"));
    return;
  }
  File file = SPIFFS.open(CONFIG_FILENAME, "r");
  delay(10);
  size_t bytesRead = file.readBytes((char *) &_config, sizeof(_config));
  file.close();

  if (bytesRead != sizeof(_config)) {
    Serial.println(F("Error during reading config file"));
    return;
  }
  Serial.println(F("Configuration loaded"));
}

PortalConfig Portal::getConfig()
{
  return _config;
}

bool Portal::isWiFiConnected()
{
  return _isWiFiConnected;
}

void Portal::loop() {
  _dnsServer.processNextRequest();
  if (_deviceRestartTime > 0 && millis() > _deviceRestartTime) {
    WiFi.disconnect();
    ESP.restart();
  }
}