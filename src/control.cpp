#include "control.h"
#include "constants.h"
#include "wifi.h"
#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <cstring>

#include "cJSON.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "timers.h"

// TODO: pressure drops more than 5 mins sound alarm, pressure drops more than 10 mins shut down compressor
// motor runs for more than 2 mins sound alarms and shut down comrpresor
// must be settable

// Queue handles
QueueHandle_t incommingMessageQueue = NULL;
QueueHandle_t interactionQueue = NULL;

TimerHandle_t longPressTimer = NULL;

TimerHandle_t compressionTimer;
TimerHandle_t supplyTimer;
TimerHandle_t motorTimer;

TimerHandle_t compressionWatchTimer;
TimerHandle_t supplyWatchTimer;
TimerHandle_t motorWatchTimer;

volatile static int compressionTimeElapsed = 0;
volatile static int supplyTimeElapsed = 0;
volatile static int motorTimeElapsed = 0;

volatile int32_t shutDownButtonDown = 0;

static uint32_t buttonPressStartTime = 0;
static bool longPressHandled = false;

typedef enum
{
  SHUT_DOWN,
  FORGET_WIFI,
} Interaction;

void handleWatchTimerChange(TimerHandle_t xTimer)
{
  if (xTimer == compressionWatchTimer)
  {
    compressionTimeElapsed++;
    sendCompressionCountdownUpdatedInfo(currentSettings.compressionTimeout - compressionTimeElapsed);
  }
  else if (xTimer == supplyWatchTimer)
  {
    supplyTimeElapsed++;
    sendSupplyCountdownUpdatedInfo(currentSettings.supplyTimeout - supplyTimeElapsed);
  }
  else if (xTimer == motorWatchTimer)
  {
    motorTimeElapsed++;
    sendMotorCountdownUpdatedInfo(currentSettings.motorTimeout - motorTimeElapsed);
  }
}

void handleTimerReached(TimerHandle_t xTimer)
{

  if (xTimer == compressionTimer)
  {
    printf("Timer expired\n");
    sendCompressionCountdownUpdatedInfo(0);
    sendCompressionCountdownEndInfo();
    handleSupplyAndOff();
    if (xTimerStop(compressionWatchTimer, 0) != pdPASS)
    {
      printf("Failed to stop watch timer \n");
    }
    compressionTimeElapsed = 0;
  }
  else if (xTimer == supplyTimer)
  {
    printf("Timer expired\n");
    sendSupplyCountdownUpdatedInfo(0);
    sendSupplyCountdownEndInfo();
    handleSupplyAndOff();
    if (xTimerStop(supplyWatchTimer, 0) != pdPASS)
    {
      printf("Failed to stop watch timer \n");
    }
    supplyTimeElapsed = 0;
  }
  else if (xTimer == motorTimer)
  {
    printf("Timer expired\n");
    sendMotorCountdownUpdatedInfo(0);
    sendMotorCountdownEndInfo();
    handleSupplyAndOff();
    if (xTimerStop(motorTimer, 0) != pdPASS)
    {
      printf("Failed to stop watch timer \n");
    }
    motorTimeElapsed = 0;
  }
}

