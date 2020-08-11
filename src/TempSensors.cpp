#include <TempSensors.h>

TempSensors::TempSensors(PubSubClient& mqtt, uint8_t oneWirePin)
: _oneWire(oneWirePin), _ds(&_oneWire)
{
    _mqtt = &mqtt;
}

void TempSensors::begin()
{
    _ds.begin();
    _ds.setCheckForConversion(true);
}

void TempSensors::update()
{
    if ((_lastTempRequestTime + TEMP_REQUEST_INTERVAL) < millis()) {
        _ds.requestTemperatures();
        _lastTempRequestTime = millis();
    }
}

std::vector<String> TempSensors::getNames()
{
    std::vector<String> names(_ds.getDS18Count());
    for (uint8_t i = 0; i < _ds.getDS18Count(); i++) {
        names[i] = String(i, DEC);
    }
    return names;
}

std::vector<float> TempSensors::getStates()
{
    std::vector<float> states(_ds.getDS18Count());

    for (uint8_t i = 0; i < _ds.getDS18Count(); i++) {
        _ds.requestTemperaturesByIndex(i);
        states[i] =_ds.getTempCByIndex(i);
    }
    return states;
}
