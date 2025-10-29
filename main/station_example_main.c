#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include <stdio.h>
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <stdbool.h>

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include <lwip/netdb.h>

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#define SERVER_IP      "172.20.10.2"  // Your PC's IP
#define SERVER_PORT    23001



#define RANGO_HUMEDAD 1000
#define RANGO_HUMEDAD2 1000
#define RANGO_NIVEL_AGUA 1000

#define BLINK_GPIO 4
#define PUMP_GPIO 15
#define PUMP_GPIO2 2

#define BLINK_PERIOD 100
#define WAIT_TIME 1000
#define RIEGO_TIME 500

#define ADC_PIN ADC2_CHANNEL_5
#define ADC_PIN2 ADC2_CHANNEL_4
#define ADC_WATER_PIN  ADC2_CHANNEL_6

static const char *TAG = "irrigation_system";
static int s_retry_num = 0;

void blink_led (int repes, int delay)
{
  for(int i = 0; i < repes; i++)
    {
      gpio_set_level(BLINK_GPIO, 1);
      vTaskDelay(delay / portTICK_PERIOD_MS);
      gpio_set_level(BLINK_GPIO, 0);
      vTaskDelay(delay / portTICK_PERIOD_MS);
    }
}

void blink_led_bomba1(void)
{
  blink_led(1, 300);
}


void blink_led_bomba2(void)
{
  blink_led(1, 600);
}


void blink_led_net(void)
{
  blink_led(1, 30);
}

void blink_led_error(void)
{
  blink_led(4, 30);
}



#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif


static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1




static void event_handler(void* arg, esp_event_base_t event_base,
			  int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG,"connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_init_once(void)
{
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
						      ESP_EVENT_ANY_ID,
						      &event_handler,
						      NULL,
						      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
						      IP_EVENT_STA_GOT_IP,
						      &event_handler,
						      NULL,
						      &instance_got_ip));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = EXAMPLE_ESP_WIFI_SSID,
      .password = EXAMPLE_ESP_WIFI_PASS,
      .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
      .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
      .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
    },
  };



  
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(TAG, "wifi_init_sta finished.");
}

bool wifi_start_and_connect(char* ip_str)
{
  s_retry_num = 0;
  
  // Iniciar WiFi (bloquea ADC2)
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "WiFi started, connecting...");

  // Esperar conexión
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdTRUE,  // Clear bits
                                         pdFALSE,
                                         pdMS_TO_TICKS(15000));

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "Connected to AP");

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
    return true;
  } else {
    ESP_LOGI(TAG, "Failed to connect");
    strcpy(ip_str, "0.0.0.0");
    return false;
  }
}

void wifi_stop_completely(void)
{
  ESP_LOGI(TAG, "Stopping WiFi...");
  esp_wifi_disconnect();
  esp_wifi_stop();
  ESP_LOGI(TAG, "WiFi stopped - ADC2 available");
}


static int sock = -1;

static bool telnet_management(int adc1, int adc2, int water_level, const char* ip_address)
{
  struct sockaddr_in dest_addr;
  dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(SERVER_PORT);

  sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (sock < 0)
    {
      printf("Unable to create socket: errno %d\r\n", errno);
      blink_led_error();
      return false;
    }
  printf( "Socket created, connecting to %s:%d\r\n", SERVER_IP, SERVER_PORT);

  int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err != 0)
    {
      printf("Socket unable to connect: errno %d\r\n", errno);
      close(sock);
      blink_led_error();
      return false;
    }
  blink_led_net();
  printf("Successfully connected\r\n");

 
  char message[200]; 
  uint32_t uptime_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;
  sprintf(message, "{\"humedad1\":%d,\"humedad2\":%d,\"nivel\":%d,\"uptime_ms\":%lu,\"ip\":\"%s\"}\r\n",
	  adc1, adc2, water_level, uptime_ms, ip_address);
  send(sock, message, strlen(message), 0);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  printf("Shutting down socket\r\n");
  shutdown(sock, 0);
  close(sock);
  return true;
}