void changeTimerPreiod(TimerHandle_t xTimer, TickType_t newPeriod)
{
  if (xTimer == compressionTimer)
  {
    sendCompressionCountdownUpdatedInfo(newPeriod);
    if (xTimerReset(compressionWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    compressionTimeElapsed = 0;
  }
  else if (xTimer == supplyTimer)
  {
    sendCompressionCountdownUpdatedInfo(newPeriod);
    if (xTimerReset(supplyWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    supplyTimeElapsed = 0;
  }
  else if (xTimer == motorTimer)
  {
    sendCompressionCountdownUpdatedInfo(newPeriod);
    if (xTimerReset(motorWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    motorTimeElapsed = 0;
  }
  if (xTimerChangePeriod(xTimer, pdMS_TO_TICKS(newPeriod * 1000 * 60), 0) != pdPASS)
  {
    // Failed to change the period
    printf("Failed to change shutdown timer \n");
  }
}

void startTimer(TimerHandle_t xTimer)
{
  if (xTimer == compressionTimer)
  {
    sendCompressionCountdownUpdatedInfo(currentSettings.compressionTimeout);
    if (xTimerStart(compressionWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    compressionTimeElapsed = 0;
  }
  else if (xTimer == supplyTimer)
  {
    sendCompressionCountdownUpdatedInfo(currentSettings.supplyTimeout);
    if (xTimerStart(supplyWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    supplyTimeElapsed = 0;
  }
  else if (xTimer == motorTimer)
  {
    sendCompressionCountdownUpdatedInfo(currentSettings.motorTimeout);
    if (xTimerStart(motorWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    motorTimeElapsed = 0;
  }
  if (xTimerStart(xTimer, 0) != pdPASS)
  {
    // Failed to start the timer
    printf("Failed to start shutdown timer \n");
  }
}

void stopTimer(TimerHandle_t xTimer)
{
  if (xTimer == compressionTimer)
  {
    sendCompressionCountdownUpdatedInfo(0);
    if (xTimerStop(compressionWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    compressionTimeElapsed = 0;
  }
  else if (xTimer == supplyTimer)
  {
    sendCompressionCountdownUpdatedInfo(0);
    if (xTimerStop(supplyWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    supplyTimeElapsed = 0;
  }
  else if (xTimer == motorTimer)
  {
    sendCompressionCountdownUpdatedInfo(0);
    if (xTimerStop(motorWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    motorTimeElapsed = 0;
  }
  if (xTimerStop(xTimer, 0) != pdPASS)
  {
    printf("Failed to stop watch timer \n");
  }
}

void handleRestartTimer(TimerHandle_t xTimer)
{
  if (xTimer == compressionTimer)
  {
    sendCompressionCountdownUpdatedInfo(currentSettings.compressionTimeout);
    if (xTimerReset(compressionWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    compressionTimeElapsed = 0;
  }
  else if (xTimer == supplyTimer)
  {
    sendCompressionCountdownUpdatedInfo(currentSettings.supplyTimeout);
    if (xTimerReset(supplyWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    supplyTimeElapsed = 0;
  }
  else if (xTimer == motorTimer)
  {
    sendCompressionCountdownUpdatedInfo(currentSettings.motorTimeout);
    if (xTimerReset(motorWatchTimer, 0) != pdPASS)
    {
      printf("Failed to reset watch timer \n");
    }
    motorTimeElapsed = 0;
  }
  if (xTimerReset(xTimer, 0) != pdPASS)
  {
    printf("Failed to restart watch timer \n");
  }
  compressionTimeElapsed = 0;
}

void longPressCallback(TimerHandle_t xTimer)
{
  // TODO:
  // printf("SHUT DOWN BUTTON PRESSED\n");
  // Interaction interaction = Interaction::SHUT_DOWN;
  // xQueueSendToBackFromISR(interactionQueue, &interaction, NULL);
}

void handleButtonISR(uint gpio, uint32_t events)
{
  if (gpio == SHUT_DOWN_BUTTON_GPIO)
  {
    if (events & GPIO_IRQ_EDGE_FALL && shutDownButtonDown == 0)
    {
      printf("SHUT DOWN BUTTON DOWN\n");
      shutDownButtonDown = 1;
      buttonPressStartTime = xTaskGetTickCount(); // Get current tick count as the start time
      longPressHandled = false;
      xTimerStartFromISR(longPressTimer, 0);
    }
    else if (events & GPIO_IRQ_EDGE_RISE && shutDownButtonDown == 1)
    {
      printf("SHUT DOWN BUTTON UP\n");
      shutDownButtonDown = 0;
      xTimerStopFromISR(longPressTimer, 0);
      if (!longPressHandled)
      {
        printf("SHUT DOWN BUTTON PRESSED\n");
        Interaction interaction = Interaction::SHUT_DOWN;
        xQueueSendToBackFromISR(interactionQueue, &interaction, NULL);
      }
    }
  }
  else if (events & GPIO_IRQ_EDGE_FALL && gpio == FORGET_WIFI_BUTTON_GPIO)
  {
    printf("FORGET WIFI BUTTON PRESSED\n");
    Interaction interaction = Interaction::FORGET_WIFI;
    xQueueSendToBackFromISR(interactionQueue, &interaction, NULL);
  }
}

void sharedISR(uint gpio, uint32_t events)
{
  handleButtonISR(gpio, events);
}

// Initialize control queues
void initControl()
{
  gpio_init(SHUT_DOWN_BUTTON_GPIO);
  gpio_set_dir(SHUT_DOWN_BUTTON_GPIO, GPIO_IN);
  gpio_init(FORGET_WIFI_BUTTON_GPIO);
  gpio_set_dir(FORGET_WIFI_BUTTON_GPIO, GPIO_IN);

  gpio_set_irq_enabled_with_callback(SHUT_DOWN_BUTTON_GPIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &sharedISR);
  gpio_set_irq_enabled(FORGET_WIFI_BUTTON_GPIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

  gpio_init(RELAY_GPIO);
  gpio_set_dir(RELAY_GPIO, GPIO_OUT);
  gpio_put(RELAY_GPIO, 0);

  gpio_init(SOLENOID_GPIO);
  gpio_set_dir(SOLENOID_GPIO, GPIO_OUT);
  gpio_put(SOLENOID_GPIO, 0);

  const TickType_t compressionTimerPeriod = pdMS_TO_TICKS(currentSettings.compressionTimeout * 1000 * 60); // 1000 ms timer period
  compressionTimer = xTimerCreate("compressionTimer",                                                      // Just a text name, not used by the kernel.
                                  compressionTimerPeriod,
                                  pdFALSE,             // The timer will auto-reload itself when it expires.
                                  (void *)0,           // Assign each timer a unique id equal to its array index.
                                  handleTimerReached); // Pointer to the timer callback function

  if (compressionTimer == NULL)
  {
    // The timer was not created.
    printf("Failed to create shutdown timer \n");
  }

  const TickType_t supplyTimerPeriod = pdMS_TO_TICKS(currentSettings.supplyTimeout * 1000 * 60); // 1000 ms timer period
  supplyTimer = xTimerCreate("supplyTimer",                                                      // Just a text name, not used by the kernel.
                             supplyTimerPeriod,
                             pdFALSE,             // The timer will auto-reload itself when it expires.
                             (void *)0,           // Assign each timer a unique id equal to its array index.
                             handleTimerReached); // Pointer to the timer callback function

  if (supplyTimer == NULL)
  {
    // The timer was not created.
    printf("Failed to create shutdown timer \n");
  }

  const TickType_t motorTimerPeriod = pdMS_TO_TICKS(currentSettings.motorTimeout * 1000 * 60); // 1000 ms timer period
  motorTimer = xTimerCreate("motorTimer",                                                      // Just a text name, not used by the kernel.
                            motorTimerPeriod,
                            pdFALSE,             // The timer will auto-reload itself when it expires.
                            (void *)0,           // Assign each timer a unique id equal to its array index.
                            handleTimerReached); // Pointer to the timer callback function

  if (motorTimer == NULL)
  {
    // The timer was not created.
    printf("Failed to create shutdown timer \n");
  }

  compressionWatchTimer = xTimerCreate("compressionWatchTimer",
                                       pdMS_TO_TICKS(60000),
                                       pdTRUE,
                                       (void *)0,
                                       handleWatchTimerChange);
  if (compressionWatchTimer == NULL)
  {
    printf("Failed to create the timer.\n");
  }

  supplyWatchTimer = xTimerCreate("supplyWatchTimer",
                                  pdMS_TO_TICKS(60000),
                                  pdTRUE,
                                  (void *)0,
                                  handleWatchTimerChange);
  if (supplyWatchTimer == NULL)
  {
    printf("Failed to create the timer.\n");
  }

  motorWatchTimer = xTimerCreate("motorWatchTimer",
                                 pdMS_TO_TICKS(60000),
                                 pdTRUE,
                                 (void *)0,
                                 handleWatchTimerChange);
  if (motorWatchTimer == NULL)
  {
    printf("Failed to create the timer.\n");
  }

  longPressTimer = xTimerCreate("LongPressTimer",
                                pdMS_TO_TICKS(LONG_PRESS_THRESHOLD),
                                pdFALSE,
                                0,
                                longPressCallback);
  if (longPressTimer == NULL)
  {
    printf("Failed to create the timer.\n");
  }

  incommingMessageQueue = xQueueCreate(10, sizeof(Message));

  if (!incommingMessageQueue)
  {
    printf("Failed to create incoming message queues.\n");
  }

  interactionQueue = xQueueCreate(10, sizeof(Interaction));

  if (!interactionQueue)
  {
    printf("Failed to create interaction queue.\n");
  }
}

// Functions to send specific info types
void sendPressureChangeInfo(float pressure)
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = PRESSURE_CHANGE;
  msg.pressure = pressure;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendTurnedOnInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = TURNED_ON;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendTurnedOffInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = TURNED_OFF;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendReleasingInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = RELEASING;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendSupplydInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = RELEASED;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendMotorStartInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = MOTOR_START;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendMotorStopInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = MOTOR_STOP;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendCompressionCountdownUpdatedInfo(int timeout)
{

  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = COMPRESSION_COUNTDOWN_UPDATED;
  msg.timeout = timeout;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendSupplyCountdownUpdatedInfo(int timeout)
{

  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = RELEASE_COUNTDOWN_UPDATE;
  msg.timeout = timeout;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendMotorCountdownUpdatedInfo(int timeout)
{

  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = MOTOR_COUNTDOWN_UPDATE;
  msg.timeout = timeout;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendCompressionCountdownEndInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = COMPRESSION_COUNTOWN_END;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendSupplyCountdownEndInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = RELEASE_COUNTDOWN_END;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendMotorCountdownEndInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = MOTOR_COUNTDOWN_END;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendSupplyStartInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = SUPPLY_START;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

void sendSupplyStopInfo()
{
  Message msg;
  msg.messageType = MessageType::INFO;
  msg.infoType = SUPPLY_STOP;

  if (xQueueSend(outgoingMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
  {
    printf("Failed to enqueue info message.\n");
  }
}

// Converts a buffer (JSON string) into a Message struct
bool bufferToMessage(const char *buffer, Message &msg)
{
  // Parse JSON
  cJSON *json = cJSON_Parse(buffer);
  if (!json)
  {
    printf("Failed to parse JSON: %s\n", buffer);
    return false;
  }

  // Parse messageType
  cJSON *messageType = cJSON_GetObjectItem(json, "messageType");
  if (cJSON_IsString(messageType))
  {
    if (strcmp(messageType->valuestring, "COMMAND") == 0)
    {
      msg.messageType = MessageType::COMMAND;

      // Parse commandType
      cJSON *commandType = cJSON_GetObjectItem(json, "commandType");
      if (cJSON_IsString(commandType))
      {
        if (strcmp(commandType->valuestring, "ON") == 0)
        {
          msg.commandType = CommandType::ON;
        }
        else if (strcmp(commandType->valuestring, "OFF") == 0)
        {
          msg.commandType = CommandType::OFF;
        }
        else if (strcmp(commandType->valuestring, "OFF_RELEASE") == 0)
        {
          msg.commandType = CommandType::OFF_RELEASE;
        }
        else if (strcmp(commandType->valuestring, "SET_COMPRESSION_TIMEOUT") == 0)
        {
          msg.commandType = CommandType::SET_COMPRESSION_TIMEOUT;

          // Parse timeout
          cJSON *timeout = cJSON_GetObjectItem(json, "timeout");
          if (cJSON_IsNumber(timeout))
          {
            msg.timeout = timeout->valueint;
          }
        }
        else if (strcmp(commandType->valuestring, "SET_RELEASE_TIMEOUT") == 0)
        {
          msg.commandType = CommandType::SET_RELEASE_TIMEOUT;

          // Parse timeout
          cJSON *timeout = cJSON_GetObjectItem(json, "timeout");
          if (cJSON_IsNumber(timeout))
          {
            msg.timeout = timeout->valueint;
          }
        }
        else if (strcmp(commandType->valuestring, "SET_MOTOR_TIMEOUT") == 0)
        {
          msg.commandType = CommandType::SET_MOTOR_TIMEOUT;

          // Parse timeout
          cJSON *timeout = cJSON_GetObjectItem(json, "timeout");
          if (cJSON_IsNumber(timeout))
          {
            msg.timeout = timeout->valueint;
          }
        }
      }
    }
    else if (strcmp(messageType->valuestring, "INFO") == 0)
    {
      msg.messageType = MessageType::INFO;

      // Parse infoType
      cJSON *infoType = cJSON_GetObjectItem(json, "infoType");
      if (cJSON_IsString(infoType))
      {
        if (strcmp(infoType->valuestring, "PRESSURE_CHANGE") == 0)
        {
          msg.infoType = InfoType::PRESSURE_CHANGE;

          // Parse pressure
          cJSON *pressure = cJSON_GetObjectItem(json, "pressure");
          if (cJSON_IsNumber(pressure))
          {
            msg.pressure = static_cast<float>(pressure->valuedouble);
          }
        }
        else if (strcmp(infoType->valuestring, "COMPRESSION_COUNTDOWN_UPDATED") == 0)
        {
          msg.infoType = InfoType::COMPRESSION_COUNTDOWN_UPDATED;

          // Parse timeout
          cJSON *timeout = cJSON_GetObjectItem(json, "timeout");
          if (cJSON_IsNumber(timeout))
          {
            msg.timeout = timeout->valueint;
          }
        }
        else if (strcmp(infoType->valuestring, "RELEASE_COUNTDOWN_UPDATED") == 0)
        {
          msg.infoType = InfoType::RELEASE_COUNTDOWN_UPDATE;

          // Parse timeout
          cJSON *timeout = cJSON_GetObjectItem(json, "timeout");
          if (cJSON_IsNumber(timeout))
          {
            msg.timeout = timeout->valueint;
          }
        }
        else if (strcmp(infoType->valuestring, "RELEASE_COUNTDOWN_UPDATED") == 0)
        {
          msg.infoType = InfoType::MOTOR_COUNTDOWN_UPDATE;

          // Parse timeout
          cJSON *timeout = cJSON_GetObjectItem(json, "timeout");
          if (cJSON_IsNumber(timeout))
          {
            msg.timeout = timeout->valueint;
          }
        }
      }
    }
  }

  cJSON_Delete(json); // Free the JSON object
  return true;
}

// Converts a Message struct into a JSON string
std::string messageToString(const Message &msg)
{
  cJSON *json = cJSON_CreateObject();

  // Add messageType
  if (msg.messageType == MessageType::COMMAND)
  {
    cJSON_AddStringToObject(json, "messageType", "COMMAND");

    // Add commandType
    switch (msg.commandType)
    {
    case CommandType::ON:
      cJSON_AddStringToObject(json, "commandType", "ON");
      break;
    case CommandType::OFF:
      cJSON_AddStringToObject(json, "commandType", "OFF");
      break;
    case CommandType::OFF_RELEASE:
      cJSON_AddStringToObject(json, "commandType", "OFF_RELEASE");
      break;
    case CommandType::SET_COMPRESSION_TIMEOUT:
      cJSON_AddStringToObject(json, "commandType", "SET_COMPRESSION_TIMEOUT");
      cJSON_AddNumberToObject(json, "timeout", msg.timeout);
      break;
    case CommandType::SET_RELEASE_TIMEOUT:
      cJSON_AddStringToObject(json, "commandType", "SET_RELEASE_TIMEOUT");
      cJSON_AddNumberToObject(json, "timeout", msg.timeout);
      break;
    case CommandType::SET_MOTOR_TIMEOUT:
      cJSON_AddStringToObject(json, "commandType", "SET_MOTOR_TIMEOUT");
      cJSON_AddNumberToObject(json, "timeout", msg.timeout);
      break;
    default:
      break;
    }
  }
  else if (msg.messageType == MessageType::INFO)
  {
    cJSON_AddStringToObject(json, "messageType", "INFO");

    // Add infoType
    switch (msg.infoType)
    {
    case InfoType::PRESSURE_CHANGE:
      cJSON_AddStringToObject(json, "infoType", "PRESSURE_CHANGE");
      cJSON_AddNumberToObject(json, "pressure", msg.pressure);
      break;
    case InfoType::COMPRESSION_COUNTDOWN_UPDATED:
      cJSON_AddStringToObject(json, "infoType", "COMPRESSION_COUNTDOWN_UPDATED");
      cJSON_AddNumberToObject(json, "timeout", msg.timeout);
      break;
    case InfoType::RELEASE_COUNTDOWN_UPDATE:
      cJSON_AddStringToObject(json, "infoType", "RELEASE_COUNTDOWN_UPDATE");
      cJSON_AddNumberToObject(json, "timeout", msg.timeout);
      break;
    case InfoType::MOTOR_COUNTDOWN_UPDATE:
      cJSON_AddStringToObject(json, "infoType", "MOTOR_COUNTDOWN_UPDATE");
      cJSON_AddNumberToObject(json, "timeout", msg.timeout);
      break;
    default:
      break;
    }
  }

  // Convert JSON to string
  char *jsonString = cJSON_PrintUnformatted(json);
  std::string result(jsonString);
  cJSON_free(jsonString);
  cJSON_Delete(json); // Free the JSON object

  return result;
}

void handleOn()
{
  gpio_put(RELAY_GPIO, 1);
  sendTurnedOnInfo();
  gpio_put(SOLENOID_GPIO, 0);
  startTimer(compressionTimer); // TODO: change to read pressure
}

void handleOff()
{
  gpio_put(RELAY_GPIO, 0);
  sendTurnedOffInfo();
  gpio_put(SOLENOID_GPIO, 0);
  stopTimer(compressionTimer); // TODO: change to read pressure
}

void handleSupplyAndOff()
{
  gpio_put(RELAY_GPIO, 0);
  sendTurnedOffInfo();
  gpio_put(SOLENOID_GPIO, 1);
  sendReleasingInfo();
  // TODO: read pressure
  vTaskDelay(pdMS_TO_TICKS(20000));
  gpio_put(SOLENOID_GPIO, 0);
  sendSupplydInfo();
  stopTimer(compressionTimer); // TODO: change to read pressure
}

void handleSetCompressionTimeout(int timeout)
{
  currentSettings.compressionTimeout = timeout;
  requestSettingsValidation();
  changeTimerPreiod(compressionTimer, timeout);
}

void handleSetSupplyTimeout(int timeout)
{
  currentSettings.supplyTimeout = timeout;
  requestSettingsValidation();
  changeTimerPreiod(supplyTimer, timeout);
}

void handleSetMotorTimeout(int timeout)
{
  currentSettings.motorTimeout = timeout;
  requestSettingsValidation();
  changeTimerPreiod(motorTimer, timeout);
}

void handleSupplyStart()
{
  sendSupplyStartInfo();
  startTimer(supplyTimer);
}
void handleSupplyStop()
{
  sendSupplyStopInfo();
  stopTimer(supplyTimer);
}
void handleMotorStart()
{
  sendMotorStartInfo();
  startTimer(motorTimer);
}
void handleMotorStop()
{
  sendMotorStopInfo();
  stopTimer(motorTimer);
}

// Process incoming commands
void controlTask(void *params)
{
  Message command;
  while (xQueueReceive(incommingMessageQueue, &command, portMAX_DELAY) == pdPASS)
  {
    if (command.messageType == MessageType::COMMAND)
    {
      switch (command.commandType)
      {
      case CommandType::ON:
        printf("Received ON command.\n");
        handleOn();
        break;
      case CommandType::OFF:
        printf("Received OFF command.\n");
        handleOff();
        break;
      case CommandType::OFF_RELEASE:
        printf("Received OFF_RELEASE command.\n");
        handleSupplyAndOff();
        break;
      case CommandType::SET_COMPRESSION_TIMEOUT:
        printf("Set pressure timeout to %d minutes.\n", command.timeout);
        handleSetCompressionTimeout(command.timeout);
        break;
      case CommandType::SET_RELEASE_TIMEOUT:
        printf("Set supply timeout to %d minutes.\n", command.timeout);
        handleSetCompressionTimeout(command.timeout);
        break;
      case CommandType::SET_MOTOR_TIMEOUT:
        printf("Set motor timeout to %d minutes.\n", command.timeout);
        handleSetCompressionTimeout(command.timeout);
        break;
      default:
        printf("Unknown command received.\n");
        break;
      }
    }
  }
}

void interactionTask(void *params)
{
  Interaction interaction;
  while (xQueueReceive(interactionQueue, &interaction, portMAX_DELAY) == pdPASS)
  {
    switch (interaction)
    {
    case Interaction::SHUT_DOWN:
      handleSupplyAndOff();
      break;
    case Interaction::FORGET_WIFI:
      disconnectAndForgetWifi();
      break;
    default:
      break;
    }
  }
}
