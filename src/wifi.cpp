#include "wifi.h"
#include "constants.h"
#include "dhcpserver.h"
#include "httpserver.h"
#include "settings.h"
#include "control.h"
#include "ws2812.pio.h"

#include <cstdio>
#include <string>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "hardware/pio.h"

#include "lwip/apps/httpd.h"
#include "lwip/apps/mdns.h"
#include "lwip/sockets.h"
#include "lwipopts.h"

#include "pico/cyw43_arch.h"

#define STARTUP_BIT (1 << 0)
#define WIFI_CONNECTED_BIT (1 << 1)
#define WIFI_CONNECTION_FAILED_BIT (1 << 2)
#define CONFIGURED_BIT (1 << 3)
#define SOCKET_DISCONNECTED_BIT (1 << 4)

EventGroupHandle_t eventGroup;

WifiScanResult topScanResults[MAX_SCAN_RESULTS];
int scanResultCount = 0;
dhcp_server_t dhcpServer;

volatile bool isTestingConnection = false;
volatile bool isConnectedToWifi = false;
volatile bool cyw43IsInitialised = false;
volatile bool isAppModeActive = false;
volatile bool isStaModeActive = false;
volatile bool isSocketActive = false;
volatile bool isConnectedToSocketServer = false;

QueueHandle_t outgoingMessageQueue = NULL;
static int clientSocket = -1;
volatile static int socketRetryDelay = 5000;
volatile static int wifiRetryDelay = 1000;
volatile bool isFlashing = false;

PIO pio;
uint sm;

void sendSS2812Color(PIO pio, uint sm, uint32_t rgb_color)
{
  uint32_t encoded_color = ((rgb_color >> 8 & 0xFF) << 16) | ((rgb_color >> 16 & 0xFF) << 8) | (rgb_color & 0xFF); // GRB format
  pio_sm_put_blocking(pio, sm, encoded_color << 8 | 0x7);                                                          // 24 bits + 1 extra to meet reset condition
}

void printSettings(const volatile Settings *settings)
{
  printf("SSID: %s\nPassword: %s\nAuth Mode: %d\nMagic Number: %d\n",
         settings->ssid,
         settings->password,
         settings->authMode,
         settings->magic);
}

// Sort scan results by RSSI in descending order
void sortScanResultsByRSSI()
{
  for (int i = 0; i < scanResultCount - 1; i++)
  {
    for (int j = 0; j < scanResultCount - i - 1; j++)
    {
      if (topScanResults[j].rssi < topScanResults[j + 1].rssi)
      {
        WifiScanResult temp = topScanResults[j];
        topScanResults[j] = topScanResults[j + 1];
        topScanResults[j + 1] = temp;
      }
    }
  }
}

// Supported security types (filter others)
bool isSupportedSecurity(int auth_mode)
{
  // Allow networks that the Pico can actually connect to
  return auth_mode == 5 || // WPA with TKIP
         auth_mode == 7;   // WPA2 with AES (or mixed)
}

// Add or replace SSID in the top results (sorted by RSSI)
void addToTopResults(const char *ssid, int rssi, int auth_mode)
{
  // Ignore empty SSIDs
  if (strlen(ssid) == 0)
  {
    return;
  }

  // Check if the SSID already exists in the list
  for (int i = 0; i < scanResultCount; i++)
  {
    if (strcmp(topScanResults[i].ssid, ssid) == 0)
    {
      // Update RSSI and auth_mode if the new RSSI is stronger
      if (rssi > topScanResults[i].rssi)
      {
        topScanResults[i].rssi = rssi;
        topScanResults[i].auth_mode = auth_mode;
      }
      return;
    }
  }

  // If we haven't filled the list, add the SSID
  if (scanResultCount < MAX_SCAN_RESULTS)
  {
    strncpy(topScanResults[scanResultCount].ssid, ssid, SSID_MAX_LEN);
    topScanResults[scanResultCount].ssid[SSID_MAX_LEN] = '\0'; // Null-terminate
    topScanResults[scanResultCount].rssi = rssi;
    topScanResults[scanResultCount].auth_mode = auth_mode;
    scanResultCount++;
  }
  else
  {
    // Replace the weakest entry if the new one is stronger
    int weakestIndex = 0;
    for (int i = 1; i < MAX_SCAN_RESULTS; i++)
    {
      if (topScanResults[i].rssi < topScanResults[weakestIndex].rssi)
      {
        weakestIndex = i;
      }
    }

    if (rssi > topScanResults[weakestIndex].rssi)
    {
      strncpy(topScanResults[weakestIndex].ssid, ssid, SSID_MAX_LEN);
      topScanResults[weakestIndex].ssid[SSID_MAX_LEN] = '\0';
      topScanResults[weakestIndex].rssi = rssi;
      topScanResults[weakestIndex].auth_mode = auth_mode;
    }
  }
}

