#include <MqttAdapter.h>

MqttAdapter::MqttAdapter(PubSubClient& mqtt, MqttSubscriptionCallback callback)
{
    _callback = callback;
    _mqtt =&mqtt;
    _mqtt->setCallback(std::bind(&MqttAdapter::handleMqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

MqttAdapter::~MqttAdapter()
{
}

void MqttAdapter::handleMqttCallback(char* topic, uint8_t* payload, unsigned int length)
{
    for (uint8_t i = 0; i < _subscriptionName.size(); i++) {
        String t = getSubscriptionTopic(_subscriptionType[i], _subscriptionName[i].c_str());
        if (t.equals(topic)) {
            String pl = "";
            for (unsigned int p = 0; p < length; p++) {
                pl += String(char(payload[p]));
            }
            _callback(_subscriptionType[i], _subscriptionName[i].c_str(), pl);
            return;
        }
    }

}

String MqttAdapter::getStateTopic(Type type, const char* name)
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

String MqttAdapter::getSubscriptionTopic(Type type, const char* name)
{
    char buff[100];
    switch(type)
    {
        case RELAY:
            sprintf(buff, MQTT_RELAY_TOPIC_COMMAND_TPL, MQTT_TOPIC_NAMESPACE, name);
            break;
        case HOMEASSISTANT:
            sprintf(buff, MQTT_HOMEASSISTANT_AVAILABILITY_TOPIC, _discoveryTopicPrefix);
            break;
        default:
            break;

    }
    return String(buff);
}

String MqttAdapter::getAvailabilityTopic()
{
    char buff[100];
    sprintf(buff, MQTT_AVAILABILITY_TOPIC_TPL, MQTT_TOPIC_NAMESPACE, HA_DEVICE_NAME);
    return String(buff);
}

String MqttAdapter::getDiscoveryTopic(Type type, const char* name)
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

bool MqttAdapter::sendState(Type type, const char* sensorId, bool value)
{
    sendState(type, sensorId, value ? STATE_ON_PL : STATE_OFF_PL);
}

bool MqttAdapter::sendState(Type type, const char* sensorId, const char* value)
{
    return _mqtt->publish(getStateTopic(type, sensorId).c_str(), value);
}

bool MqttAdapter::sendDiscovery(Type type, const char* name)
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
            json["cmd_t"] = getSubscriptionTopic(type, name);
            break;
        case TEMP_SENSOR:
            sprintf(nameBuff, "%s-temperature-%s", DEVICE_PLACE_NAME, name);
            json["name"] = nameBuff;
            json["stat_t"] = getStateTopic(type, name);
            json["unit_of_meas"] = "°C";
            json["dev_cla"] = "temperature";
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

bool MqttAdapter::sendAvailability()
{
    return _mqtt->publish(getAvailabilityTopic().c_str(), AVAILABILITY_ONLINE_PL);
}

bool MqttAdapter::subscribe(Type type, const char* name)
{
    String topic = getSubscriptionTopic(type, name);
    if (topic.length() > 0 && _mqtt->subscribe(topic.c_str(), SUBSCRIPTION_QOS)) {
        Serial.print("Register supscription handler on ");
        Serial.println(topic);
        _subscriptionName.push_back(String(name));
        _subscriptionType.push_back(type);
        return true;
    }
    return false;
}