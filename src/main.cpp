#include <Arduino.h>
#include <Wire.h>              //for ESP8266 use bug free i2c driver https://github.com/enjoyneering/ESP8266-I2C-Driver
// #include <LiquidCrystal_I2C.h>
#include <PCF8574.h>
#include <portal.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <Relays.h>
#include <TempSensors.h>
#include <FlowRateSensors.h>
#include <DistanceSensor.h>
#include <MqttAdapter.h>

#define DEBUG
#define COLUMS 16
#define ROWS   2
#define PAGE   ((COLUMS) * (ROWS))
#define MQTT_BUFFER_SIZE 512
#define PCF_RELAYS_ADDR 0x27
#define ONE_WIRE_BUS D3

#define MQTT_INIT_INTERVAL 1000
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


bool mqttInitialized = false;
double distanceSensorLastValue = 0.0;
uint8_t mqttInitializationStep = MQTT_INIT_STEP_DISCOVERY;

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

void mqttAdapterSubscriptionCallback(Type type, const char* name, String value);

bool relayStateChangedHandler(uint8_t relayId, bool state);
void temperatureChangedHandler(const char* name, float value);
void waterFlowChangedHandler(const char* name, double value);
void distanceChangedHandler(const char* name, double value);

void scanI2C(void);

Portal portal(80);
PortalConfig config;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
DistanceSensor distanceSensor(D7, D8);
TempSensors tempSensors(ONE_WIRE_BUS);
Relays relays(PCF_RELAYS_ADDR);
FlowRateSensors flowRateSensors(D5, D6);
MqttAdapter mqttAdapter(mqtt, mqttAdapterSubscriptionCallback);

