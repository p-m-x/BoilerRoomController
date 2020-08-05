#include <PubSubClient.h>

class MqttAdapter {
    public:
      MqttAdapter(PubSubClient client);
    private:
      PubSubClient _mqtt;
};