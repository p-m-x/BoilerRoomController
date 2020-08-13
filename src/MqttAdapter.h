#include <PubSubClient.h>
#include <ArduinoJson.h>

#define MQTT_PAYLOAD_SIZE 400

#define HA_SW_VERSION "0.0.1"
#define HA_DEVICE_SN "PREGO1"
#define HA_DEVICE_NAME "BRC1"
#define STATE_ON_PL "on"
#define STATE_OFF_PL "off"
#define SUBSCRIPTION_QOS 0

#define AVAILABILITY_ONLINE_PL "online"
#define MQTT_DISCOVERY_TOPIC_PREFIX "homeassistant"

static const char* DEVICE_PLACE_NAME = "boiler-room";
static const char* MQTT_TOPIC_NAMESPACE = "home/boiler-room";

#define MQTT_AVAILABILITY_TOPIC_TPL "%s/%s/status"
#define MQTT_HOMEASSISTANT_AVAILABILITY_TOPIC "%s/status"

#define MQTT_RELAY_DISCOVERY_TOPIC_TPL "%s/switch/"HA_DEVICE_NAME"/relay-%s/config"
#define MQTT_RELAY_TOPIC_STATE_TPL "%s/switch/"HA_DEVICE_NAME"-%s/state"
#define MQTT_RELAY_TOPIC_COMMAND_TPL "%s/switch/"HA_DEVICE_NAME"-%s/set"

#define MQTT_TEMP_SENSOR_DISCOVERY_TOPIC_TPL "%s/sensor/"HA_DEVICE_NAME"/temp-%s/config"
#define MQTT_TEMP_SENSOR_TOPIC_STATE_TPL "%s/temperature/"HA_DEVICE_NAME"-%s/state"

#define MQTT_WATER_SENSOR_DISCOVERY_TOPIC_TPL "%s/sensor/"HA_DEVICE_NAME"/%s/config"
#define MQTT_WATER_SENSOR_TOPIC_STATE_TPL "%s/"HA_DEVICE_NAME"-%s/state"

#define MQTT_DISTANCE_SENSOR_DISCOVERY_TOPIC_TPL "%s/sensor/"HA_DEVICE_NAME"/%s/config"
#define MQTT_DISTANCE_SENSOR_TOPIC_STATE_TPL "%s/"HA_DEVICE_NAME"-%s/state"

enum Type { RELAY, TEMP_SENSOR, WATER_FLOW_SENSOR, DISTANCE_SENSOR, HOMEASSISTANT };

typedef std::function<void(Type, const char*, String)> MqttSubscriptionCallback;

class MqttAdapter
{

private:
    PubSubClient* _mqtt;
    char* _discoveryTopicPrefix = MQTT_DISCOVERY_TOPIC_PREFIX;
    std::vector<String> _subscriptionName = {};
    std::vector<Type> _subscriptionType = {};
    MqttSubscriptionCallback _callback;
    void handleMqttCallback(char* topic, uint8_t* payload, unsigned int length);
    String getStateTopic(Type type, const char* name);
    String getSubscriptionTopic(Type type, const char* name);
    String getAvailabilityTopic();
    String getDiscoveryTopic(Type type, const char* name);
public:
    MqttAdapter(PubSubClient& mqtt, MqttSubscriptionCallback callback);
    ~MqttAdapter();
    void setDiscoveryTopicPrefix(char* prefix) {
        _discoveryTopicPrefix = prefix;
    }
    bool sendState(Type type, const char* sensorId, const char* value);
    bool sendState(Type type, const char* sensorId, bool value);
    bool sendDiscovery(Type type, const char* name);
    bool sendAvailability();
    bool subscribe(Type type, const char* name);
};