Ticker ledOneTimeBlinker(ledOneFlashHandler, 20, 2);
Ticker wifiConnectingBlinker(ledWiFiConnectingHandler, 100);
Ticker mqttInitializeTicker(mqttInitializeTickerHandler, MQTT_INIT_INTERVAL);

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

  if (!String(config.wifiSsid).isEmpty() && !String(config.wifiPassword).isEmpty()) {
    wifiConnectingBlinker.start();
    portal.startInStationMode(config.wifiSsid, config.wifiPassword);
  } else {
    portal.startInAccessPointMode(AP_MODE_NETWORK_NAME.c_str());
  }

  mqtt.setServer(config.mqttHost, 1883);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqttAdapter.setDiscoveryTopicPrefix(config.configHaMqttDiscoveryTopicPrefix);

  relays.setRelayStateChangedCallback(relayStateChangedHandler);
  tempSensors.setValueChangedCallback(temperatureChangedHandler);
  flowRateSensors.setValueChangedCallback(waterFlowChangedHandler);
  distanceSensor.setDistanceChangedCallback(distanceChangedHandler);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA End");
    Serial.println("Rebooting...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  mqttInitializeTicker.start();


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
  ArduinoOTA.handle();
  ledOneTimeBlinker.update();
  mqttInitializeTicker.update();
  if (mqttInitialized && mqtt.connected()) {
    flowRateSensors.update();
    tempSensors.update();
    distanceSensor.update();
  }


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

void mqttAdapterSubscriptionCallback(Type type, const char* name, String value)
{
  switch (type)
  {
    case Type::RELAY:
      relays.setState(atoi(name), value);
      break;
    case Type::HOMEASSISTANT:
      Serial.print("Home Assistant status: ");
      Serial.println(value);
      if (value.equals("online")) {
        mqttInitializationStart();
      }
      break;
  }
}

bool relayStateChangedHandler(uint8_t relayId, bool state)
{
  #ifdef DEBUG
  Serial.print(F("Sending relay state update #"));
  Serial.print(relayId);
  Serial.print(F(" v: "));
  Serial.println(state);
  #endif
  mqttAdapter.sendState(Type::RELAY, String(relayId).c_str(), state);
}

void temperatureChangedHandler(const char* name, float value)
{
  #ifdef DEBUG
  Serial.print(F("Sending temperature update #"));
  Serial.print(name);
  Serial.print(F(" v: "));
  Serial.println(value);
  #endif
  mqttAdapter.sendState(Type::TEMP_SENSOR, name, String(value).c_str());
}

void waterFlowChangedHandler(const char* name, double value)
{
  #ifdef DEBUG
  Serial.print(F("Sending water flow update #"));
  Serial.print(name);
  Serial.print(F(" v: "));
  Serial.println(value);
  #endif
  mqttAdapter.sendState(Type::WATER_FLOW_SENSOR, name, String(value).c_str());
}

void distanceChangedHandler(const char* name, double value)
{
  #ifdef DEBUG
  Serial.print(F("Sending distance update #"));
  Serial.print(name);
  Serial.print(F(" v: "));
  Serial.println(value);
  #endif
  mqttAdapter.sendState(Type::DISTANCE_SENSOR, name, String(value).c_str());
}

void sendDiscovery()
{
  std::vector<String> relaysNames = relays.getNames();
  for (uint8_t i =0; i < relaysNames.size(); i++) {
    mqttAdapter.sendDiscovery(Type::RELAY, relaysNames[i].c_str());
  }

  std::vector<String> tempSensorsNames = tempSensors.getNames();
  for (uint8_t i =0; i < tempSensorsNames.size(); i++) {
    mqttAdapter.sendDiscovery(Type::TEMP_SENSOR, tempSensorsNames[i].c_str());
  }

  std::vector<String> waterSensorsNames = flowRateSensors.getNames();
  for (uint8_t i =0; i < waterSensorsNames.size(); i++) {
    mqttAdapter.sendDiscovery(Type::WATER_FLOW_SENSOR, waterSensorsNames[i].c_str());
  }

  mqttAdapter.sendDiscovery(Type::DISTANCE_SENSOR, distanceSensor.getName().c_str());

}

void sendAvailability()
{
  mqttAdapter.sendAvailability();
}

void sendStates()
{
  std::vector<bool> relayStates = relays.getStates();
  for (uint8_t i = 0; i < relayStates.size(); i++) {
    mqttAdapter.sendState(Type::RELAY, String(i).c_str(), relayStates[i]);
  }

  std::vector<float> dsStates = tempSensors.getStates();
  for (uint8_t i = 0; i < dsStates.size(); i++) {
    if (dsStates[i] == NULL) {
      continue;
    }
    mqttAdapter.sendState(Type::TEMP_SENSOR, String(i).c_str(), String(dsStates[i]).c_str());
  }

  std::vector<double> waterSensorsStates = flowRateSensors.getStates();
  std::vector<String> waterSensorsNames = flowRateSensors.getNames();
  for (uint8_t i = 0; i < waterSensorsStates.size(); i++) {
    mqttAdapter.sendState(Type::WATER_FLOW_SENSOR, waterSensorsNames[i].c_str(), String(waterSensorsStates[i]).c_str());
  }

  mqttAdapter.sendState(Type::DISTANCE_SENSOR, distanceSensor.getName().c_str(), String(distanceSensor.getValue()).c_str());

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

  relays.begin();
  tempSensors.begin();
  flowRateSensors.begin();
  Serial.println(F("Initialize subscriptions"));
  mqttAdapter.subscribe(Type::HOMEASSISTANT, NULL);
  std::vector<String> relaysNames = relays.getNames();
  for (uint8_t i = 0; i < relaysNames.size(); i++) {
    mqttAdapter.subscribe(Type::RELAY, relaysNames[i].c_str());
  }
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
    Serial.println(F("Sending discovery message"));
    sendDiscovery();
    break;
  case MQTT_INIT_STEP_AVAILABILITY:
    Serial.println(F("Sending availability message"));
    sendAvailability();
    break;
  case MQTT_INIT_STEP_STATES:
    Serial.println(F("Sending states"));
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