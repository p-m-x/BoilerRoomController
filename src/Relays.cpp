#include <Relays.h>

Relays::Relays(RelayStateCallback callback, PubSubClient& mqtt, uint8_t pcf8574Address)
:
_pcf8574(pcf8574Address)
{
    _callback = callback;
    _mqtt = &mqtt;
}

Relays::Relays(PubSubClient& mqtt, uint8_t pcf8574Address)
:
_pcf8574(pcf8574Address)
{
    _mqtt = &mqtt;
}

void Relays::begin()
{
    Serial.println(F("Initialize relays"));
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        _pcf8574.pinMode(i, OUTPUT);
        _pcf8574.digitalWrite(i, RELAY_INITIAL_STATE);
    }
    _pcf8574.begin();
    _mqtt->setCallback(std::bind(&Relays::mqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void Relays::setHomeassistantDiscoveryTopicPrefix(char* prefix)
{
   _hassioTopicPrefix = prefix;
}

bool Relays::setState(uint8_t relayId, bool on)
{
    if (_pcf8574.digitalWrite(relayId, (uint8_t)!on)) {
        return sendState(relayId);
    }
    return false;
}

bool Relays::getState(uint8_t relayId)
{
    return _pcf8574.digitalRead(relayId) == RELAY_ACTIVE_VALUE ? true : false;
}

String Relays::getMqttCommandTopic(uint8_t relayId)
{
  return String(MQTT_RELAY_TOPIC_PREFIX) + "/" + String(relayId) + "/" + String(MQTT_RELAY_TOPIC_COMMAND_POSTFIX);
}

String Relays::getMqttStateTopic(uint8_t relayId)
{
  return String(MQTT_RELAY_TOPIC_PREFIX) + "/" + String(relayId) + "/" + String(MQTT_RELAY_TOPIC_STATE_POSTFIX);
}

String Relays::getMqttAvabilityTopic(uint8_t relayId)
{
  return String(MQTT_RELAY_TOPIC_PREFIX) + "/" + String(relayId) + "/" + String(MQTT_RELAY_TOPIC_AVAILABLE_POSTFIX);
}

String Relays::getMqttDiscoveryTopic(uint8_t relayId)
{
    char topicBuff[70];
    sprintf(topicBuff, HA_DISCO_RELAY_TOPIC_TPL, _hassioTopicPrefix, relayId);
    return String(topicBuff);
}

bool Relays::sendHomeassistantDiscovery()
{
    for (uint8_t i = 0; i < getCount(); i++) {
        char payload[400];
        sprintf(payload, HA_DISCO_RELAY_PAYLOAD_TPL, i, i,
            getMqttStateTopic(i).c_str(),
            getMqttCommandTopic(i).c_str(),
            getMqttAvabilityTopic(i).c_str(),
            HA_DISCO_DEVICE_INFO_JSON);
        _mqtt->publish(getMqttDiscoveryTopic(i).c_str(), payload);
    }
    Serial.println(F("Discovery message sent"));
    return true;
}

bool Relays::sendAvailabilityMessage()
{
    for (uint8_t i = 0; i < getCount(); i++) {
        _mqtt->publish(getMqttAvabilityTopic(i).c_str(), RELAY_AVAILABILITY_PL);
    }
    _mqttAvalLastSentTime = millis();
    Serial.println(F("Availability message sent"));
}

void Relays::initSubscriptions()
{
    for (uint8_t i = 0; i < getCount(); i++) {
        _mqtt->unsubscribe(getMqttCommandTopic(i).c_str());
        _mqtt->subscribe(getMqttCommandTopic(i).c_str());
        Serial.print(F("Register subscription on "));
        Serial.println(getMqttCommandTopic(i));
    }
}

void Relays::mqttCallback(char* topic, uint8_t* payload, unsigned int length)
{
    String t(topic);
    if (t.startsWith(MQTT_RELAY_TOPIC_PREFIX)) {
        uint8_t startIndex = String(MQTT_RELAY_TOPIC_PREFIX).length() + 1;
        uint8_t endIndex = t.indexOf("/", startIndex + 1);
        uint8_t relayId = atoi(t.substring(startIndex, endIndex).c_str());

        String p = "";
        for (uint8_t i = 0; i < length; i++) {
            p += String((char)payload[i]);
        }
        setState(relayId, p.equals(RELAY_STATE_ON_PAYLOAD));
    }
}

bool Relays::sendState(uint8 relayId, bool state)
{
    return _mqtt->publish(getMqttStateTopic(relayId).c_str(), state ? RELAY_STATE_ON_PAYLOAD : RELAY_STATE_OFF_PAYLOAD);
}

bool Relays::sendState(uint8 relayId)
{
    return sendState(relayId, _pcf8574.digitalRead(relayId) == RELAY_ACTIVE_VALUE);
}

bool Relays::sendState()
{
    for (uint8_t i = 0; i < getCount(); i++) {
        sendState(i, _pcf8574.digitalRead(i) ? false : true);
    }
    return true;
}

