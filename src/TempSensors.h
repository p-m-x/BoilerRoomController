#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

#define DS_DEVICES_MAX_COUNT 10
#define TEMPERATURE_PRECISION 9
#define DS_AVAILABILITY_PL "online"
#define HA_SW_VERSION "0.0.1"
#define HA_DEVICE_SN "PREGO1"
#define HA_DEVICE_NAME "BRC1"
#define HA_DISCO_DS_TOPIC_TPL "%s/sensor/"HA_DEVICE_NAME"/t-%d/config"
#define HA_DISCO_DEVICE_INFO_JSON "{ \
    \"name\":\""HA_DEVICE_NAME"\", \
    \"sw\":\"" HA_SW_VERSION "\", \
    \"ids\":[\"" HA_DEVICE_SN "\"]}"
#define HA_DISCO_DS_PAYLOAD_TPL "{ \
    \"name\":\"brc-temp-%d\", \
    \"uniq_id\":\""HA_DEVICE_SN"-t-%d\", \
    \"stat_t\":\"%s\", \
    \"avty_t\":\"%s\", \
    \"dev_cla\":\"temperature\", \
    \"dev\":%s}"

static const char* MQTT_DS_TOPIC_PREFIX = "home/boiler-room/temp";
static const char* MQTT_DS_TOPIC_STATE_POSTFIX = "state";
static const char* MQTT_DS_TOPIC_COMMAND_POSTFIX = "set";
static const char* MQTT_DS_TOPIC_AVAILABLE_POSTFIX = "avty";

class TempSensors : DallasTemperature {
    public:
        TempSensors(PubSubClient& mqtt, uint8_t oneWirePin);
        void begin();
        void setHomeassistantDiscoveryTopicPrefix(char* prefix);
        bool sendState();
        bool sendState(uint8_t sensorId);
        bool sendHomeassistantDiscovery();
        bool sendAvailabilityMessage();
    private:
        DallasTemperature _ds;
        OneWire _oneWire;
        PubSubClient* _mqtt;
        char* _hassioTopicPrefix;
        String getMqttDiscoveryTopic(uint8_t relayId);
        String getMqttCommandTopic(uint8_t relayId);
        String getMqttStateTopic(uint8_t relayId);
        String getMqttAvabilityTopic(uint8_t relayId);
};