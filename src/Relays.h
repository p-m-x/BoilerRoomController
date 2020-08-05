#include <PubSubClient.h>
#include <PCF8574.h>

#define NUM_RELAYS 8
#define RELAY_ACTIVE_VALUE 0
#define RELAY_INITIAL_STATE HIGH
#define MQTT_AVAILABILITY_MSG_INTERVAL 60000

#define RELAY_STATE_ON_PAYLOAD "on"
#define RELAY_STATE_OFF_PAYLOAD "off"
#define RELAY_AVAILABILITY_PL "online"

#define HA_SW_VERSION "0.0.1"
#define HA_DEVICE_SN "PREGO1"
#define HA_DEVICE_NAME "BRC1"
#define HA_DISCO_RELAY_TOPIC_TPL "%s/switch/brc-r-%d/config"
#define HA_DISCO_DEVICE_INFO_JSON "{\"name\":\""HA_DEVICE_NAME"\",\"sw\":\"" HA_SW_VERSION "\",\"ids\":[\"" HA_DEVICE_SN "\"]}"
#define HA_DISCO_RELAY_PAYLOAD_TPL "{\"name\":\"brc-relay-%d\",\"uniq_id\":\""HA_DEVICE_SN"-r-%d\",\"stat_t\":\"%s\",\"cmd_t\":\"%s\",\"avty_t\":\"%s\",\"pl_off\":\"off\",\"pl_on\":\"on\",\"dev\":%s}"

static const char* MQTT_RELAY_TOPIC_PREFIX = "home/boiler-room/relay";
static const char* MQTT_RELAY_TOPIC_STATE_POSTFIX = "state";
static const char* MQTT_RELAY_TOPIC_COMMAND_POSTFIX = "set";
static const char* MQTT_RELAY_TOPIC_AVAILABLE_POSTFIX = "avty";

typedef std::function<void(uint8_t, bool)> RelayStateCallback;

class Relays {
    public:
        Relays(RelayStateCallback callback, PubSubClient& mqtt, uint8_t pcf8574Address);
        Relays(PubSubClient& mqtt, uint8_t pcf8574Address);
        void begin();
        bool setState(uint8_t relayId, bool on);
        bool getState(uint8_t relayId);
        uint8_t getCount() {
            return NUM_RELAYS;
        };
        bool sendState();
        bool sendHomeassistantDiscovery();
        bool sendAvailabilityMessage();
        void setHomeassistantDiscoveryTopicPrefix(char* prefix);
        void initSubscriptions();
    private:
        PCF8574 _pcf8574;
        RelayStateCallback _callback;
        PubSubClient* _mqtt;
        unsigned long _mqttAvalLastSentTime = 0;
        char* _hassioTopicPrefix;
        void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
        bool sendState(uint8 relayId, bool state);
        bool sendState(uint8 relayId);
        String getMqttDiscoveryTopic(uint8_t relayId);
        String getMqttCommandTopic(uint8_t relayId);
        String getMqttStateTopic(uint8_t relayId);
        String getMqttAvabilityTopic(uint8_t relayId);
};