#include <Arduino.h>
#include <Wire.h>              //for ESP8266 use bug free i2c driver https://github.com/enjoyneering/ESP8266-I2C-Driver
// #include <LiquidCrystal_I2C.h>
#include <PCF8574.h>
#include <portal.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <HCSR04.h>
#include <Relays.h>
#include <TempSensors.h>
#include <FlowRateSensors.h>
#include <MqttPublisher.h>

#define COLUMS 16
#define ROWS   2
#define PAGE   ((COLUMS) * (ROWS))
#define MQTT_BUFFER_SIZE 512
#define PCF_RELAYS_ADDR 0x27
#define ONE_WIRE_BUS D3

#define MQTT_INIT_INTERVAL 1000
#define MQTT_AVAILABLE_INTERVAL 60000
#define MQTT_UPDATE_STATES_INTERVAL 15000
#define DISTANCE_SENSOR_UPDATE_INTERVAL 1000 //3600000 // 1h
#define MQTT_INIT_STEP_DISCOVERY 1
#define MQTT_INIT_STEP_AVAILABILITY 2
#define MQTT_INIT_STEP_STATES 3
#define MQTT_INIT_END 4

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
TempSensors tempSensors(mqtt, ONE_WIRE_BUS);

bool relayStateHandler(uint8_t relayId, bool state);
Relays relays(relayStateHandler, mqtt, PCF_RELAYS_ADDR);

FlowRateSensors flowRateSensors(mqtt, D5, D6);
MqttPublisher mqttPublisher(mqtt);

bool isWiFiConnected = false;
bool relaysStateSent = false;
bool mqttInitialized = false;
double distanceSensorLastValue = 0.0;
uint8_t mqttInitializationStep = MQTT_INIT_STEP_DISCOVERY;
String haAvailabilityTopic = "/status";

void mqttCallback(char* topic, uint8_t* payload, unsigned int length);

void sendDiscovery();
void sendAvailability();
void sendStates();
void mqttInitializationStart();
void mqttInitializationStop();
void mqttInitializeTickerHandler();

void ledOneFlashHandler();
void ledWiFiConnectingHandler();
void distanceSensorUpdateHandler();
void onMqttConnected();

void mqttUpdateStates();

void scanI2C(void);

Ticker ledOneTimeBlinker(ledOneFlashHandler, 20, 2);
Ticker wifiConnectingBlinker(ledWiFiConnectingHandler, 100);
Ticker mqttInitializeTicker(mqttInitializeTickerHandler, MQTT_INIT_INTERVAL);
Ticker mqttUpdateStatesTicker(mqttUpdateStates, MQTT_UPDATE_STATES_INTERVAL);
Ticker distanceSensorTicker(distanceSensorUpdateHandler, DISTANCE_SENSOR_UPDATE_INTERVAL);

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  // scanI2C();

  // while (lcd.begin(COLUMS, ROWS) != 1) //colums - 20, rows - 4
  // {
  //   Serial.println(F("PCF8574 is not connected or lcd pins declaration is wrong. Only pins numbers: 4,5,6,16,11,12,13,14 are legal."));
  //   delay(5000);
  // }
  portal.begin();
  config = portal.getConfig();
  mqttPublisher.setDiscoveryTopicPrefix(config.configHaMqttDiscoveryTopicPrefix);


  haAvailabilityTopic = config.configHaMqttDiscoveryTopicPrefix + haAvailabilityTopic;

  mqtt.setServer(config.mqttHost, 1883);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setCallback(mqttCallback);

  if (!String(config.wifiSsid).isEmpty() && !String(config.wifiPassword).isEmpty()) {
    wifiConnectingBlinker.start();
    portal.startInStationMode(config.wifiSsid, config.wifiPassword);
  } else {
    portal.startInAccessPointMode(AP_MODE_NETWORK_NAME.c_str());
  }
  mqttInitializeTicker.start();
  mqttUpdateStatesTicker.start();
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
  flowRateSensors.update();
  tempSensors.update();
  ledOneTimeBlinker.update();
  mqttInitializeTicker.update();
  mqttUpdateStatesTicker.update();
  distanceSensorTicker.update();

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

