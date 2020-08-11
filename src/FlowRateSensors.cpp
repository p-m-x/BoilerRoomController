#include <FlowRateSensors.h>

FlowRateSensors *flowRateSensorsPointer;

ICACHE_RAM_ATTR static void coldWaterInterruptHandler(void) {
  flowRateSensorsPointer->coldWaterISR();
}

ICACHE_RAM_ATTR static void hotWaterInterruptHandler(void) {
  flowRateSensorsPointer->hotWaterISR();
}

FlowRateSensors::FlowRateSensors(PubSubClient& mqtt, uint8_t coldWaterIntPin, uint8_t hotWaterIntPin)
: _coldWaterSensor(coldWaterIntPin), _hotWaterSensor(hotWaterIntPin)
{
  flowRateSensorsPointer = this;
  _coldWaterSensorPin = coldWaterIntPin;
  _hotWaterSensorPin = hotWaterIntPin;
  _mqtt = &mqtt;
}

void FlowRateSensors::begin()
{
  pinMode(_coldWaterSensorPin, INPUT_PULLUP);
  pinMode(_hotWaterSensorPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(_coldWaterSensorPin), coldWaterInterruptHandler, RISING);
  attachInterrupt(digitalPinToInterrupt(_coldWaterSensorPin), hotWaterInterruptHandler, RISING);
}

void FlowRateSensors::setTickPeriod(unsigned long period)
{
  _tickPeriod = period;
}

void FlowRateSensors::update()
{
  if (_lastTickTime + _tickPeriod < millis()){
    _coldWaterSensor.tick(millis() - _lastTickTime);
    _hotWaterSensor.tick(millis() - _lastTickTime);
    _lastTickTime = millis();
  }
}

std::vector<String> FlowRateSensors::getNames()
{
  std::vector<String> names(4);
  names[0] = getNameFormatString(SENSOR_NAME_WATER_COLD, SENSOR_UNIT_FLOW);
  names[1] = getNameFormatString(SENSOR_NAME_WATER_COLD, SENSOR_UNIT_VOLUME);
  names[2] = getNameFormatString(SENSOR_NAME_WATER_HOT, SENSOR_UNIT_FLOW);
  names[3] = getNameFormatString(SENSOR_NAME_WATER_HOT, SENSOR_UNIT_VOLUME);
  return names;
}

std::vector<double> FlowRateSensors::getStates()
{
    std::vector<double> states(4);
    states[0] =_coldWaterSensor.getCurrentFlowrate();
    states[1] =_coldWaterSensor.getTotalVolume();
    states[2] =_hotWaterSensor.getCurrentFlowrate();
    states[3] =_hotWaterSensor.getTotalVolume();
    return states;
}

String FlowRateSensors::getNameFormatString(const char* arg1, const char* arg2)
{
  char buff[20];
  sprintf(buff, "%s-%s", arg1, arg2);
  return String(buff);
}