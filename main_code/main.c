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
#define BLINK_GPIO CONFIG_BLINK_GPIO
#define CONFIG_PUMP_GPIO 15
#define CONFIG_BLINK_PERIOD 1000 // 1 segundo


static const char *TAG = "example";


void logi(const char *texto)
{
  ESP_LOGI(TAG," %s", texto);
}


void configure_led(void)
{
    logi("Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}



void configure_adc(void) {
    adc2_config_channel_atten(ADC2_CHANNEL_5, ADC_ATTEN_DB_11);  
}

void configure_pump(void)
{
  logi("Configurando bomba...");
  gpio_reset_pin(CONFIG_PUMP_GPIO);
  gpio_set_direction(CONFIG_PUMP_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(CONFIG_PUMP_GPIO, 0);
}


int read_adc(void)
{
    /* int adc_raw; */
 
    char adc_raw[20];
    printf("\n--- Ingrese nuevo valor para RANGO_HUMEDAD: ");
    fflush(stdout);
    
    if ( fgets(adc_raw, sizeof(adc_raw), stdin)!=NULL)
    {
      int rango_humedad = atoi(adc_raw);
      printf("Nuevo rango establecido: %d\n", rango_humedad);

    /* adc2_get_raw(ADC2_CHANNEL_5, ADC_WIDTH_BIT_12, &adc_raw); */
    /* ESP_LOGI(TAG, "adc raw %i!", adc_raw); */
    return rango_humedad;
    } else
    {
        return RANGO_HUMEDAD;
    }

}

bool check_adc(int adc_raw)
{
  bool mode = false;
  if (adc_raw < RANGO_HUMEDAD)  {
    
    logi("Valor bajo");
    gpio_set_level(BLINK_GPIO,0);
    mode = true;
    return mode;
  } else {
       logi("Valor medio");
       gpio_set_level(BLINK_GPIO,1);
       mode = false;
       return mode;
   }
}

void control_pump(bool on)
{
  if (on)
  {
    gpio_set_level(CONFIG_PUMP_GPIO, 1);
  } else
    {
      gpio_set_level(CONFIG_PUMP_GPIO, 0);
    }
}
      

void app_main(void)
{
    configure_led();
    configure_adc();
    configure_pump();
    static bool pump_state = false;
    static TickType_t pump_time = 0;

    
    while (1) {
      int valor = read_adc();
      bool bajo = check_adc(valor);
    

      if (bajo && !pump_state)
      {
	gpio_set_level(CONFIG_PUMP_GPIO,  1);
	pump_state = true;
	control_pump(true);
	pump_time = xTaskGetTickCount();
	logi("Bomba Encendida");
	
      }

      if (pump_state)
      {
	  TickType_t elapsed_ms = (xTaskGetTickCount() - pump_time) * portTICK_PERIOD_MS;
	  if (!bajo || elapsed_ms >= 5000)
	  {
	      control_pump(false);
	      pump_state = false;
	      logi ("Bomba Apagada");
	  }
      }
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
      
   }
 }
