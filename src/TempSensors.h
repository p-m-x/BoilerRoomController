#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

#define TEMP_REQUEST_INTERVAL 10000
#define DS_DEVICES_MAX_COUNT 10
#define TEMPERATURE_PRECISION 9

class TempSensors : DallasTemperature {
    public:
        TempSensors(PubSubClient& mqtt, uint8_t oneWirePin);
        void begin();
        void update();
        std::vector<String> getNames();
        std::vector<float> getStates();
    private:
        DallasTemperature _ds;
        OneWire _oneWire;
        PubSubClient* _mqtt;
        unsigned long _lastTempRequestTime = 0;
};