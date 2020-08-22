#include <DistanceSensor.h>

DistanceSensor::DistanceSensor(uint8_t triggerPin, uint8_t echoPin)
:
_sensor(triggerPin, echoPin)
{
}

void DistanceSensor::update()
{
    if (_distanceChangedCallback == NULL) {
        return;
    }

    if (_lastRefreshTime + REFRESH_INTERVAL < millis()) {
        _lastRefreshTime = millis();
        double distance = _sensor.measureDistanceCm();
        if (abs(distance - _lastValue) >= DISTANCE_DIFF_RATIO) {
            _distanceChangedCallback(SENSOR_NAME, calculateLevel(distance));
            _lastValue = distance;
        }
    }

}

double DistanceSensor::calculateLevel(double distance)
{
    double level = distance / abs(LEVEL_MAX_DISTANCE);
    if (LEVEL_MAX_DISTANCE < 0) {
        level = 1 - level;
    }
    return level * 100;

}