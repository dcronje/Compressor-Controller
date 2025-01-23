// control.h
#ifndef CONTROL_H
#define CONTROL_H

#include <string>

#include "FreeRTOS.h"
#include "queue.h"
#include "timers.h"
#include "cJSON.h"

#define LONG_PRESS_THRESHOLD 500

typedef enum
{
  COMMAND,
  INFO
} MessageType;

typedef enum
{
  ON,
  OFF,
  OFF_RELEASE,
  SET_PRESSURE_TIMEOUT,
  SET_RELEASE_TIMEOUT,
  SET_MOTOR_TIMEOUT,
} CommandType;

typedef enum
{
  TURNED_ON,
  TURNED_OFF,
  RELEASING,
  RELEASED,
  PRESSURE_CHANGE,
  MOTOR_START,
  MOTOR_STOP,
  PRESSURE_COUNTOWN_END,
  RELEASE_COUNTDOWN_END,
  MOTOR_COUNTDOWN_END,
  PRESSURE_COUNTDOWN_UPDATED,
  RELEASE_COUNTDOWN_UPDATE,
  MOTOR_COUNTDOWN_UPDATE,
  SUPPLY_START,
  SUPPLY_STOP
} InfoType;

typedef struct
{
  MessageType messageType;

  // Command-specific fields
  CommandType commandType;
  int timeout; // For SET_SHUTDOWN_TIMEOUT

  // Info-specific fields
  InfoType infoType;
  float pressure; // For PRESSURE_CHANGE
} Message;

// Initialize control queues
void initControl();

// Functions to send specific info types
void sendPressureChangeInfo(float pressure);
void sendTurnedOnInfo();
void sendTurnedOffInfo();
void sendReleasingInfo();
void sendSupplydInfo();
void sendMotorStartInfo();
void sendMotorStopInfo();
void sendPressureCountdownUpdatedInfo(int timeout);
void sendSupplyCountdownUpdatedInfo(int timeout);
void sendMotorCountdownUpdatedInfo(int timeout);
void sendPressureCountdownEndInfo();
void sendSupplyCountdownEndInfo();
void sendMotorCountdownEndInfo();
void sendSupplyStartInfo();
void sendSupplyStopInfo();

// Queue handles for receiving commands and sending info
extern QueueHandle_t incommingMessageQueue;

bool bufferToMessage(const char *buffer, Message &msg);
std::string messageToString(const Message &msg);

// Functions to process incoming commands
void controlTask(void *params);
void interactionTask(void *params);

void handleSetPressureTimeout(int timeout);
void handleSetSupplyTimeout(int timeout);
void handleSetMotorTimeout(int timeout);
void handleSupplyAndOff();
void handleOff();
void handleOn();

void handleSupplyStart();
void handleSupplyStop();
void handleMotorStart();
void handleMotorStop();

void handleTimerWatch(TimerHandle_t xTimer);
void longPressCallback(TimerHandle_t xTimer);

#endif // CONTROL_H
