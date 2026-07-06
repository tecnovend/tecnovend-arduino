#pragma once
#include <Arduino.h>

void setupOta();
void confirmFwStable();
bool hasOtaRollbackOccurred();
void clearOtaRollbackFlag();
bool performOta(const String& url, const String& targetVersion);

extern bool otaFailedFlag;
extern String otaFailedError;

