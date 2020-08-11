#include <FlowMeter.h>
#include <PubSubClient.h>

#define DEFAULT_TICK_PERIOD 1000

static const char* SENSOR_NAME_WATER_COLD = "cold-water";
static const char* SENSOR_NAME_WATER_HOT = "hot-water";
static const char* SENSOR_UNIT_FLOW = "flow";
static const char* SENSOR_UNIT_VOLUME = "volume";

class FlowRateSensors {
  public:
    FlowRateSensors(PubSubClient& mqtt, uint8_t coldWaterIntPin, uint8_t hotWaterIntPin);
    void begin();
    void update();
    void setTickPeriod(unsigned long period);
    void coldWaterISR(void) {
      _coldWaterSensor.count();
    }
    void hotWaterISR(void) {
      _hotWaterSensor.count();
    }
    std::vector<double> getStates();
    std::vector<String> getNames();

  private:
    PubSubClient* _mqtt;
    FlowMeter _coldWaterSensor;
    FlowMeter _hotWaterSensor;
    uint8_t _coldWaterSensorPin, _hotWaterSensorPin;
    unsigned long _tickPeriod = DEFAULT_TICK_PERIOD;
    unsigned long _lastTickTime = 0;
    String getNameFormatString(const char* arg1, const char* arg2);
};

