#include <Relays.h>

Relays::Relays(uint8_t pcf8574Address)
:
_pcf8574(pcf8574Address)
{
}

void Relays::begin()
{
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        _pcf8574.pinMode(i, OUTPUT);
        _pcf8574.digitalWrite(i, RELAY_INITIAL_STATE);
    }
    _pcf8574.begin();
}

std::vector<String> Relays::getNames()
{
    std::vector<String> names(getCount());
    for (uint8_t i = 0; i < getCount(); i++) {
        names[i] = String(i, DEC);
    }
    return names;
}

bool Relays::setState(uint8_t relayId, bool on)
{
    // check if _alwaysSingleRelayId contains relayId
    bool singleRelay = false;
    if (on) {
        for (uint8_t i = 0; i < _alwaysSingleRelayId.size(); i++) {
            if (relayId == _alwaysSingleRelayId[i]) {
                singleRelay = true;
                break;
            }
        }
    }

    // turn off other relays from the list
    if (singleRelay) {
        for (uint8_t i = 0; i < _alwaysSingleRelayId.size(); i++) {
            if (relayId == _alwaysSingleRelayId[i]) continue;
            if (getState(_alwaysSingleRelayId[i])) changeState(_alwaysSingleRelayId[i], false);
        }
    }
    // turn on selected relyId
    return changeState(relayId, on);
}

bool Relays::setState(uint8_t relayId, String value)
{
    return setState(relayId, value.equals(RELAY_STATE_ON_PAYLOAD));
}

bool Relays::changeState(uint8_t relayId, bool on)
{
    if (_pcf8574.digitalWrite(relayId, (uint8_t)(on == (RELAY_ACTIVE_VALUE == 1)))) {
        if (_stateChangedCallback != NULL) {
            _stateChangedCallback(relayId, on);
        }
        return true;

    }
    return false;
}

bool Relays::getState(uint8_t relayId)
{
    return _pcf8574.digitalRead(relayId) == RELAY_ACTIVE_VALUE ? true : false;
}

std::vector<bool> Relays::getStates()
{
    std::vector<bool> boolArray(NUM_RELAYS);
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        boolArray[i] = getState(i);
    }
    return boolArray;
}