int wifiScanCallback(void *env, const cyw43_ev_scan_result_t *result)
{
  if (result)
  {
    // Convert SSID and check supported security
    if (isSupportedSecurity(result->auth_mode))
    {
      addToTopResults(reinterpret_cast<const char *>(result->ssid), result->rssi, result->auth_mode);
    }
  }
  else
  {
    printf("Scan complete\n");
  }
  return 0;
}

// Function to perform Wi-Fi scan
bool performWifiScan()
{
  printf("Starting Wi-Fi scan...\n");
  scanResultCount = 0; // Reset the results buffer

  cyw43_wifi_scan_options_t scanOptions = {0};
  int result = cyw43_wifi_scan(&cyw43_state, &scanOptions, NULL, wifiScanCallback);

  if (result != 0)
  {
    printf("Wi-Fi scan failed with error code: %d\n", result);
    return false;
  }

  // Wait for the scan to complete
  while (cyw43_wifi_scan_active(&cyw43_state))
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  sortScanResultsByRSSI();

  return true;
}

// Function to print scan results (for debugging)
void printScanResults()
{
  printf("Wi-Fi scan results:\n");
  if (scanResultCount == 0)
  {
    printf("No results found.\n");
    return;
  }
  for (int i = 0; i < scanResultCount; i++)
  {
    printf("SSID: %s, RSSI: %d, Auth Mode: %d\n", topScanResults[i].ssid, topScanResults[i].rssi, topScanResults[i].auth_mode);
  }
}

int mapAuthMode(int input)
{
  switch (input)
  {
  case 0:
    return CYW43_AUTH_OPEN;
  case 1:
    return CYW43_AUTH_WPA_TKIP_PSK;
  case 7:
    return CYW43_AUTH_WPA2_AES_PSK; // Observed correspondence
  case 3:
    return CYW43_AUTH_WPA2_MIXED_PSK;
  case 4:
    return CYW43_AUTH_WPA3_SAE_AES_PSK;
  case 5:
    return CYW43_AUTH_WPA3_WPA2_AES_PSK;
  default:
    printf("Unknown auth mode: %d. Defaulting to open.\n", input);
    return CYW43_AUTH_OPEN;
  }
}

void checkWifiConnection()
{
  if (!cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA))
  {
    printf("WiFi connection dropped.\n");
    // Attempt reconnection or other handling logic
    isConnectedToWifi = false;
  }
  else
  {
    isConnectedToWifi = true;
  }
}

void initArch()
{
  if (!cyw43IsInitialised)
  {
    if (cyw43_arch_init())
    {
      printf("Failed to initialize Wi-Fi module.\n");
      return;
    }
    printf("Wi-Fi module initialized successfully.\n");
    cyw43IsInitialised = true;
  }
}

