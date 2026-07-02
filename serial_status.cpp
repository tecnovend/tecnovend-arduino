#include "serial_status.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include "config.h"
#include "globals.h"
#include "service.h"

const char* serialResetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "poweron";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "interrupt_wdt";
    case ESP_RST_TASK_WDT:
      return "task_wdt";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deepsleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    default:
      return "unknown";
  }
}

const char* serialCommunicationStateText() {
  switch (communicationState) {
    case COMM_NO_CONNECTION:
      return "no_connection";
    case COMM_API_REACHED:
      return "api_reached";
    case COMM_WEB_LINK_OK:
      return "web_link_ok";
    default:
      return "unknown";
  }
}

int pendingAckCount() {
  int count = 0;
  for (int i = 0; i < RESULT_QUEUE_SIZE; i++) {
    if (resultQueue[i].pending) {
      count++;
    }
  }
  return count;
}

void printSerialBootStatus() {
#if ENABLE_SERIAL_STATUS_LOG
  Serial.println();
  Serial.println("===== JAVOPOINT BOOT =====");
  Serial.print("fw=");
  Serial.println(FW_VERSION);
  Serial.print("arduino_id=");
  Serial.println(ARDUINO_ID);
  Serial.print("reset_reason=");
  Serial.print((int)esp_reset_reason());
  Serial.print(" ");
  Serial.println(serialResetReasonText());
  Serial.println("==========================");
#endif
}

void printSerialStatusIfDue() {
#if ENABLE_SERIAL_STATUS_LOG
  static unsigned long lastSerialStatusMs = 0;
  unsigned long now = millis();

  if (now - lastSerialStatusMs < SERIAL_STATUS_INTERVAL_MS) {
    return;
  }
  lastSerialStatusMs = now;

  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  bool rawService = rawMachineInService();

  Serial.print("[STATUS] uptime_s=");
  Serial.print(now / 1000);
  Serial.print(" fw=");
  Serial.print(FW_VERSION);
  Serial.print(" wifi=");
  Serial.print(wifiConnected ? "connected" : "off");
  Serial.print(" ssid=");
  Serial.print(wifiConnected ? WiFi.SSID() : currentWifiSsid);
  Serial.print(" rssi=");
  Serial.print(wifiConnected ? WiFi.RSSI() : 0);
  Serial.print(" web=");
  Serial.print(serialCommunicationStateText());
  Serial.print(" raw_inhibit=");
  Serial.print(rawService ? "service" : "out_of_service");
  Serial.print(" reported_service=");
  Serial.print(reportedInService ? "true" : "false");
  Serial.print(" startup_sent=");
  Serial.print(startupHeartbeatSent ? "true" : "false");
  Serial.print(" config_loaded=");
  Serial.print(configLoadedThisBoot ? "true" : "false");
  Serial.print(" pending_ack=");
  Serial.print(pendingAckCount());
  Serial.print(" netop=");
  Serial.print(currentNetworkOperation);
  Serial.print(" heap=");
  Serial.print(ESP.getFreeHeap());
  Serial.print(" reset=");
  Serial.println(serialResetReasonText());
#endif
}
