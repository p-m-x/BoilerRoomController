#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
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
#include <AceButton.h>
using namespace ace_button;

ADC_MODE(ADC_VCC);

#define DEBUG
#define LCD_COLUMS 16
#define LCD_ROWS   2
#define LCD_BACKLIGHT_TIME 5000
#define PAGE   ((LCD_COLUMS) * (LCD_ROWS))
#define MQTT_BUFFER_SIZE 512
#define PCF_RELAYS_ADDR 0x27
#define ONE_WIRE_BUS D3
#define WATER_FLOW_RATE_1 D7
#define WATER_FLOW_RATE_2 D8
#define DISTANCE_SENS_TRIGG D5
#define DISTANCE_SENS_ECHO D6
#define FUNC_BUTTON_PIN D0

#define MQTT_INIT_INTERVAL 1000
#define MQTT_INIT_STEP_DISCOVERY 1
#define MQTT_INIT_STEP_AVAILABILITY 2
#define MQTT_INIT_STEP_STATES 3
#define MQTT_INIT_END 4

const String AP_MODE_NETWORK_NAME = "BRC-WiFi";
const char* MQTT_CLIENT_NAME = "Controller-1";


LiquidCrystal_I2C lcd(PCF8574_ADDR_A20_A10_A00, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);


bool mqttInitialized = false;
double distanceSensorLastValue = 0.0;
uint8_t mqttInitializationStep = MQTT_INIT_STEP_DISCOVERY;
uint8_t lcdStatusPosition = 0;

void sendDiscovery();
void sendAvailability();
void sendUnavailability();
void sendStates();
void mqttInitializationStart();
void mqttInitializationStop();
void mqttInitializeTickerHandler();

void ledWiFiConnectingHandler();
void lcdBackLightHandler();
void distanceSensorUpdateHandler();
void onMqttConnected();

void mqttAdapterSubscriptionCallback(Type type, const char* name, String value);

bool relayStateChangedHandler(uint8_t relayId, bool state);
void temperatureChangedHandler(const char* name, float value);
void waterFlowChangedHandler(const char* name, double value);
void distanceChangedHandler(const char* name, double value);
void funcButtonHandler(AceButton* button, uint8_t eventType, uint8_t buttonState);

void lcdBacklight();
void lcdShowNext();
void scanI2C(void);

Portal portal(80);
PortalConfig config;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
DistanceSensor distanceSensor(DISTANCE_SENS_TRIGG, DISTANCE_SENS_ECHO);
TempSensors tempSensors(ONE_WIRE_BUS);
Relays relays(PCF_RELAYS_ADDR);
FlowRateSensors flowRateSensors(WATER_FLOW_RATE_1, WATER_FLOW_RATE_2);
MqttAdapter mqttAdapter(mqtt, mqttAdapterSubscriptionCallback);

Ticker wifiConnectingBlinker(ledWiFiConnectingHandler, 100);
Ticker mqttInitializeTicker(mqttInitializeTickerHandler, MQTT_INIT_INTERVAL);
Ticker lcdBacklightTicker(lcdBackLightHandler, 5000, 1);

AceButton funcButton(FUNC_BUTTON_PIN);

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(FUNC_BUTTON_PIN, INPUT_PULLUP);
  ButtonConfig* buttonConfig = funcButton.getButtonConfig();

  buttonConfig->setEventHandler(funcButtonHandler);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  // buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAll);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setLongPressDelay(5000);
  scanI2C();

  while (lcd.begin(LCD_COLUMS, LCD_ROWS) != 1) //colums - 20, rows - 4
  {
    Serial.println(F("PCF8574 is not connected or lcd pins declaration is wrong. Only pins numbers: 4,5,6,16,11,12,13,14 are legal."));
    delay(5000);
  }
  portal.begin();
  config = portal.getConfig();

  lcd.clear();
  if (!String(config.wifiSsid).isEmpty() && !String(config.wifiPassword).isEmpty()) {
    wifiConnectingBlinker.start();
    portal.startInStationMode(config.wifiSsid, config.wifiPassword);
    lcd.print(F("STATION"));
    lcd.setCursor(0, 1);
    lcd.print(F("CONNECTING..."));
  } else {
    portal.startInAccessPointMode(AP_MODE_NETWORK_NAME.c_str());
    lcd.print(F("ACCESS POINT"));
    lcd.setCursor(0, 1);
    lcd.print(WiFi.SSID());
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
    sendUnavailability();
    digitalWrite(LED_BUILTIN, LOW);
    lcd.clear();
    lcd.print(F("UPDATING FW..."));
    lcdBacklight();
  });
  ArduinoOTA.onEnd([]() {
    sendUnavailability();
    Serial.println("OTA End");
    Serial.println("Rebooting...");
    lcd.clear();
    lcd.print(F("REBOOTING..."));
    lcdBacklight();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    uint8_t percent = (progress / (total / 100));
    Serial.printf("Progress: %u%%\r\n", percent);
    lcd.setCursor(0, 1);
    lcd.print(percent);
    lcd.print(F("%"));
    lcdBacklight();
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
}


void loop()
{
  ArduinoOTA.handle();
  funcButton.check();
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
    lcd.clear();
    lcd.print(F("CONNECTED"));
    lcdBacklight();
    mqttInitializationStart();
  }


  // code below is executed only in STATION mode and WiFi connected
  mqtt.loop();
  mqttInitializeTicker.update();
  lcdBacklightTicker.update();
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
  mqttAdapter.sendAvailability(true);
}

