#ifndef SENSORS_H
#define SENSORS_H

#include "constants.h"
#include "FreeRTOS.h"

void initSensors(void);
void sensorTask(void *params);

// External variables (declarations only)
extern volatile float currentDraw;
extern volatile float pressure;

#endif // SENSORS_H