#include <HCSR04.h>

#define REFRESH_INTERVAL 30 * 1000
#define SENSOR_NAME "salt-level"
#define DISTANCE_DIFF_RATIO 1
#define LEVEL_MAX_DISTANCE -100

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
        double _lastValue = 0.0;
        unsigned long _lastRefreshTime = 0;
        DistanceChangedCallback _distanceChangedCallback;
        double calculateLevel(double distance);

};