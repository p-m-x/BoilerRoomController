#include <FlowRateSensors.h>

FlowRateSensors *flowRateSensorsPointer;

ICACHE_RAM_ATTR static void coldWaterInterruptHandler(void) {
  flowRateSensorsPointer->coldWaterISR();
}

ICACHE_RAM_ATTR static void hotWaterInterruptHandler(void) {
  flowRateSensorsPointer->hotWaterISR();
}

FlowRateSensors::FlowRateSensors(uint8_t coldWaterIntPin, uint8_t hotWaterIntPin)
: _coldWaterSensor(coldWaterIntPin, FS400A), _hotWaterSensor(hotWaterIntPin, FS400A)
{
  flowRateSensorsPointer = this;
  _coldWaterSensorPin = coldWaterIntPin;
  _hotWaterSensorPin = hotWaterIntPin;
}

void FlowRateSensors::begin()
{
  pinMode(_coldWaterSensorPin, INPUT_PULLUP);
  pinMode(_hotWaterSensorPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(_coldWaterSensorPin), coldWaterInterruptHandler, RISING);
  attachInterrupt(digitalPinToInterrupt(_hotWaterSensorPin), hotWaterInterruptHandler, RISING);

  _lastValues.resize(4);
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
    std::vector<double> values = getStates();
    for (uint8_t i = 0; i < values.size(); i++) {
      if (values[i] != _lastValues[i]) {
        _lastValues[i] = values[i];
        if (_valuesChangeCallback != NULL) {
          _valuesChangeCallback(_names[i].c_str(), values[i]);
        }
      }
    }
  }
}

std::vector<String> FlowRateSensors::getNames()
{
  return _names;
}

std::vector<double> FlowRateSensors::getStates()
{
    std::vector<double> states(4);
    states[Measurement::COLD_WATER_FLOW] =_coldWaterSensor.getCurrentFlowrate();
    states[Measurement::COLD_WATER_VOLUME] =_coldWaterSensor.getTotalVolume();
    states[Measurement::HOT_WATER_FLOW] =_hotWaterSensor.getCurrentFlowrate();
    states[Measurement::HOT_WATER_VOLUME] =_hotWaterSensor.getTotalVolume();
    return states;
}
