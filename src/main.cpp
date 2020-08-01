#define PCF8574_DEBUG
#include <Arduino.h>
#include <Wire.h>              //for ESP8266 use bug free i2c driver https://github.com/enjoyneering/ESP8266-I2C-Driver
// #include <LiquidCrystal_I2C.h>
#include <PCF8574.h>
#include <portal.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <relays.h>

#define COLUMS 16
#define ROWS   2
#define PAGE   ((COLUMS) * (ROWS))
#define MQTT_BUFFER_SIZE 512
#define MQTT_PUBLISH_QOS 2;

#define SW_VERSION "0.0.1"
#define DEVICE_SN "PREGO00001"
#define RELAYS_DEVICE_NAME "BRC-RELAY"
#define RELAYS_DS_TEMP_DEVICE_NAME "BRC-T-SENSOR"
#define HA_DISCO_DEVICE_INFO_JSON_TPL "{\"name\":\"%s\",\"sw_version\":\"" SW_VERSION "\",\"identifiers\":[\"" DEVICE_SN "\"]}"
#define HA_DISCO_RELAY_PAYLOAD_TPL "{\"name\":\"boiler-room-relay-%d\",\"unique_id\":\""DEVICE_SN"-relay-%d\",\"stat_t\":\"%s\",\"cmd_t\":\"%s\",\"pl_off\":\"off\",\"pl_on\":\"on\",\"dev\":%s}"
#define HA_DISCO_RELAY_TOPIC_TPL "%s/switch/boilerRoomRelay-%d/config"

static const char* MQTT_RELAY_TOPIC_PREFIX = "home/boiler-room/relay";
static const char* MQTT_RELAY_TOPIC_STATE_POSTFIX = "state";
static const char* MQTT_RELAY_TOPIC_COMMAND_POSTFIX = "set";

byte loaderChars[5][8] = {
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
};

const String AP_MODE_NETWORK_NAME = "BRC-WiFi";
const char* MQTT_CLIENT_NAME = "Controller-1";


// LiquidCrystal_I2C lcd(PCF8574_ADDR_A20_A10_A00, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);

Portal portal(80);
PortalConfig config;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

bool isWiFiConnected = false;
bool relaysStateSent = false;


PCF8574 pcfRelays(0x27);

void relayStateHandler(uint8_t relayId, bool state);
Relays relays(relayStateHandler, 0x27);


void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
void ledOneFlashHandler();
void ledWiFiConnectingHandler();
void sendHomeAssistantDiscoveryMsg();
void mqttSendRelaysState();
void mqttSendRelayState(uint8_t relayId, bool state);
String getRelayMqttCommandTopic(uint8_t relayId);
String getRelayMqttStateTopic(uint8_t relayId);


void mqttInitSubscriptions();
void scanI2C(void);

Ticker ledOneTimeBlinker(ledOneFlashHandler, 20, 2);
Ticker wifiConnectingBlinker(ledWiFiConnectingHandler, 100);

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  relays.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);


Serial.println(relays.getCount());
  scanI2C();

  // while (lcd.begin(COLUMS, ROWS) != 1) //colums - 20, rows - 4
  // {
  //   Serial.println(F("PCF8574 is not connected or lcd pins declaration is wrong. Only pins numbers: 4,5,6,16,11,12,13,14 are legal."));
  //   delay(5000);
  // }
  portal.begin();
  config = portal.getConfig();
  mqtt.setServer(config.mqttHost, 1883);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setCallback(mqttCallback);

  if (!String(config.wifiSsid).isEmpty() && !String(config.wifiPassword).isEmpty()) {
    wifiConnectingBlinker.start();
    portal.startInStationMode(config.wifiSsid, config.wifiPassword);
  } else {
    portal.startInAccessPointMode(AP_MODE_NETWORK_NAME.c_str());
  }
}


