#pragma once

#include <Arduino.h>
#include "config.h"
#include <Preferences.h>
#include "types.h"

// ============================================================================
// Estado mutable compartido entre modulos.
// Las definiciones (memoria real) viven en globals.cpp.
// ============================================================================

extern Preferences preferences;

extern unsigned long lastPollMs;
extern unsigned long lastHeartbeatMs;
extern unsigned long lastStartupHeartbeatAttemptMs;
extern unsigned long lastStateHeartbeatAttemptMs;
extern unsigned long lastConfigAttemptMs;
extern unsigned long lastNetworkOkMs;
extern unsigned long lastNetworkRecoveryMs;
extern unsigned long lastRemoteStatusLogMs;
extern int consecutiveNetworkFailures;

extern bool lastReportedInService;
extern bool reportedInService;
extern bool inhibitLowActive;
extern bool bootInhibitGraceActive;
extern unsigned long inhibitLowSinceMs;
extern unsigned long wifiResetLowSinceMs;
extern bool wifiResetLatched;
extern bool skipConfigThisBoot;
extern bool configLoadedThisBoot;
extern bool startupHeartbeatSent;

extern int pulseValue;
extern unsigned long pulseHighMs;
extern unsigned long pulseLowMs;

extern String machineId;
extern String currentWifiSsid;
extern String currentWifiPass;
extern bool usingDefaultWifi;
extern bool configPortalActive;

extern CommunicationState communicationState;

extern PulseResult resultQueue[RESULT_QUEUE_SIZE];
extern Pulse observedSalePulse;
extern bool hasObservedSalePulse;
extern bool saleInhibitSeen;
extern unsigned long observedSaleStartedMs;
extern String heartbeatReason;
extern String heartbeatAffectedPulseId;
extern String currentNetworkOperation;
