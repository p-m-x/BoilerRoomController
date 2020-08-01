#include <relays.h>

Relays::Relays(RelayStateCallback callback, uint8_t pcf8574Address)
:
_pcf8574(pcf8574Address)
{
    _callback = callback;
}

void Relays::begin(void)
{
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        _pcf8574.pinMode(i, OUTPUT);
        _pcf8574.digitalWrite(i, RELAY_INITIAL_STATE);
    }
    _pcf8574.begin();
}

bool Relays::setRelay(uint8_t relayId, bool on)
{
    if (_pcf8574.digitalWrite(relayId, (uint8_t)!on)) {
        _callback(relayId, _pcf8574.digitalRead(relayId) == RELAY_ACTIVE_VALUE ? true : false);
        return true;
    }
    return false;
}

bool Relays::getState(uint8_t relayId)
{
    return _pcf8574.digitalRead(relayId) == RELAY_ACTIVE_VALUE ? true : false;
}
