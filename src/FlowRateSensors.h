#include <FlowMeter.h>
#include <PubSubClient.h>

#define DEFAULT_TICK_PERIOD 1000

class FlowRateSensors {
  public:
    FlowRateSensors(PubSubClient& mqtt, uint8_t coldWaterIntPin, uint8_t hotWaterIntPin);
    void update();
    void setTickPeriod(unsigned long period);

  private:
    PubSubClient* _mqtt;
    FlowMeter _coldWaterSensor;
    FlowMeter _hotWaterSensor;
    unsigned long _tickPeriod = DEFAULT_TICK_PERIOD;
    unsigned long _lastTickTime = 0;
};