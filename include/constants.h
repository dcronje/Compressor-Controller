#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#define SSID_MAX_LEN 32
#define PASSWORD_MAX_LEN 64
#define WIFI_SSID "Pico-Compressor"
#define WIFI_PASSWORD "password"
#define SOCKET_SERVER_IP "192.168.10.44"

const int PRESSURE_SENSOR_GPIO = 26;
const int PRESSURE_SENSOR_ADC_CHANNEL = 0;
const int CURRENT_SENSOR_GPIO = 27;
const int CURRENT_SENSOR_ADC_CHANNEL = 1;

const int RELAY_GPIO = 17;
const int SOLENOID_GPIO = 15;

const int TEMPERATURE_SENSOR_GPIO = 12;

const int LED_GPIO = CYW43_WL_GPIO_LED_PIN;
const int SOCKET_SERVER_PORT = 3000;

const int SHUT_DOWN_BUTTON_GPIO = 6;
const int FORGET_WIFI_BUTTON_GPIO = 4;

const int WS2812_GPIO = 3;

#endif // CONSTANTS_H