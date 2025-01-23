#ifndef WIFI_H
#define WIFI_H

#include <string>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Wi-Fi-related constants
#define WIFI_MAX_RETRY 3
#define WIFI_TIMEOUT_MS 15000

#define MAX_SCAN_RESULTS 10
#define SSID_MAX_LEN 32

#define SOCKET_MAX_RETRY 3
#define SOCKET_TIMEOUT_MS 5000

typedef struct
{
  char ssid[SSID_MAX_LEN + 1];
  int rssi;
  int auth_mode; // New field for authentication mode
} WifiScanResult;

extern WifiScanResult topScanResults[MAX_SCAN_RESULTS];
extern int scanResultCount;
extern QueueHandle_t outgoingMessageQueue;

void initWifi();
void disconnectAndForgetWifi();
bool sendMessage(const std::string &message);
void wifiTask(void *params);
void ledTask(void *params);
void socketTask(void *params);

#endif // WIFI_H
