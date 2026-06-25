#pragma once

#include <Arduino.h>
#include "types.h"

// ============================================================================
// Ejecucion de pulsos hacia la maquina y cola de resultados con reintentos.
// ============================================================================

void queuePulseResult(const Pulse& pulse, const String& status, const String& reason);
bool reportPulseResultWithRetries(const Pulse& pulse, const String& status, const String& reason);
void retryPendingPulseResults();
void executePulse(const Pulse& pulse);
