#include <mqtt.h>

static const char* DEVICE_NAMESPACE = "home/boiler-room/";


MqttAdapter::MqttAdapter(PubSubClient client)
{
    _mqtt = client;
}