bool relayStateHandler(uint8_t relayId, bool state)
{
  mqttPublisher.sendState(Type::RELAY, String(relayId).c_str(), state);
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int length)
{
  String p = "";
  for (uint8_t i = 0; i < length; i++) {
    p+= String(char(payload[i]));
  }

  if (haAvailabilityTopic.equalsIgnoreCase(topic)) {
    Serial.print(F("HA status: "));
    Serial.println(p);
    if (p.equalsIgnoreCase("online")) {
      mqttInitializationStart();
    }
  } else {
    // relays.handleMqtt(topic, payload, length);
  }
}

void sendDiscovery()
{
  Serial.println(F("Sending discovery message"));

  std::vector<String> relaysNames = relays.getNames();
  for (uint8_t i =0; i < relaysNames.size(); i++) {
    mqttPublisher.sendDiscovery(Type::RELAY, relaysNames[i].c_str());
  }

  std::vector<String> tempSensorsNames = tempSensors.getNames();
  for (uint8_t i =0; i < tempSensorsNames.size(); i++) {
    mqttPublisher.sendDiscovery(Type::TEMP_SENSOR, tempSensorsNames[i].c_str());
  }

  std::vector<String> waterSensorsNames = flowRateSensors.getNames();
  for (uint8_t i =0; i < waterSensorsNames.size(); i++) {
    mqttPublisher.sendDiscovery(Type::WATER_FLOW_SENSOR, waterSensorsNames[i].c_str());
  }
}

void sendAvailability()
{
  Serial.println(F("Sending availability message"));
  mqttPublisher.sendAvailability();
}

void sendStates()
{
  Serial.println(F("Sending initial states"));

  std::vector<bool> relayStates = relays.getStates();
  for (uint8_t i = 0; i < relayStates.size(); i++) {
    mqttPublisher.sendState(Type::RELAY, String(i).c_str(), relayStates[i]);
  }

  std::vector<float> dsStates = tempSensors.getStates();
  for (uint8_t i = 0; i < dsStates.size(); i++) {
    mqttPublisher.sendState(Type::TEMP_SENSOR, String(i).c_str(), String(dsStates[i]).c_str());
  }

  std::vector<double> waterSensorsStates = flowRateSensors.getStates();
  std::vector<String> waterSensorsNames = flowRateSensors.getNames();
  for (uint8_t i = 0; i < waterSensorsStates.size(); i++) {
    mqttPublisher.sendState(Type::WATER_FLOW_SENSOR, waterSensorsNames[i].c_str(), String(waterSensorsStates[i]).c_str());
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
  distanceSensorTicker.start();

  relays.begin();
  tempSensors.begin();
  flowRateSensors.begin();
  Serial.println(F("Initialize subscriptions"));
  // relays.initSubscriptions();
  Serial.print(F("Register subscription on "));
  Serial.println(haAvailabilityTopic);
  mqtt.subscribe(haAvailabilityTopic.c_str());
}

void mqttUpdateStates()
{
  // tempSensors.sendState();
}



void mqttInitializationStart()
{
  mqttInitialized = false;
  mqttInitializationStep = MQTT_INIT_STEP_DISCOVERY;
  mqttInitializeTicker.start();
}

void mqttInitializationStop()
{
  mqttInitializeTicker.stop();
}

void mqttInitializeTickerHandler()
{
  if (!mqtt.connected()) {
    return;
  }

  if (mqttInitialized) {
    mqttInitializationStop();
    return;
  }

  switch (mqttInitializationStep)
  {
  case MQTT_INIT_STEP_DISCOVERY:
    sendDiscovery();
    break;
  case MQTT_INIT_STEP_AVAILABILITY:
    sendAvailability();
    break;
  case MQTT_INIT_STEP_STATES:
    sendStates();
    break;
  case MQTT_INIT_END:
    Serial.println(F("Initialization finished"));
    mqttInitialized = true;
    break;
  default:
    break;
  }
  mqttInitializationStep++;
}

void distanceSensorUpdateHandler()
{
  distanceSensorLastValue = distanceSensor.measureDistanceCm();
  Serial.println(distanceSensorLastValue);
}

// void scanI2C(void)
// {
//   byte error, address;
//   uint8_t nDevices = 0;

//   Serial.println(F("Scanning i2c bus..."));
//   for(uint8_t address = 1; address < 255; address++) {
//     Wire.beginTransmission(address);
//     if (Wire.endTransmission() == 0) {
//       Serial.print(F("i2c device found at address 0x"));
//       if (address<16) Serial.print("0");
//       Serial.println(address,HEX);
//       nDevices++;
//     }
//   }
//   if (nDevices == 0) {
//     Serial.println("Devices not found");
//   }
// }