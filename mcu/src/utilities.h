#pragma once

#include <Preferences.h>
#include <vector>
#include <keeloq.h>
#include <config.h>
#include <Arduino.h>

#ifdef DEBUG
  #define log_begin(...) Serial.begin(__VA_ARGS__)
  #define log(...)       Serial.printf(__VA_ARGS__)
#else
  #define log(...)
  #define log_begin(...)
#endif

extern Preferences prefs;

enum GateState {
  OPEN,
  OPENING,
  CLOSED,
  CLOSING,
  PARTIALLY_OPEN,
  UNKNOWN
};


class GateController {
using GateCallback = void (*)(const GateState newState);
  private:
  uint8_t gatePin, openPin, openingPin, closedPin, closingPin;
  GateState _gateState = UNKNOWN;  
  GateCallback _callback = nullptr;
 public:
  GateController(uint8_t gatePin, uint8_t openPin, uint8_t openingPin, uint8_t closedPin, uint8_t closingPin);
  void toggleGate();
  void begin();
  void loop();
  void setCallback(GateCallback callback);
  GateState getCurrentState();
  const char* stateToString(GateState state);
};

void         readCounters(std::vector<Remote> remotes);
void         saveCounters(std::vector<Remote> remotes);
String       parseToString(byte* payload, unsigned int len);