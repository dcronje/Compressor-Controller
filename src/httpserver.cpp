// http_server.cpp
#include "httpserver.h"
#include "fsdata.h"
#include <string.h>
#include <stdio.h>
#include <sstream>
#include "wifi.h"
#include "cJSON.h"
#include "settings.h"

static struct tcp_pcb *http_pcb = NULL;

static std::string fullRequest;

static err_t handle_post_request(struct tcp_pcb *pcb, const char *request)
{
  char *body = strstr(request, "\r\n\r\n");
  if (body)
  {
    body += 4; // Move past the "\r\n\r\n" delimiter
    printf("Received POST data: %s\n", body);

    // Parse JSON
    cJSON *json = cJSON_Parse(body);
    if (json)
    {
      // Extract fields
      cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
      cJSON *password = cJSON_GetObjectItem(json, "password");
      cJSON *authMode = cJSON_GetObjectItem(json, "authMode");

      if (cJSON_IsString(ssid) && cJSON_IsString(password) && cJSON_IsNumber(authMode))
      {
        // Save to settings
        strncpy((char *)currentSettings.ssid, ssid->valuestring, sizeof(currentSettings.ssid) - 1);
        currentSettings.ssid[sizeof(currentSettings.ssid) - 1] = '\0';

        strncpy((char *)currentSettings.password, password->valuestring, sizeof(currentSettings.password) - 1);
        currentSettings.password[sizeof(currentSettings.password) - 1] = '\0';

        currentSettings.authMode = authMode->valueint;

        // Trigger a settings validation to save the updated settings
        requestSettingsValidation();

        printf("Saved credentials: SSID='%s', Password='%s', AuthMode=%d\n",
               currentSettings.ssid, currentSettings.password, currentSettings.authMode);
      }
      else
      {
        printf("Invalid JSON fields.\n");
      }

      cJSON_Delete(json); // Free memory allocated for JSON parsing
    }
    else
    {
      printf("Failed to parse JSON.\n");
    }
    const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    tcp_write(pcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
  }
  else
  {
    printf("Incomplete POST request received.\n");
  }
  return ERR_OK;
}

static const char *generateScanResultsJson()
{
  static char jsonBuffer[2048]; // Ensure it's large enough for your scan results
  size_t offset = 0;

  offset += snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset, "[");

  for (int i = 0; i < scanResultCount; ++i)
  {
    const WifiScanResult &network = topScanResults[i];
    int written = snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset,
                           "{\"ssid\":\"%s\",\"rssi\":%d,\"authMode\":%d}",
                           network.ssid, network.rssi, network.auth_mode);

    // Ensure we don't overflow the buffer
    if (written < 0 || offset + written >= sizeof(jsonBuffer))
    {
      break;
    }

    offset += written;

    // Add a comma after all but the last entry
    if (i < scanResultCount - 1)
    {
      written = snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset, ",");
      if (written < 0 || offset + written >= sizeof(jsonBuffer))
      {
        break;
      }
      offset += written;
    }
  }

  snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset, "]");

  // Null-terminate in case of truncation
  jsonBuffer[sizeof(jsonBuffer) - 1] = '\0';

  printf("Generated JSON: %s\n", jsonBuffer);

  return jsonBuffer;
}

static err_t http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
  if (!p)
  {
    tcp_close(pcb);
    return ERR_OK;
  }

  char *request = (char *)p->payload;

  if (strncmp(request, "GET /scan.json", 14) == 0)
  {
    printf("HIT SCAN!\n");
    const char *jsonResponse = generateScanResultsJson();
    char header[128];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", strlen(jsonResponse));
    tcp_write(pcb, header, strlen(header), TCP_WRITE_FLAG_COPY);
    tcp_write(pcb, jsonResponse, strlen(jsonResponse), TCP_WRITE_FLAG_COPY);
  }
  else if (strncmp(request, "GET /", 5) == 0)
  {
    char header[256];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             strlen(index_html_markup));
    tcp_write(pcb, header, strlen(header), TCP_WRITE_FLAG_COPY);
    tcp_write(pcb, index_html_markup, strlen(index_html_markup), TCP_WRITE_FLAG_COPY);
  }
  else if (strncmp(request, "POST /configure", 15) == 0)
  {
    printf("IN CONFIGURE\n");
    if (!p)
    {
      tcp_close(pcb);
      return ERR_OK;
    }

    // Append received data to the fullRequest buffer
    fullRequest.append(static_cast<const char *>(p->payload), p->len);

    // Check if the request is complete (headers and body separated by \r\n\r\n)
    size_t headerEnd = fullRequest.find("\r\n\r\n");
    if (headerEnd != std::string::npos)
    {
      // Process the complete request
      handle_post_request(pcb, fullRequest.c_str());

      // Clear the buffer for the next request
      fullRequest.clear();
    }
    else
    {
      // Wait for more data
      return ERR_OK;
    }
  }

  tcp_recved(pcb, p->len);
  pbuf_free(p);
  return ERR_OK;
}

static err_t http_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
  printf("IN http_accept\n");
  tcp_recv(pcb, http_recv);
  return ERR_OK;
}

void startHttpServer()
{
  http_pcb = tcp_new();
  if (!http_pcb)
  {
    printf("Failed to create HTTP PCB\n");
    return;
  }
  tcp_bind(http_pcb, IP_ADDR_ANY, 80);
  http_pcb = tcp_listen(http_pcb);
  tcp_accept(http_pcb, http_accept);
  printf("HTTP server started\n");
}

void stopHttpServer()
{
  if (http_pcb)
  {
    tcp_close(http_pcb);
    http_pcb = NULL;
    printf("HTTP server stopped\n");
  }
}
