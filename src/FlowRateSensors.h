#include <FlowMeter.h>

#define DEFAULT_TICK_PERIOD 5000

static const char* COLD_WATER_FLOW_LABEL = "cold-water-flow";
static const char* COLD_WATER_VOLUME_LABEL = "cold-water-volume";
static const char* HOT_WATER_FLOW_LABEL = "hot-water-flow";
static const char* HOT_WATER_VOLUME_LABEL = "hot-water-volume";

enum Measurement {COLD_WATER_FLOW, COLD_WATER_VOLUME, HOT_WATER_FLOW, HOT_WATER_VOLUME};

typedef std::function<void(const char*, double)> FlowRateChangedCallback;

class FlowRateSensors {
  public:
    FlowRateSensors(uint8_t coldWaterIntPin, uint8_t hotWaterIntPin);
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
    void setValueChangedCallback(FlowRateChangedCallback c) {
      _valuesChangeCallback = c;
    }

  private:
    FlowMeter _coldWaterSensor;
    FlowMeter _hotWaterSensor;
    uint8_t _coldWaterSensorPin, _hotWaterSensorPin;
    unsigned long _tickPeriod = DEFAULT_TICK_PERIOD;
    unsigned long _lastTickTime = 0;
    std::vector<String> _names = {
      COLD_WATER_FLOW_LABEL,
      COLD_WATER_VOLUME_LABEL,
      HOT_WATER_FLOW_LABEL,
      HOT_WATER_VOLUME_LABEL
    };
    std::vector<double> _lastValues = {};
    FlowRateChangedCallback _valuesChangeCallback;
};

