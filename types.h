#pragma once

#include <Arduino.h>

// ============================================================================
// Tipos compartidos del firmware.
// ============================================================================

enum CommunicationState {
  COMM_NO_CONNECTION,
  COMM_API_REACHED,
  COMM_WEB_LINK_OK
};

struct Pulse {
  String id;
  int channel;
  int count;
};

struct PulseResult {
  bool pending;
  String id;
  int channel;
  int count;
  String status;
  String reason;
  int attempts;
  unsigned long nextAttemptMs;
};
