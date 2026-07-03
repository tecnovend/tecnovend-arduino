#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include "types.h"

// ============================================================================
// Comunicacion con la API: GET/POST, heartbeat, config y poll de pulsos.
// ============================================================================

void addApiKeyIfNeeded(HTTPClient& http);
bool httpGet(const String& url, String& response);
bool httpPostPulseResult(const Pulse& pulse, const String& status, const String& reason);
bool sendHeartbeat();
bool sendServiceHeartbeat(const String& reason, const String& affectedPulseId);
bool sendRemoteStatusLog();
bool fetchConfigOnce();
bool pollOnce();