void sendUnavailability()
{
  mqttAdapter.sendAvailability(false);
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
  lcd.clear();
  lcd.print(F("INITIALIZING..."));
  lcd.backlight();
  mqttInitialized = false;
  mqttInitializationStep = MQTT_INIT_STEP_DISCOVERY;
  mqttInitializeTicker.start();
}

void mqttInitializationStop()
{
  mqttInitializeTicker.stop();
  lcd.clear();
  lcd.print(F("READY"));
  lcdBacklight();
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


void funcButtonHandler(AceButton* button, uint8_t eventType, uint8_t buttonState)
{
  lcdBacklight();
  switch (eventType) {
    case AceButton::kEventClicked:
      lcdShowNext();
      break;
    case AceButton::kEventLongPressed:
      lcdBacklight();
      lcd.clear();
      lcd.print(F("RESET WiFi"));
      sendUnavailability();
      portal.clearWiFiCredentials();
      delay(2000);
      lcdBacklight();
      lcd.clear();
      lcd.print("REBOOTING...");
      ESP.restart();
      break;
  }
}

void lcdBackLightHandler()
{
  //don't turn off display uninitialized
  if (!mqttInitialized) {
    return;
  }
  lcd.displayOff();
}

void lcdBacklight()
{
  lcd.displayOn();
  lcdBacklightTicker.start();
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

void lcdShowNext()
{
  lcdBacklight();
  lcd.clear();
  uint8_t waterFlowStart = 3;
  uint8_t waterFlowEnd = waterFlowStart + 4;

  uint8_t relaysStart = waterFlowEnd;
  uint8_t relaysEnd = relaysStart + relays.getCount();

  uint8_t tempStart = relaysEnd;
  uint8_t tempEnd = tempStart + tempSensors.getCount();
  Serial.println(lcdStatusPosition);
  if (lcdStatusPosition == 0) {

    if (WiFi.getMode() == WIFI_STA) {
      lcd.print(F("IP"));
      lcd.setCursor(0, 1);
      lcd.print(WiFi.localIP().toString());
    } else {
      lcd.print(F("SSID"));
      lcd.setCursor(0, 1);
      lcd.print(WiFi.SSID());
    }
  } else if (lcdStatusPosition == 1) {
    lcd.print(F("VCC"));
    lcd.setCursor(0, 1);
    char buff[4];
    lcd.printf("%0.2fV", ESP.getVcc() / 1000);
  } else if (lcdStatusPosition == 2) {
    lcd.print("DISTANCE SENSOR");
    lcd.setCursor(0, 1);
    lcd.print(distanceSensor.getValue());
  } else if (lcdStatusPosition >= waterFlowStart && lcdStatusPosition < waterFlowEnd) {
    std::vector<double> states = flowRateSensors.getStates();
    std::vector<String> names = flowRateSensors.getNames();
    uint8_t index = lcdStatusPosition - waterFlowStart;
    String name = names[index];
    name.replace(F("-"), F(" "));
    name.toUpperCase();
    lcd.print(name);
    lcd.setCursor(0, 1);
    lcd.printf("%0.2f", states[index]);
  } else if (lcdStatusPosition >= relaysStart && lcdStatusPosition < relaysEnd) {
    uint8_t relayId = lcdStatusPosition - relaysStart;
    lcd.print(F("RELAY #"));
    lcd.print(relayId);
    lcd.setCursor(0, 1);
    lcd.print(relays.getState(relayId) ? F("ON") : F("OFF"));
  } else if (lcdStatusPosition >= tempStart && tempStart == tempEnd) {
    lcd.print(F("NO TEMP SENSORS"));
  } else if (lcdStatusPosition >= tempStart && lcdStatusPosition < tempEnd) {
    uint8_t tempId = lcdStatusPosition - tempStart;
    std::vector<float> states = tempSensors.getStates();
    lcd.print(F("TEMPERATURE #"));
    lcd.print(tempId);
    lcd.setCursor(0, 1);
    lcd.printf("%0.2f", states[tempId]);
    lcd.print((char)223);
    lcd.print(F("C"));
  } else {
    lcdStatusPosition = 0;
    lcdShowNext();
    return;
  }
  lcdStatusPosition++;
}