bool connectToWiFi(const char *ssid, const char *password, int auth_mode)
{
  printf("Using credentials SSID: %s Password: %s Auth Mode: %d\n", ssid, password, auth_mode);

  if (!isTestingConnection && !isConnectedToWifi)
  {
    isTestingConnection = true;
    const int timeoutMs = 30000; // Increased timeout
    const int retryDelayMs = 2000;

    for (int attempt = 1; attempt <= WIFI_MAX_RETRY; ++attempt)
    {
      printf("Attempting to connect to SSID: %s (Attempt %d/%d)\n", ssid, attempt, WIFI_MAX_RETRY);

      int result = cyw43_arch_wifi_connect_timeout_ms(ssid, password, mapAuthMode(auth_mode), timeoutMs);

      if (result == 0)
      {
        printf("Successfully connected to Wi-Fi.\n");
        isConnectedToWifi = true;
        isTestingConnection = false;
        return true;
      }
      else
      {
        printf("Failed to connect to Wi-Fi (Error %d). Retrying...\n", result);
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        vTaskDelay(pdMS_TO_TICKS(retryDelayMs));
      }
    }

    printf("Failed to connect to Wi-Fi after %d attempts.\n", WIFI_MAX_RETRY);
    isTestingConnection = false;
  }
  return false;
}

void disconnectWiFi()
{
  if (isConnectedToWifi)
  {
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    printf("DisConnectedToWifi from Wi-Fi.\n");
    isConnectedToWifi = false;
  }
}

bool hasCredentials()
{
  if (strlen((const char *)currentSettings.ssid) > 0 && strlen((const char *)currentSettings.password) > 0)
  {
    return true;
  }
  return false;
}

void disconnectAndForgetWifi()
{
  requestSettingsReset();
  vTaskDelay(pdMS_TO_TICKS(500));
  disconnectWiFi();
}

bool connectToWifiWithCredentials()
{
  printSettings(&currentSettings);
  return connectToWiFi((const char *)currentSettings.ssid, (const char *)currentSettings.password, currentSettings.authMode);
}

bool sendMessage(const std::string &message)
{
  if (outgoingMessageQueue == NULL)
  {
    printf("Message queue is not initialized.\n");
    return false;
  }

  if (xQueueSend(outgoingMessageQueue, &message, pdMS_TO_TICKS(100)) == pdPASS)
  {
    printf("Message queued: %s\n", message.c_str());
    return true;
  }
  else
  {
    printf("Failed to queue message: %s\n", message.c_str());
    return false;
  }
}

