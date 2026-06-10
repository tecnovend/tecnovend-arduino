#pragma once

// ============================================================================
// Estado de servicio de la maquina (pin INHIBIT) y seguimiento de ventas.
// ============================================================================

bool rawMachineInService();
bool machineInService();
bool canAcceptPulse();
void clearObservedSalePulse();
void updateInhibitState();
