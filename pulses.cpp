#include "pulses.h"
#include "config.h"
#include "globals.h"
#include "led.h"
#include "api.h"

void queuePulseResult(const Pulse& pulse, const String& status, const String& reason) {
  int slot = -1;

  for (int i = 0; i < RESULT_QUEUE_SIZE; i++) {
    if (resultQueue[i].pending && resultQueue[i].id == pulse.id) {
      slot = i;
      break;
    }
    if (!resultQueue[i].pending && slot < 0) {
      slot = i;
    }
  }

  if (slot < 0) {
    Serial.println("Cola de resultados llena. No se pudo guardar resultado pendiente.");
    return;
  }

  resultQueue[slot].pending = true;
  resultQueue[slot].id = pulse.id;
  resultQueue[slot].channel = pulse.channel;
  resultQueue[slot].count = pulse.count;
  resultQueue[slot].status = status;
  resultQueue[slot].reason = reason;
  resultQueue[slot].attempts = 0;
  resultQueue[slot].nextAttemptMs = millis() + RESULT_RETRY_INTERVAL_MS;
}

bool reportPulseResultWithRetries(const Pulse& pulse, const String& status, const String& reason) {
  for (int attempt = 1; attempt <= 3; attempt++) {
    if (httpPostPulseResult(pulse, status, reason)) {
      return true;
    }

    Serial.print("Reintentando resultado de pulso ");
    Serial.print(attempt);
    Serial.println("/3");
    delay(250);
  }

  queuePulseResult(pulse, status, reason);
  return false;
}

void retryPendingPulseResults() {
  unsigned long now = millis();

  for (int i = 0; i < RESULT_QUEUE_SIZE; i++) {
    if (!resultQueue[i].pending || now < resultQueue[i].nextAttemptMs) {
      continue;
    }

    Pulse pulse;
    pulse.id = resultQueue[i].id;
    pulse.channel = resultQueue[i].channel;
    pulse.count = resultQueue[i].count;

    resultQueue[i].attempts++;
    if (httpPostPulseResult(pulse, resultQueue[i].status, resultQueue[i].reason)) {
      resultQueue[i].pending = false;
      continue;
    }

    resultQueue[i].nextAttemptMs = millis() + RESULT_RETRY_INTERVAL_MS;
  }
}

void executePulse(const Pulse& pulse) {
  Serial.print("Ejecutando pulse_id=");
  Serial.print(pulse.id);
  Serial.print(" channel=");
  Serial.print(pulse.channel);
  Serial.print(" count=");
  Serial.println(pulse.count);

  for (int i = 0; i < pulse.count; i++) {
    setLed(true);
    digitalWrite(PULSE_PIN, HIGH);
    delay(pulseHighMs);
    digitalWrite(PULSE_PIN, LOW);
    setLed(false);

    if (i + 1 < pulse.count) {
      delay(pulseLowMs);
    }
  }
}