void configure_led(void)
{
  printf("Configurando LED\r\n");
  gpio_reset_pin(BLINK_GPIO);
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}
void configure_adc(void)
{
  printf("Configurando ADC 1\r\n");
  adc2_config_channel_atten(ADC_PIN, ADC_ATTEN_DB_12);   
}
void configure_pump(void)
{
  printf("Configurando bomba 1\r\n");
  gpio_reset_pin(PUMP_GPIO);
  gpio_set_direction(PUMP_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(PUMP_GPIO, 0);
}
void configure_adc_water(void)
{
  printf("Configurando ADC water sensor\r\n");
  adc2_config_channel_atten(ADC_WATER_PIN, ADC_ATTEN_DB_12);
}



int read_water_adc(void)
{
  int adc_water_raw;
  adc2_get_raw(ADC_WATER_PIN, ADC_WIDTH_BIT_12, &adc_water_raw); 
  printf( "adc water raw %d\r\n",adc_water_raw);
  return adc_water_raw;
}
bool check_water_adc(int adc_water_raw)
{
  if (adc_water_raw > RANGO_NIVEL_AGUA)
    {
      printf("Nivel de agua normal\r\n");
      return true;
    } else
    {
      printf("No hay agua disponible\r\n");
      return false;
    }
}

int read_adc(void)
{
  int adc_raw;
  adc2_get_raw(ADC_PIN, ADC_WIDTH_BIT_12, &adc_raw); 
  printf( "adc raw 1 %d\r\n",adc_raw);
  return adc_raw;
}
bool check_adc(int adc_raw)
{
  if (adc_raw > RANGO_HUMEDAD)
    {
      printf("Valor del adc 1 bajo\r\n");
      return true;
    } else
    {
      printf("Valor del adc 1 medio\r\n");
      return false;
    }
}

void control_pump(bool pump_on)
{
  if (pump_on)
    {
      gpio_set_level(PUMP_GPIO, 1);
    } else
    {
      gpio_set_level(PUMP_GPIO, 0);
    }
}




void configure_adc2(void)
{
  printf("Configurando ADC 2\r\n");
  adc2_config_channel_atten(ADC_PIN2, ADC_ATTEN_DB_12);   
}
void configure_pump2(void)
{
  printf("Configurando bomba 2\r\n");
  gpio_reset_pin(PUMP_GPIO2);
  gpio_set_direction(PUMP_GPIO2, GPIO_MODE_OUTPUT);
  gpio_set_level(PUMP_GPIO2, 0);
}

int read_adc2(void)
{
  int adc_raw2;
  adc2_get_raw(ADC_PIN2, ADC_WIDTH_BIT_12, &adc_raw2); 
  printf( "adc raw 2 %d\r\n",adc_raw2);
  return adc_raw2;
}
bool check_adc2(int adc_raw2)
{
  if (adc_raw2 > RANGO_HUMEDAD2)
    {
      printf("Valor del adc 2 bajo\r\n");
      return true;
    } else
    {
      printf("Valor del adc 2 medio\r\n");
      return false;
    }
}

void control_pump2(bool pump_on2)
{
  if (pump_on2)
    {
      gpio_set_level(PUMP_GPIO2, 1);
    } else
    {
      gpio_set_level(PUMP_GPIO2, 0);
    }
}




void app_main(void)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_init_once();


  configure_led();
  configure_adc_water();
  configure_adc();
  configure_pump();
  
  configure_adc2();
  configure_pump2();
  
  static TickType_t pump_start_time = 0;
  static TickType_t waiting_start_time = 0;
  TickType_t pump_elapsed_ms = 0;
  TickType_t riego_elapsed_ms = 0;
  
  static TickType_t pump_start_time2 = 0;
  static TickType_t waiting_start_time2 = 0;
  TickType_t pump_elapsed_ms2 = 0;
  TickType_t riego_elapsed_ms2 = 0;


  int adc_measure = 0;
  bool adc_state = false;
  int water_measure = 0;
  bool water_state = false;

  int adc_measure2 = 0;
  bool adc_state2 = false;

  typedef enum {
    STATE_MIDIENDO,
    STATE_WNIVEL,
    STATE_REGANDO,
    STATE_ESPERANDO,
  }SystemState;

  SystemState currentState = STATE_MIDIENDO;
  SystemState currentState2 = STATE_MIDIENDO;

  while (1)
    {
      switch (currentState) {
      case STATE_MIDIENDO:
	configure_adc();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	adc_measure = read_adc();
	adc_state = check_adc(adc_measure);
	if (adc_state)
	  {
	    currentState = STATE_WNIVEL;
	  }
	break;

      case STATE_WNIVEL:
	configure_adc();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	adc_measure = read_adc();
	adc_state = check_adc(adc_measure);
	configure_adc_water();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	water_measure = read_water_adc();
	water_state = check_water_adc(water_measure);
	if (water_state)
	  {
	    pump_start_time = xTaskGetTickCount();
	    currentState = STATE_REGANDO;
	  }
	else
	  {
	    currentState = STATE_MIDIENDO;
	  }
	break;

      case STATE_REGANDO:
	configure_adc();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	adc_measure = read_adc();
	adc_state = check_adc(adc_measure);
	control_pump(true);
	blink_led_bomba1();
	printf("Bomba Encendida\r\n");
	pump_elapsed_ms = (xTaskGetTickCount() - pump_start_time) * portTICK_PERIOD_MS;
	if (!adc_state || pump_elapsed_ms > RIEGO_TIME)
	  {
	    control_pump(false);
	    waiting_start_time = xTaskGetTickCount();
	    currentState = STATE_ESPERANDO;
	  }
	break;

      case STATE_ESPERANDO:
	configure_adc();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	adc_measure = read_adc();
	adc_state = check_adc(adc_measure);
	printf ("Bomba Apagada - Esperando\r\n");
	riego_elapsed_ms = (xTaskGetTickCount() - waiting_start_time) * portTICK_PERIOD_MS;
	if (riego_elapsed_ms > WAIT_TIME)
	  {
	    printf("Tiempo de espera terminado\r\n");
	    currentState = STATE_MIDIENDO;
	  }
	break;
      }


      switch (currentState2) {
      case STATE_MIDIENDO:
	configure_adc2();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	adc_measure2 = read_adc2();
	adc_state2 = check_adc2(adc_measure2);
	if (adc_state2)
	  {
	    currentState2 = STATE_WNIVEL;
	  }
	break;

      case STATE_WNIVEL:
	configure_adc2();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	adc_measure2 = read_adc2();
	adc_state2 = check_adc2(adc_measure2);
	configure_adc_water();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	water_measure = read_water_adc();
	water_state = check_water_adc(water_measure);
	if (water_state)
	  {
	    pump_start_time2 = xTaskGetTickCount();
	    currentState2 = STATE_REGANDO;
	  }else
	  {
	    currentState2 = STATE_MIDIENDO;
	  }	
	break;

      case STATE_REGANDO:
       
	configure_adc2();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	adc_measure2 = read_adc2();
	adc_state2 = check_adc2(adc_measure2);
	control_pump2(true);
	blink_led_bomba2();
	printf("Bomba 2 Encendida\r\n");
	pump_elapsed_ms2 = (xTaskGetTickCount() - pump_start_time2) * portTICK_PERIOD_MS;
	if (!adc_state2 || pump_elapsed_ms2 > RIEGO_TIME)
	  {
	    control_pump2(false);
	    waiting_start_time2 = xTaskGetTickCount();
	    currentState2  = STATE_ESPERANDO;
	  }
	break;

      case STATE_ESPERANDO:
	configure_adc2();
	vTaskDelay(10 / portTICK_PERIOD_MS);
	adc_measure2 = read_adc2();
	adc_state2 = check_adc2(adc_measure2);
	printf ("Bomba 2 Apagada - Esperando\r\n");
	riego_elapsed_ms2 = (xTaskGetTickCount() - waiting_start_time2)  * portTICK_PERIOD_MS;
	if (riego_elapsed_ms2 > WAIT_TIME)
	  {
	    printf("Tiempo de espera terminado\r\n");
	    currentState2 = STATE_MIDIENDO;
	  }
	break;
      }

      
      printf("INICIANDO WIFI\r\n");
      char esp_ip[16];
      wifi_start_and_connect(esp_ip);
      telnet_management(adc_measure, adc_measure2, water_measure, esp_ip);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      wifi_stop_completely();
      printf("Desconectando WIFI\r\n");
    } 
}
