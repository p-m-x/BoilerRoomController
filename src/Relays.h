#include <PubSubClient.h>
#include <PCF8574.h>

#define NUM_RELAYS 8
#define RELAY_ACTIVE_VALUE 0
#define RELAY_INITIAL_STATE HIGH
#define MQTT_AVAILABILITY_MSG_INTERVAL 60000

#define RELAY_STATE_ON_PAYLOAD "on"
#define RELAY_STATE_OFF_PAYLOAD "off"

typedef std::function<bool(uint8_t, bool)> RelayStateCallback;

class Relays {
    public:
        Relays(RelayStateCallback callback, PubSubClient& mqtt, uint8_t pcf8574Address);
        Relays(PubSubClient& mqtt, uint8_t pcf8574Address);
        void begin();
        bool setState(uint8_t relayId, bool on);
        bool getState(uint8_t relayId);
        std::vector<bool> getStates();
        std::vector<String> getNames();
        uint8_t getCount() {
            return NUM_RELAYS;
        };
    private:
        PCF8574 _pcf8574;
        RelayStateCallback _callback;
        PubSubClient* _mqtt;
};