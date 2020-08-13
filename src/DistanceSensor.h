#include <HCSR04.h>

#define SAMPLE_COUNT 5
#define REFRESH_INTERVAL 5000
#define SAMPLE_INTERVAL 100
#define SENSOR_NAME "salt-level"
#define DISTANCE_DIFF_RATIO 1

typedef std::function<void(const char*, double)> DistanceChangedCallback;

class DistanceSensor
{
    public:
        DistanceSensor(uint8_t triggerPin, uint8_t echoPin);
        void update();
        void setDistanceChangedCallback(DistanceChangedCallback c) {
            _distanceChangedCallback = c;
        }
        String getName() {
            return String(SENSOR_NAME);
        }
        double getValue() {
            return _lastValue;
        }
    private:
        UltraSonicDistanceSensor _sensor;
        std::array<double, SAMPLE_COUNT> _samples;
        double _lastValue = 0.0;
        unsigned long _lastRefreshTime = 0;
        unsigned long _lastSampleTime = 0;
        uint8_t _sampleCounter = 0;
        DistanceChangedCallback _distanceChangedCallback;

};