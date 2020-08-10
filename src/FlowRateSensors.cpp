#include <FlowRateSensors.h>

FlowRateSensors::FlowRateSensors(PubSubClient& mqtt, uint8_t coldWaterIntPin, uint8_t hotWaterIntPin)
: _coldWaterSensor(coldWaterIntPin), _hotWaterSensor(hotWaterIntPin)
{
  _mqtt = &mqtt;
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