void initSocket()
{
  if (!isSocketActive)
  {
    Message msg;
    // flush queues
    while (xQueueReceive(incommingMessageQueue, &msg, 0) == pdTRUE)
      ;
    while (xQueueReceive(outgoingMessageQueue, &msg, 0) == pdTRUE)
      ;
    xTaskCreate(socketTask, "SocketTask", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    isSocketActive = true;
  }
}

void deInitSocket()
{
  if (isSocketActive)
  {
    Message msg;
    // flush queues
    while (xQueueReceive(incommingMessageQueue, &msg, 0) == pdTRUE)
      ;
    while (xQueueReceive(outgoingMessageQueue, &msg, 0) == pdTRUE)
      ;
    isSocketActive = false;
  }
}

void initAPMode()
{
  if (!isAppModeActive)
  {
    cyw43_arch_enable_ap_mode(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
    printf("AP mode enabled.\n");

    // Initialize DHCP server
    ip_addr_t ip, nm;
    IP4_ADDR(ip_2_ip4(&ip), 192, 168, 4, 1);   // Set AP's IP address
    IP4_ADDR(ip_2_ip4(&nm), 255, 255, 255, 0); // Set subnet mask
    dhcp_server_init(&dhcpServer, &ip, &nm);

    auto ip_addr = cyw43_state.netif[CYW43_ITF_AP].ip_addr.addr;
    printf("Pico W IP Address: %lu.%lu.%lu.%lu\n", ip_addr & 0xFF, (ip_addr >> 8) & 0xFF, (ip_addr >> 16) & 0xFF, ip_addr >> 24);

    // Start HTTP server for Wi-Fi configuration
    startHttpServer();
    isAppModeActive = true;
  }
}

void deInitAPMode()
{
  if (isAppModeActive)
  {
    // Stop HTTP server and DHCP server
    stopHttpServer();
    dhcp_server_deinit(&dhcpServer);
    cyw43_arch_disable_ap_mode();
    printf("AP mode dis-abled.\n");
    isAppModeActive = false;
  }
}

void initSTAMode()
{
  if (!isStaModeActive)
  {
    cyw43_arch_enable_sta_mode();
    isStaModeActive = true;
  }
}

void deInitSTAMode()
{
  if (isStaModeActive)
  {
    cyw43_arch_disable_sta_mode();
    isStaModeActive = false;
  }
}

void credentialsTask(void *params)
{
  while (true)
  {
    if (hasCredentials())
    {
      xEventGroupSetBits(eventGroup, CONFIGURED_BIT);
      vTaskDelete(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void socketTask(void *params)
{
  struct sockaddr_in serverAddr;
  clientSocket = lwip_socket(AF_INET, SOCK_STREAM, 0);
  if (clientSocket < 0)
  {
    printf("Failed to create socket.\n");
    xEventGroupSetBits(eventGroup, SOCKET_DISCONNECTED_BIT);
    vTaskDelete(NULL);
    return;
  }

  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(SOCKET_SERVER_PORT);
  inet_aton(SOCKET_SERVER_IP, &serverAddr.sin_addr);

  if (lwip_connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
  {
    printf("Failed to connect to server.\n");
    lwip_close(clientSocket);
    clientSocket = -1;
    xEventGroupSetBits(eventGroup, SOCKET_DISCONNECTED_BIT);
    vTaskDelete(NULL);
    return;
  }

  printf("Connected to server.\n");
  socketRetryDelay = 5000;
  char buffer[1024];

  while (true)
  {
    // Check for incoming messages
    int bytesRead = lwip_recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead > 0)
    {
      buffer[bytesRead] = '\0';
      printf("Received: %s\n", buffer);

      Message msg;
      if (bufferToMessage(buffer, msg))
      {
        if (xQueueSend(incommingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
        {
          printf("Failed to enqueue info message.\n");
        }
      }
      else
      {
        printf("Failed to convert buffer to Message\n");
      }
    }
    else if (bytesRead < 0)
    {
      printf("Error reading from socket. Closing connection.\n");
      break;
    }

    // Check for outgoing messages
    Message msg;
    if (xQueueReceive(outgoingMessageQueue, &msg, 0)) // Non-blocking check for messages
    {
      std::string messageString = messageToString(msg);
      if (lwip_send(clientSocket, messageString.c_str(), messageString.length(), 0) < 0)
      {
        printf("Failed to send message: %s\n", messageString.c_str());
        break;
      }
    }

    // Yield CPU to other tasks
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  printf("Socket task shutting down.\n");
  lwip_close(clientSocket);
  clientSocket = -1;
  xEventGroupSetBits(eventGroup, SOCKET_DISCONNECTED_BIT);
  vTaskDelete(NULL);
}

void handleStartup()
{
  initSTAMode();
  if (hasCredentials() && connectToWifiWithCredentials())
  {
    printf("Connected to Wi-Fi!\n");
    xEventGroupSetBits(eventGroup, WIFI_CONNECTED_BIT);
  }
  else
  {
    // No valid credentials, switch to AP mode
    printf("No valid Wi-Fi credentials found. Switching to AP mode...\n");
    performWifiScan();
    printScanResults();
    xEventGroupSetBits(eventGroup, WIFI_CONNECTION_FAILED_BIT);
  }
}

void handleWifiFailed()
{
  deInitSTAMode();
  initAPMode();
  xTaskCreate(credentialsTask, "CredentialsTask", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
}

void handleConfigured()
{
  deInitAPMode();
  initSTAMode();
  if (connectToWifiWithCredentials())
  {
    xEventGroupSetBits(eventGroup, WIFI_CONNECTED_BIT);
  }
  else
  {
    requestSettingsReset();
    vTaskDelay(pdMS_TO_TICKS(500));
    xEventGroupSetBits(eventGroup, WIFI_CONNECTION_FAILED_BIT);
  }
}

void handleWifiConnected()
{
  initSocket();
}

void handleSocketDisconnected()
{
  printf("Deinit socket\n");
  deInitSocket();
  printf("Checking Wifi\n");
  checkWifiConnection();
  if (!isConnectedToWifi)
  {
    printf("Wifi disconnected\n");
    xEventGroupSetBits(eventGroup, STARTUP_BIT);
  }
  else
  {
    printf("Waiting\n");
    vTaskDelay(pdMS_TO_TICKS(socketRetryDelay));
    if (socketRetryDelay < 120000)
    {
      socketRetryDelay += 5000;
    }
    printf("Re initialising\n");
    xEventGroupSetBits(eventGroup, WIFI_CONNECTED_BIT);
  }
}

void updateLedColor()
{
  if (isConnectedToSocketServer)
  {
    sendSS2812Color(pio, sm, 0x00FF00); // Green
  }
  else if (isConnectedToWifi)
  {
    sendSS2812Color(pio, sm, 0xFFFF00); // Orange
  }
  else if (isTestingConnection)
  {
    sendSS2812Color(pio, sm, isFlashing ? 0x000000 : 0xFFFF00); // Flashing Orange
    isFlashing = !isFlashing;
  }
  else if (isAppModeActive)
  {
    sendSS2812Color(pio, sm, 0x0000FF); // Blue
  }
  else
  {
    sendSS2812Color(pio, sm, 0xFFFFFF); // White
  }
}

void initWifi()
{

  pio = pio0;

  // Load the WS2812 program into the PIO memory
  uint offset = pio_add_program(pio, &ws2812_program);

  // Configure the PIO state machine
  pio_sm_config c = ws2812_program_get_default_config(offset);
  sm_config_set_out_pins(&c, WS2812_GPIO, 1);  // Set output pin
  sm_config_set_sideset_pins(&c, WS2812_GPIO); // Set side-set pin (optional, only if used)
  sm_config_set_clkdiv(&c, 2.0f);              // Set clock divider to adjust data rate

  sm = pio_claim_unused_sm(pio, true); // Claim a state machine
  pio_sm_init(pio, sm, offset, &c);    // Initialize the state machine with configuration
  pio_sm_set_enabled(pio, sm, true);   // Enable the state machine

  eventGroup = xEventGroupCreate();
  incommingMessageQueue = xQueueCreate(5, sizeof(Message));
  if (incommingMessageQueue == NULL)
  {
    printf("Failed to create incoming message queue.\n");
  }
  outgoingMessageQueue = xQueueCreate(5, sizeof(Message));
  if (outgoingMessageQueue == NULL)
  {
    printf("Failed to create outgoing message queue.\n");
  }
}

void wifiTask(void *params)
{
  printf("Wi-Fi Task started.\n");
  initArch();
  xEventGroupSetBits(eventGroup, STARTUP_BIT);
  while (true)
  {
    EventBits_t bits = xEventGroupWaitBits(
        eventGroup,
        STARTUP_BIT | WIFI_CONNECTED_BIT | WIFI_CONNECTION_FAILED_BIT | CONFIGURED_BIT | SOCKET_DISCONNECTED_BIT,
        pdTRUE,         // Clear on exit
        pdFALSE,        // Wait for any bit
        portMAX_DELAY); // Wait indefinitely

    if (bits & STARTUP_BIT)
    {
      printf("HANDLE STARTUP\n");
      handleStartup();
    }

    if (bits & WIFI_CONNECTION_FAILED_BIT)
    {
      printf("HANDLE WIFI FAILED\n");
      handleWifiFailed();
    }

    if (bits & CONFIGURED_BIT)
    {
      printf("HANDLE CONFIGURED\n");
      handleConfigured();
    }

    if (bits & WIFI_CONNECTED_BIT)
    {
      printf("HANDLE WIFI CONNECTED\n");
      handleWifiConnected();
    }

    if (bits & SOCKET_DISCONNECTED_BIT)
    {
      printf("HANDLE SOCKET DISCONNECTED\n");
      handleSocketDisconnected();
    }
  }
}

void ledTask(void *params)
{
  while (1)
  {
    updateLedColor();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}