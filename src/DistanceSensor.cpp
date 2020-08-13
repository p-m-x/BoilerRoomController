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
    }

    if (_lastRefreshTime + REFRESH_INTERVAL > millis()) {
        if (_sampleCounter < SAMPLE_COUNT && _lastSampleTime + SAMPLE_INTERVAL < millis()) {
            _lastSampleTime = millis();
            _samples[_sampleCounter++] =_sensor.measureDistanceCm();
        }

        if (_sampleCounter == SAMPLE_COUNT) {
            double sum = 0.0;
            for (uint8_t s = 0; s < _samples.size(); s++) {
                sum += _sensor.measureDistanceCm();
            }
            double distance = sum /_samples.size();
            if (abs(distance - _lastValue) >= DISTANCE_DIFF_RATIO) {
                _distanceChangedCallback(SENSOR_NAME, sum /_samples.size());
                _lastValue = distance;
            }

            _sampleCounter = 0;
        }

    }

}