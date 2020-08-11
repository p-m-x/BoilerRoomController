#include <MqttPublisher.h>

MqttPublisher::MqttPublisher(PubSubClient& mqtt)
{
    _mqtt =&mqtt;
}

MqttPublisher::~MqttPublisher()
{
}

String MqttPublisher::getStateTopic(Type type, const char* name)
{
    char buff[100];
    switch(type)
    {
        case RELAY:
            sprintf(buff, MQTT_RELAY_TOPIC_STATE_TPL, MQTT_TOPIC_NAMESPACE, name);
            break;
        case TEMP_SENSOR:
            sprintf(buff, MQTT_TEMP_SENSOR_TOPIC_STATE_TPL, MQTT_TOPIC_NAMESPACE, name);
            break;
        case WATER_FLOW_SENSOR:
            sprintf(buff, MQTT_WATER_SENSOR_TOPIC_STATE_TPL, MQTT_TOPIC_NAMESPACE, name);
            break;
        default:
            break;

    }
    return String(buff);
}

String MqttPublisher::getCommandTopic(Type type, const char* name)
{
    char buff[100];
    switch(type)
    {
        case RELAY:
            sprintf(buff, MQTT_RELAY_TOPIC_COMMAND_TPL, MQTT_TOPIC_NAMESPACE, name);
            break;
        default:
            break;

    }
    return String(buff);
}

String MqttPublisher::getAvailabilityTopic()
{
    char buff[100];
    sprintf(buff, MQTT_AVAILABILITY_TOPIC_TPL, MQTT_TOPIC_NAMESPACE, HA_DEVICE_NAME);
    return String(buff);
}

String MqttPublisher::getDiscoveryTopic(Type type, const char* name)
{
    char buff[100];
    switch(type)
    {
        case RELAY:
            sprintf(buff, MQTT_RELAY_DISCOVERY_TOPIC_TPL, _discoveryTopicPrefix, name);
            break;
        case TEMP_SENSOR:
            sprintf(buff, MQTT_TEMP_SENSOR_DISCOVERY_TOPIC_TPL, _discoveryTopicPrefix, name);
            break;
        case WATER_FLOW_SENSOR:
            sprintf(buff, MQTT_WATER_SENSOR_DISCOVERY_TOPIC_TPL, _discoveryTopicPrefix, name);
            break;
        default:
            break;

    }
    return String(buff);
}

bool MqttPublisher::sendState(Type type, const char* sensorId, bool value)
{
    sendState(type, sensorId, value ? STATE_ON_PL : STATE_OFF_PL);
}

bool MqttPublisher::sendState(Type type, const char* sensorId, const char* value)
{
    return _mqtt->publish(getStateTopic(type, sensorId).c_str(), value);
}

bool MqttPublisher::sendDiscovery(Type type, const char* name)
{
    String topic = getDiscoveryTopic(type, name);
    if (topic.length() == 0) {
        return false;
    }
    StaticJsonDocument<MQTT_PAYLOAD_SIZE> json;

    JsonObject dev = json.createNestedObject("dev");
    dev["name"] = HA_DEVICE_NAME;
    dev["sw"] = HA_SW_VERSION;

    JsonArray ids = dev.createNestedArray("ids");
    ids.add(HA_DEVICE_SN);

    json["avty_t"] = getAvailabilityTopic();

    char nameBuff[15];
    switch(type)
    {
        case RELAY:
            sprintf(nameBuff, "%s-relay-%s", DEVICE_PLACE_NAME, name);
            json["name"] = nameBuff;
            json["pl_on"] = STATE_ON_PL;
            json["pl_off"] = STATE_OFF_PL;
            json["stat_t"] = getStateTopic(type, name);
            json["cmd_t"] = getCommandTopic(type, name);
            break;
        case TEMP_SENSOR:
            sprintf(nameBuff, "%s-temperature-%s", DEVICE_PLACE_NAME, name);
            json["name"] = nameBuff;
            json["stat_t"] = getStateTopic(type, name);
            break;
        case WATER_FLOW_SENSOR:
            sprintf(nameBuff, "%s-%s", DEVICE_PLACE_NAME, name);
            json["name"] = nameBuff;
            json["stat_t"] = getStateTopic(type, name);
            break;
        default:
            return false;
    }
    char buff[50];
    sprintf(buff, "%s-%d-%s", HA_DEVICE_SN, type, name);
    json["uniq_id"] = String(buff);

    char payload[MQTT_PAYLOAD_SIZE];
    serializeJson(json, payload);
    return _mqtt->publish(topic.c_str(), payload);
}

bool MqttPublisher::sendAvailability()
{
    return _mqtt->publish(getAvailabilityTopic().c_str(), AVAILABILITY_ONLINE_PL);
}