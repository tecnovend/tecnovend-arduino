#include "service.h"
#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "api.h"

bool rawMachineInService() {
  return digitalRead(INHIBIT_PIN) == HIGH;
}

bool machineInService() {
  return reportedInService;
}

bool canAcceptPulse() {
  return rawMachineInService() && reportedInService && !hasObservedSalePulse;
}

void clearObservedSalePulse() {
  hasObservedSalePulse = false;
  saleInhibitSeen = false;
  observedSalePulse.id = "";
  observedSalePulse.channel = 0;
  observedSalePulse.count = 0;
}

void updateInhibitState() {
  unsigned long now = millis();
  bool rawInService = rawMachineInService();

  if (rawInService) {
    inhibitLowActive = false;
    bootInhibitGraceActive = false;

    if (hasObservedSalePulse && saleInhibitSeen) {
      Serial.print("Venta finalizada normalmente. pulse_id=");
      Serial.println(observedSalePulse.id);
      clearObservedSalePulse();
    }

    if (hasObservedSalePulse && !saleInhibitSeen &&
        now - observedSaleStartedMs >= SALE_START_WAIT_MS) {
      Serial.println("No se detecto ciclo inhibit despues del pulso. Venta observada finalizada.");
      clearObservedSalePulse();
    }

    if (!reportedInService &&
        now - lastStateHeartbeatAttemptMs >= STATE_HEARTBEAT_RETRY_MS) {
      reportedInService = true;
      lastStateHeartbeatAttemptMs = now;
      sendServiceHeartbeat("recovered", "");
    }

    if (reportedInService && lastReportedInService != reportedInService &&
        now - lastStateHeartbeatAttemptMs >= STATE_HEARTBEAT_RETRY_MS) {
      lastStateHeartbeatAttemptMs = now;
      sendServiceHeartbeat("recovered", "");
    }

    return;
  }

  if (!inhibitLowActive) {
    inhibitLowActive = true;
    inhibitLowSinceMs = now;
  }

  if (hasObservedSalePulse && !saleInhibitSeen) {
    saleInhibitSeen = true;
    inhibitLowSinceMs = now;
    Serial.print("Inhibit activo durante venta. Esperando recuperacion de pulse_id=");
    Serial.println(observedSalePulse.id);
  }

  unsigned long graceMs = INHIBIT_OUT_OF_SERVICE_DEBOUNCE_MS;
  if (saleInhibitSeen) {
    graceMs = SALE_INHIBIT_GRACE_MS;
  } else if (bootInhibitGraceActive) {
    graceMs = BOOT_SERVICE_GRACE_MS;
  }

  if (reportedInService &&
      now - inhibitLowSinceMs >= graceMs &&
      now - lastStateHeartbeatAttemptMs >= STATE_HEARTBEAT_RETRY_MS) {
    String reason = saleInhibitSeen ? "sale_timeout" : "out_of_service";
    String affectedPulseId = saleInhibitSeen && hasObservedSalePulse ? observedSalePulse.id : "";

    reportedInService = false;
    lastStateHeartbeatAttemptMs = now;

    if (sendServiceHeartbeat(reason, affectedPulseId) && saleInhibitSeen) {
      Serial.print("Venta afectada informada como fuera de servicio: ");
      Serial.println(affectedPulseId);
      clearObservedSalePulse();
    }
  }

  if (!reportedInService && lastReportedInService != reportedInService &&
      now - lastStateHeartbeatAttemptMs >= STATE_HEARTBEAT_RETRY_MS) {
    String reason = saleInhibitSeen ? "sale_timeout" : "out_of_service";
    String affectedPulseId = saleInhibitSeen && hasObservedSalePulse ? observedSalePulse.id : "";
    lastStateHeartbeatAttemptMs = now;
    if (sendServiceHeartbeat(reason, affectedPulseId) && saleInhibitSeen) {
      clearObservedSalePulse();
    }
  }
}
