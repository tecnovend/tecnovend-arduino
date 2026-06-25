#include "globals.h"

// ============================================================================
// Definiciones del estado mutable compartido (ver globals.h).
// ============================================================================

Preferences preferences;

unsigned long lastPollMs = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastStartupHeartbeatAttemptMs = 0;
unsigned long lastStateHeartbeatAttemptMs = 0;
unsigned long lastConfigAttemptMs = 0;

bool lastReportedInService = true;
bool reportedInService = true;
bool inhibitLowActive = false;
bool bootInhibitGraceActive = false;
unsigned long inhibitLowSinceMs = 0;
unsigned long wifiResetLowSinceMs = 0;
bool wifiResetLatched = false;
bool skipConfigThisBoot = false;
bool configLoadedThisBoot = false;
bool startupHeartbeatSent = false;

int pulseValue = 1;
unsigned long pulseHighMs = 175;
unsigned long pulseLowMs = 250;

String machineId = "";
String currentWifiSsid;
String currentWifiPass;
bool usingDefaultWifi = true;
bool configPortalActive = false;

CommunicationState communicationState = COMM_NO_CONNECTION;

PulseResult resultQueue[RESULT_QUEUE_SIZE];
Pulse observedSalePulse;
bool hasObservedSalePulse = false;
bool saleInhibitSeen = false;
unsigned long observedSaleStartedMs = 0;
String heartbeatReason = "";
String heartbeatAffectedPulseId = "";
