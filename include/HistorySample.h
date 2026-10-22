#pragma once

#include <Arduino.h>

// Persistentes 5-Minuten-Messwertformat; Layout nicht aendern.
struct HistorySample {
  uint32_t epoch; uint32_t uptime;
  int16_t outside10,room10,boiler10;
  uint8_t flags;
  uint32_t starts, burnerSec, pumpSec;
};
