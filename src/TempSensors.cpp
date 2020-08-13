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
    _lastValues.resize(_ds.getDS18Count());
}

void TempSensors::update()
{
    if ((_lastTempRequestTime + TEMP_REQUEST_INTERVAL) < millis()) {
        _ds.requestTemperatures();
        _lastTempRequestTime = millis();
        if (_ds.isConversionComplete()) {
            std::vector<float> values = getStates();

            for (uint8_t i = 0; i < _ds.getDS18Count(); i++) {
                if (values[i] != _lastValues[i]) {
                    _lastValues[i] = values[i];
                    if (_valuesChangedCallback != NULL) {
                        _valuesChangedCallback(String(i, DEC).c_str(), values[i]);
                    }
                }
            }
        }
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
        states[i] =_ds.getTempCByIndex(i);
    }
    return states;
}