void loop()
{
  portal.loop();
  wifiConnectingBlinker.update();

  if (WiFi.getMode() != WIFI_STA) {
    return;
  }

  if (!WiFi.isConnected()) {
    if (wifiConnectingBlinker.state() == STOPPED) {
      wifiConnectingBlinker.start();
    }
    return;
  } else if (wifiConnectingBlinker.state() == RUNNING) {
    wifiConnectingBlinker.stop();
    // make sure to turn off status led on WiFi connected
    digitalWrite(LED_BUILTIN, HIGH);
  }


  // code below is executed only in STATION mode and WiFi connected

  mqtt.loop();
  ledOneTimeBlinker.update();


  if (!String(config.mqttHost).isEmpty()) {
    if (!mqtt.connected()) {
      if (mqtt.state() == MQTT_CONNECTION_LOST) {
        Serial.print(F("Reconnecting to MQTT "));
      } else {
        Serial.print(F("Connecting to MQTT "));
      }
      Serial.println(config.mqttHost);
      if (mqtt.connect(MQTT_CLIENT_NAME, config.mqttUsername, config.mqttPassword)) {
        Serial.print(F("Connected to MQTT as "));
        Serial.println("Controller-1");
        sendHomeAssistantDiscoveryMsg();
        mqttInitSubscriptions();
        mqttSendRelaysState();
      } else {
        Serial.println(F("Failed to connect"));
      }
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int length)
{

  ledOneTimeBlinker.start();
  String t(topic);
  if (t.startsWith(MQTT_RELAY_TOPIC_PREFIX)) {
    uint8_t startIndex = String(MQTT_RELAY_TOPIC_PREFIX).length() + 1;
    uint8_t endIndex = t.indexOf("/", startIndex + 1);
    uint8_t relayId = atoi(t.substring(startIndex, endIndex).c_str());

    String p = "";
    for (uint8_t i = 0; i < length; i++) {
      p += String((char)payload[i]);
    }
    relays.setRelay(relayId, p.equals("on"));

  }
}

void ledOneFlashHandler()
{
  digitalWrite(LED_BUILTIN, ledOneTimeBlinker.counter() - 1);
}

void ledWiFiConnectingHandler()
{
  digitalWrite(LED_BUILTIN, wifiConnectingBlinker.counter() % 2 > 0);
}

void sendHomeAssistantDiscoveryMsg()
{

  if (mqtt.connected() && strlen(config.configHaMqttDiscoveryTopicPrefix) > 0) {
    // relays
    char deviceInfo[100];
    sprintf(deviceInfo, HA_DISCO_DEVICE_INFO_JSON_TPL, RELAYS_DEVICE_NAME);

    for (uint8_t i = 0; i < relays.getCount(); i++) {

      char discoTopic[70];
      sprintf(discoTopic, HA_DISCO_RELAY_TOPIC_TPL, config.configHaMqttDiscoveryTopicPrefix, i);

      char payloadBuff[300];
      sprintf(payloadBuff, HA_DISCO_RELAY_PAYLOAD_TPL, i, i, getRelayMqttStateTopic(i).c_str(), getRelayMqttCommandTopic(i).c_str(), deviceInfo);
      mqtt.publish(discoTopic, payloadBuff);
    }
    Serial.println("HA discovery sent");
  }
}

void mqttInitSubscriptions()
{
  for (uint8_t i = 0; i < relays.getCount(); i++) {
    mqtt.unsubscribe(getRelayMqttCommandTopic(i).c_str());
    mqtt.subscribe(getRelayMqttCommandTopic(i).c_str());
    Serial.print(F("Subscribed to "));
    Serial.println(getRelayMqttCommandTopic(i).c_str());
  }
}

String getRelayMqttCommandTopic(uint8_t relayId)
{
  return String(MQTT_RELAY_TOPIC_PREFIX) + "/" + String(relayId) + "/" + String(MQTT_RELAY_TOPIC_COMMAND_POSTFIX);
}

String getRelayMqttStateTopic(uint8_t relayId)
{
  return String(MQTT_RELAY_TOPIC_PREFIX) + "/" + String(relayId) + "/" + String(MQTT_RELAY_TOPIC_STATE_POSTFIX);
}

void relayStateHandler(uint8_t relayId, bool state)
{
  mqttSendRelayState(relayId, state);
}

void mqttSendRelayState(uint8_t relayId, bool state)
{
  mqtt.publish(getRelayMqttStateTopic(relayId).c_str(), state ? "on" : "off");
}

void mqttSendRelaysState()
{
  for (uint8_t i = 0; i < relays.getCount(); i++) {
    mqttSendRelayState(i, relays.getState(i));
  }
}

void scanI2C(void)
{
  byte error, address;
  uint8_t nDevices = 0;

  Serial.println(F("Scanning i2c bus..."));
  for(uint8_t address = 1; address < 255; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("i2c device found at address 0x"));
      if (address<16) Serial.print("0");
      Serial.println(address,HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("Devices not found");
  }
}