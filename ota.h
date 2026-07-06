#pragma once
#include <Arduino.h>

void setupOta();
void confirmFwStable();
bool hasOtaRollbackOccurred();
void clearOtaRollbackFlag();
void checkOtaTimeout();
bool performOta(const String& url, const String& targetVersion);

extern bool otaFailedFlag;
extern String otaFailedError;

