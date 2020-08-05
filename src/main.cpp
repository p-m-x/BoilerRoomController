#include <Arduino.h>
#include <Wire.h>              //for ESP8266 use bug free i2c driver https://github.com/enjoyneering/ESP8266-I2C-Driver
// #include <LiquidCrystal_I2C.h>
#include <PCF8574.h>
#include <portal.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <Relays.h>
#include <HCSR04.h>
#include <OneWire.h>
#include <DallasTemperature.h>


#define COLUMS 16
#define ROWS   2
#define PAGE   ((COLUMS) * (ROWS))
#define MQTT_BUFFER_SIZE 512
#define ONE_WIRE_BUS D2
#define PCF_RELAYS_ADDR 0x27

#define MQTT_UPDATE_INIT_INTERVAL 1000
#define MQTT_UPDATE_INTERVAL 60000
#define MQTT_INIT_STEP_DISCOVERY 1
#define MQTT_INIT_STEP_SUBSCRIPTIONS 2
#define MQTT_INIT_STEP_AVAILABILITY 3
#define MQTT_INIT_STEP_STATES 4

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
UltraSonicDistanceSensor distanceSensor(D7, D8);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);
Relays relays(mqtt, PCF_RELAYS_ADDR);

bool isWiFiConnected = false;
bool relaysStateSent = false;
bool mqttInitialized = false;
uint8_t mqttInitializationStep = MQTT_INIT_STEP_DISCOVERY;

void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
void ledOneFlashHandler();
void ledWiFiConnectingHandler();
void onMqttConnected();
void mqttUpdate();

void scanI2C(void);

Ticker ledOneTimeBlinker(ledOneFlashHandler, 20, 2);
Ticker wifiConnectingBlinker(ledWiFiConnectingHandler, 100);
Ticker mqttUpdaterTicker(mqttUpdate, MQTT_UPDATE_INIT_INTERVAL);

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

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

  if (!String(config.wifiSsid).isEmpty() && !String(config.wifiPassword).isEmpty()) {
    wifiConnectingBlinker.start();
    portal.startInStationMode(config.wifiSsid, config.wifiPassword);
  } else {
    portal.startInAccessPointMode(AP_MODE_NETWORK_NAME.c_str());
  }
  mqttUpdaterTicker.start();
}


void loop()
{
  portal.loop();
  wifiConnectingBlinker.update();
  mqttUpdaterTicker.update();

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
        onMqttConnected();
      } else {
        Serial.print(F("Failed to connect, error: "));
        Serial.println(mqtt.state());
      }
      delay(5000);
    }
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

void onMqttConnected()
{
  Serial.println(F("Connected to MQTT"));
  while(!mqtt.connected()) {}
  relays.setHomeassistantDiscoveryTopicPrefix(config.configHaMqttDiscoveryTopicPrefix);
  relays.begin();
}

void mqttUpdate()
{
  if (!mqtt.connected()) {
    return;
  }

  // steps triggered only once initialization
  if (!mqttInitialized) {
    switch (mqttInitializationStep)
    {
    case MQTT_INIT_STEP_DISCOVERY:
      relays.sendHomeassistantDiscovery();
      break;
    case MQTT_INIT_STEP_AVAILABILITY:
      relays.sendAvailabilityMessage();
      break;
    case MQTT_INIT_STEP_SUBSCRIPTIONS:
      relays.initSubscriptions();
      break;
    case MQTT_INIT_STEP_STATES:
      relays.sendState();
      mqttInitialized = true;
      mqttInitializationStep = MQTT_INIT_STEP_DISCOVERY;
      mqttUpdaterTicker.interval(MQTT_UPDATE_INTERVAL);
      break;
    default:
      break;
    }
    mqttInitializationStep++;
  }
  // steps fired every MQTT_UPDATE_INTERVAL
  else {
    relays.sendAvailabilityMessage();
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