#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <stdbool.h>


#define RANGO_HUMEDAD 1000
#define BLINK_GPIO 33
#define PUMP_GPIO 15
#define BLINK_PERIOD 1000
#define WAIT_TIME 5000
#define RIEGO_TIME 2000

void configure_led(void)
{
  printf("Example configured to blink GPIO LED!");
  gpio_reset_pin(BLINK_GPIO);
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}



void configure_adc(void)
{
  adc2_config_channel_atten(ADC2_CHANNEL_5, ADC_ATTEN_DB_11);  
}

void configure_pump(void)
{
  printf("Configurando bomba...\n");
  gpio_reset_pin(PUMP_GPIO);
  gpio_set_direction(PUMP_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(PUMP_GPIO, 0);
}


int read_adc(void)
{
  int adc_raw;
  adc2_get_raw(ADC2_CHANNEL_5, ADC_WIDTH_BIT_12, &adc_raw); 
  printf( "adc raw %i!\n", adc_raw);
  return adc_raw;
}

bool check_adc(int adc_raw)
{
  if (adc_raw < RANGO_HUMEDAD)
  {
    printf("Valor bajo\n");
    gpio_set_level(BLINK_GPIO,1);
    return true;
  } else
    {
      printf("Valor medio\n");
      gpio_set_level(BLINK_GPIO,0);
      return false;
    }
}

void control_pump(bool on)
{
  if (on)
  {
    gpio_set_level(PUMP_GPIO, 1);
  } else
    {
      gpio_set_level(PUMP_GPIO, 0);
    }
}
      

void app_main(void)
{
  static bool pump_state = false;
  static TickType_t pump_time = 0;
  static TickType_t waiting_time = 0;
  static bool waiting_state = false;
  int valor;
  bool bajo;
  
  configure_led();
  configure_adc();
  configure_pump();
  
    while (true)
    {
      valor = read_adc();
      bajo = check_adc(valor);
      if (bajo && !pump_state && !waiting_state)
      {
	pump_state = true;
	control_pump(true);
	pump_time = xTaskGetTickCount();
	printf("Bomba Encendida\n");
      }
      if (pump_state)
      {
	TickType_t elapsed_ms = (xTaskGetTickCount() - pump_time) * portTICK_PERIOD_MS;
	if (!bajo || elapsed_ms >= RIEGO_TIME)
	{
	  control_pump(false);
	  pump_state = false;
	  waiting_state = true;
	  waiting_time = xTaskGetTickCount();
	  printf ("Bomba Apagada - Esperando\n");
	}
      }
      
      if (waiting_state)
      {
	TickType_t elapsed_ms = (xTaskGetTickCount() - waiting_time) * portTICK_PERIOD_MS;
	if (elapsed_ms >= WAIT_TIME)
	{
	  waiting_state = false;
	  printf("Tiempo de espera terminado\n");
	}
      }	
      vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);      
    }
}
