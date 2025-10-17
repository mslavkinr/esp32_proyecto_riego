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



#define BLINK_GPIO 33
#define BLINK_PERIOD 10000

void configure_led(void)
{
  printf("Example configured to blink GPIO LED!");
  gpio_reset_pin(BLINK_GPIO);
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}



void configure_adc(void) {
  adc2_config_channel_atten(ADC2_CHANNEL_5, ADC_ATTEN_DB_11);  
}



int read_adc(void)
{
  int adc_raw;
  adc2_get_raw(ADC2_CHANNEL_5, ADC_WIDTH_BIT_12, &adc_raw);
  return adc_raw;
}


	
void app_main(void)
{
  configure_led();
  configure_adc();

  
  while (1)
    {
      TickType_t tiempo = xTaskGetTickCount() * portTICK_PERIOD_MS;
      printf("%05i %05lu\r\n", read_adc(), tiempo);
      vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}

