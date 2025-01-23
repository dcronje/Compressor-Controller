#include "sensors.h"
#include "constants.h"
#include "control.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "FreeRTOS.h"
#include "task.h"

volatile float currentDraw = 0.0f;
volatile float pressure = 0.0f;

void initSensors(void)
{
  // Initialize ADC
  adc_init();

  // Assuming you use onboard ADC (GPIO 26 to 29 are ADC capable on Pico)
  // Setup GPIO for ADC usage (Check actual GPIO connection and adjust)
  adc_gpio_init(PRESSURE_SENSOR_GPIO);
  adc_gpio_init(CURRENT_SENSOR_GPIO);

  // Configure ADC to use 12-bit resolution
  adc_set_round_robin(1u << 0 | 1u << 1); // Enable channels 0 and 1 for round robin
  adc_select_input(0);                    // Default to channel 0
}

float readADC(uint channel)
{
  adc_select_input(channel);
  uint16_t raw = adc_read();
  return (raw * 3.3f) / 4096.0f; // Convert ADC value to voltage assuming 3.3V reference
}

float voltageToPsi(float voltage)
{
  // Clamp voltage to the expected range after scaling
  if (voltage < 0.333f) // Minimum expected scaled voltage
    voltage = 0.333f;
  if (voltage > 3.0f) // Maximum expected scaled voltage
    voltage = 3.0f;

  // Convert the scaled voltage back to the presumed sensor voltage
  float actualSensorVoltage = (voltage - 0.333f) * (4.5f - 0.5f) / (3.0f - 0.333f) + 0.5f;

  // Calculate kPa from the presumed sensor voltage
  // Linear mapping from 0.5V (-100 kPa) to 4.5V (300 kPa)
  float kPa = ((actualSensorVoltage - 0.5f) * 400.0f / 4.0f) - 100.0f;

  // Convert kPa to PSI
  return kPa * 0.14503773779f;
}

float voltageToAmps(float voltage)
{
  // Ensure voltage stays within the expected output range of the sensor
  if (voltage < 1.65f) // Quiescent output when no current flows
    voltage = 1.65f;
  if (voltage > 3.3f) // Maximum expected voltage output
    voltage = 3.3f;

  // Subtract the quiescent voltage to get the voltage contribution by the current
  float voltageContribution = voltage - 1.65f;

  // Conversion from voltage to current, using the adjusted sensitivity
  return voltageContribution / 0.0264; // Adjusted sensitivity of 26.4mV/A, converted to V/A for the formula
}

void sensorTask(void *params)
{
  const TickType_t xDelay = pdMS_TO_TICKS(500); // 1000 ms delay between readings

  static float lastPressure = 0;
  static float lastCurrentDraw = 0;
  bool isPressurized = false;

  while (1)
  {
    float voltage_pressure = readADC(PRESSURE_SENSOR_ADC_CHANNEL); // Adjust channel as needed
    pressure = roundf(voltageToPsi(voltage_pressure) * 10.0f) / 10.0f;

    float voltage_current = readADC(CURRENT_SENSOR_ADC_CHANNEL); // Adjust channel as needed
    currentDraw = roundf(voltageToAmps(voltage_current) * 10.0f) / 10.0f;

    if (currentDraw > 0 && lastCurrentDraw == 0)
    {
      handleMotorStart();
    }
    else if (currentDraw == 0 && lastCurrentDraw > 0)
    {
      handleMotorStop();
    }
    lastCurrentDraw = currentDraw;

    if (pressure != lastPressure)
    {
      sendPressureChangeInfo(pressure);
    }

    if (pressure > lastPressure)
    {
      if (isPressurized)
      {
        handleSupplyStop();
      }
    }
    else if (lastPressure > pressure)
    {
      if (pressure == 0)
      {
        isPressurized = false; // Reset pressurization status on shutdown
      }
      else if (isPressurized)
      {
        handleSupplyStart();
      }
    }
    else if (pressure == lastPressure && !isPressurized && pressure > 0)
    {
      isPressurized = true; // Mark as pressurized when pressure stabilizes
    }

    lastPressure = pressure;

    printf("Current Draw: %.2f A, Pressure: %.2f PSI\n", currentDraw, pressure);

    vTaskDelay(xDelay); // Delay for a second
  }
}