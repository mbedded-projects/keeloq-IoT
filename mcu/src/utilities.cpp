#include "utilities.h"

GateController::GateController(uint8_t gatePin, uint8_t openPin, uint8_t openingPin, uint8_t closedPin, uint8_t closingPin)
    : gatePin(gatePin), openPin(openPin), openingPin(openingPin), closedPin(closedPin), closingPin(closingPin) {}

void GateController::begin()
{
    pinMode(openPin, INPUT_PULLUP);
    pinMode(openingPin, INPUT_PULLUP);
    pinMode(closedPin, INPUT_PULLUP);
    pinMode(closingPin, INPUT_PULLUP);
    pinMode(gatePin, OUTPUT);
    digitalWrite(GATE_PIN, LOW); // ustanowienie stanu podstawowego pinu od bramy
}

void GateController::toggleGate()
{
    digitalWrite(gatePin, HIGH);
    delay(1000);
    digitalWrite(gatePin, LOW);
}

void GateController::loop()
{
    bool openState = digitalRead(openPin);
    bool openingState = digitalRead(openingPin);
    bool closedState = digitalRead(closedPin);
    bool closingState = digitalRead(closingPin);

    GateState newState = PARTIALLY_OPEN;

    if (!openState)
        newState = OPEN;
    if (!openingState)
        newState = OPENING;
    if (!closedState)
        newState = CLOSED;
    if (!closingState)
        newState = CLOSING;

    if (newState != _gateState)
    {
        _gateState = newState;
        if (_callback != nullptr)
            _callback(newState);
    }
}

GateState GateController::getCurrentState()
{
    return _gateState;
}

void GateController::setCallback(GateCallback callback)
{
    _callback = callback;
}

const char *GateController::stateToString(GateState state)
{
    switch (state)
    {
    case OPEN:
        return "open";
    case OPENING:
        return "opening";
    case CLOSED:
        return "closed";
    case CLOSING:
        return "closing";
    case PARTIALLY_OPEN:
        return "partially_open";
    default:
        return "unknown";
    }
}

GateState toggle(GateState state)
{
    switch (state)
    {
    case OPEN:
        return CLOSING;
    case OPENING:
        return CLOSING;
    case CLOSED:
        return OPENING;
    case CLOSING:
        return OPENING;
    default:
        return UNKNOWN;
    }
}

void readCounters(std::vector<Remote> remotes)
{
    Preferences prefs;
    prefs.begin("counters", true); // tylko do odczytu
    for (auto &remote : remotes)
    {
        String key = String(remote.serial);
        remote.counter = prefs.getULong(key.c_str(), 0);
    }
    prefs.end();
}

void saveCounters(std::vector<Remote> remotes)
{
    Preferences prefs;
    prefs.begin("counters"); // do zapisu
    for (auto &remote : remotes)
    {
        String key = String(remote.serial);
        prefs.putULong(key.c_str(), remote.counter);
    }
    prefs.end();
}

String parseToString(byte *payload, unsigned int len)
{
    return String((const char *)payload).substring(0, len);
}
