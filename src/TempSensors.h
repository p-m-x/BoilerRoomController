#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

#define TEMP_REQUEST_INTERVAL 10000
#define TEMPERATURE_PRECISION 9

typedef std::function<void(const char*, float)> TemperatureChangedCallback;

class TempSensors : DallasTemperature {
    public:
        TempSensors(uint8_t oneWirePin);
        void begin();
        void update();
        void setValueChangedCallback(TemperatureChangedCallback c) {
            _valuesChangedCallback = c;
        }
        std::vector<String> getNames();
        std::vector<float> getStates();
    private:
        DallasTemperature _ds;
        OneWire _oneWire;
        TemperatureChangedCallback _valuesChangedCallback;
        unsigned long _lastTempRequestTime = 0;
        std::vector<float> _lastValues = {};
};