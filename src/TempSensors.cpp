#include <TempSensors.h>

TempSensors::TempSensors(PubSubClient& mqtt, uint8_t oneWirePin)
: _oneWire(oneWirePin), _ds(&_oneWire)
{
    _mqtt = &mqtt;
}

void TempSensors::begin()
{
    _ds.begin();
    _ds.setCheckForConversion(true);
}

bool TempSensors::sendState()
{
    for (uint8_t i = 0; i < _ds.getDS18Count(); i++) {
        sendState(i);
    }
    return true;
}

bool TempSensors::sendState(uint8_t sensorId)
{
    _ds.requestTemperaturesByIndex(sensorId);
    // while (!_ds.isConversionComplete()) {}
    float temp = _ds.getTempCByIndex(sensorId);
    return _mqtt->publish(getMqttStateTopic(sensorId).c_str(), String(temp).c_str());
}


String TempSensors::getMqttCommandTopic(uint8_t sensorId)
{
  return String(MQTT_DS_TOPIC_PREFIX) + "/" + String(sensorId) + "/" + String(MQTT_DS_TOPIC_COMMAND_POSTFIX);
}

String TempSensors::getMqttStateTopic(uint8_t sensorId)
{
  return String(MQTT_DS_TOPIC_PREFIX) + "/" + String(sensorId) + "/" + String(MQTT_DS_TOPIC_STATE_POSTFIX);
}

String TempSensors::getMqttAvabilityTopic(uint8_t sensorId)
{
  return String(MQTT_DS_TOPIC_PREFIX) + "/" + String(sensorId) + "/" + String(MQTT_DS_TOPIC_AVAILABLE_POSTFIX);
}

String TempSensors::getMqttDiscoveryTopic(uint8_t sensorId)
{
    char topicBuff[70];
    sprintf(topicBuff, HA_DISCO_DS_TOPIC_TPL, _hassioTopicPrefix, sensorId);
    return String(topicBuff);
}

void TempSensors::setHomeassistantDiscoveryTopicPrefix(char* prefix)
{
    _hassioTopicPrefix = prefix;
}

bool TempSensors::sendHomeassistantDiscovery()
{
    for (uint8_t i = 0; i < _ds.getDS18Count(); i++) {
        char payload[400];
        sprintf(payload, HA_DISCO_DS_PAYLOAD_TPL, i, i,
            getMqttStateTopic(i).c_str(),
            getMqttAvabilityTopic(i).c_str(),
            HA_DISCO_DEVICE_INFO_JSON);
        _mqtt->publish(getMqttDiscoveryTopic(i).c_str(), payload);
    }
    return true;
}

bool TempSensors::sendAvailabilityMessage()
{
    for (uint8_t i = 0; i < _ds.getDS18Count(); i++) {
        _mqtt->publish(getMqttAvabilityTopic(i).c_str(), DS_AVAILABILITY_PL);
    }
    return true;
}