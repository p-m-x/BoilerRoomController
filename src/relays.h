
#include <PubSubClient.h>
#include <PCF8574.h>

#define NUM_RELAYS 8
#define RELAY_ACTIVE_VALUE 0
#define RELAY_INITIAL_STATE HIGH


typedef std::function<void(uint8_t, bool)> RelayStateCallback;

class Relays {
    public:
        Relays(RelayStateCallback callback, uint8_t pcf8574Address);
        void begin(void);
        bool setRelay(uint8_t relayId, bool on);
        bool getState(uint8_t relayId);
        uint8_t getCount() {
            return NUM_RELAYS;
        };
    private:
        PCF8574 _pcf8574;
        RelayStateCallback _callback;
};