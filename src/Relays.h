#include <PCF8574.h>

#define NUM_RELAYS 8
#define RELAY_ACTIVE_VALUE 0
#define RELAY_INACTIVE_VALUE 1
#define RELAY_INITIAL_STATE HIGH
#define MQTT_AVAILABILITY_MSG_INTERVAL 60000

#define RELAY_STATE_ON_PAYLOAD "on"
#define RELAY_STATE_OFF_PAYLOAD "off"

typedef std::function<void(uint8_t, bool)> RelayStateChangedCallback;

class Relays {
    public:
        Relays(uint8_t pcf8574Address);
        void begin();
        bool setState(uint8_t relayId, bool on);
        bool setState(uint8_t relayId, String value);
        bool getState(uint8_t relayId);
        std::vector<bool> getStates();
        std::vector<String> getNames();
        uint8_t getCount() {
            return NUM_RELAYS;
        };
        void setRelayStateChangedCallback(RelayStateChangedCallback c) {
            _stateChangedCallback = c;
        }
    private:
        PCF8574 _pcf8574;
        RelayStateChangedCallback _stateChangedCallback;
        std::vector<uint8_t> _alwaysSingleRelayId = {0,1,2,3,4,5};
        bool changeState(uint8_t relayId, bool on);
};