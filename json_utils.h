#pragma once

#include <Arduino.h>
#include "types.h"

// ============================================================================
// Utilidades de parseo de JSON ligero y de codificacion de URL/JSON.
// ============================================================================

String urlEncode(const String& value);
String jsonEscape(const String& value);

String extractStringField(const String& object, const char* fieldName);
int extractIntField(const String& object, const char* fieldName, int fallback);
String extractObjectField(const String& json, const char* fieldName);
bool parseBoolField(const String& object, const char* fieldName, bool fallback);

int parsePulses(const String& json, Pulse* pulses, int maxPulses);
