/*
 * wifi_connector.c
 *
 *  Created on: Nov 18, 2019
 *      Author: vvasilev
 */

#include "wifi_connector.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_wps.h"
#include "wifi_connector.h"

#include "esp_log.h"

#define MAX_WIFI_SSID_LEN (32)


#if 0
typedef enum{
	EAP_TLS,
	EAP_PEAP,
	EAP_TTLS,
	EAP__MAX,
}eap_method_t;

typedef struct {
	sint8_t  uname[20];
	sint8_t  password[20];
	sint8_t  eap_id[20];
	uint8_t  srv_validate;
	/*Server CA validation certificate*/
	uint8_t* ca_srv_pem_start;
	uint32_t ca_srv_pem_len;
	/*Client TLS certificate*/
	uint8_t* client_crt_start;
	uint32_t client_crt_len;
	/*Client TLS KEY*/
	uint8_t* client_key_start;
	uint32_t client_key_len;
}wpa_connfig_t;

typedef struct{
	wps_type_t wps_type;
} wps_cfg_t;


typedef struct{
    uint8_t        ssid[MAX_WIFI_SSID_LEN];
    ewifi_con_t    wificon_type;
    wps_cfg_t*     wps_cfg;
    wpa_connfig_t* wpa_cfg;
    uint32_t       max_retry;
}wifi_con_cfg_t;
#endif
static const char *TAG = "WIFI_CONN";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* CA cert, taken from wpa2_ca.pem
   Client cert, taken from wpa2_client.crt
   Client key, taken from wpa2_client.key

   The PEM, CRT and KEY file were provided by the person or organization
   who configured the AP with wpa2 enterprise.

   To embed it in the app binary, the PEM, CRT and KEY file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
 */
#ifdef CONFIG_WIFICON_VALIDATE_SERVER_CERT
extern uint8_t ca_pem_start[] asm("_binary_wpa2_ca_pem_start");
extern uint8_t ca_pem_end[]   asm("_binary_wpa2_ca_pem_end");
#endif /* CONFIG_WIFICON_VALIDATE_SERVER_CERT */

#ifdef CONFIG_WIFICON_EAP_METHOD_TLS
extern uint8_t client_crt_start[] asm("_binary_wpa2_client_crt_start");
extern uint8_t client_crt_end[]   asm("_binary_wpa2_client_crt_end");
extern uint8_t client_key_start[] asm("_binary_wpa2_client_key_start");
extern uint8_t client_key_end[]   asm("_binary_wpa2_client_key_end");
#endif /* CONFIG_WIFICON_EAP_METHOD_TLS */

static void event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		ESP_LOGI(TAG, "WIFI start event");
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		ESP_LOGI(TAG, "WIFI disconnected event");
		esp_wifi_connect();
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ESP_LOGI(TAG, "WIFI got IP event");
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	}
}

static void _initialise_wifi(void)
{

#ifdef CONFIG_WIFICON_VALIDATE_SERVER_CERT
	unsigned int ca_pem_bytes = ca_pem_end - ca_pem_start;
#endif /* CONFIG_WIFICON_VALIDATE_SERVER_CERT */

#ifdef CONFIG_WIFICON_EAP_METHOD_TLS
	unsigned int client_crt_bytes = client_crt_end - client_crt_start;
	unsigned int client_key_bytes = client_key_end - client_key_start;
#endif /* CONFIG_WIFICON_EAP_METHOD_TLS */

	tcpip_adapter_init();
	wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_event_loop_create_default());

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

	wifi_config_t wifi_config_old;
	int rc = esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config_old);
	if(ESP_OK == rc ) {
		ESP_LOGI(TAG, "Boot WiFi configuration SSID %s...", wifi_config_old.sta.ssid);
		ESP_LOGI(TAG, "Boot WiFi configuration password %s...", wifi_config_old.sta.password);
	}
	else {
		ESP_LOGI(TAG, "esp_wifi_get_config error code %s...", esp_err_to_name(rc));
	}

	ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_FLASH) );

	wifi_config_t wifi_config = {
			.sta = {
					.ssid = CONFIG_ESP_WIFI_SSID,
#ifdef CONFIG_WIFI_CON_METHOD_WPA
					.password = CONFIG_ESP_WIFI_PASSWORD,
#endif
			},
	};

	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

#ifdef CONFIG_WIFI_CON_METHOD_WPA_ENT
	ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)CONFIG_WIFICON_EAP_ID, strlen(CONFIG_WIFICON_EAP_ID)) );

#ifdef CONFIG_WIFICON_VALIDATE_SERVER_CERT
	ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_ca_cert(ca_pem_start, ca_pem_bytes) );
#endif /* CONFIG_WIFICON_VALIDATE_SERVER_CERT */

#ifdef CONFIG_WIFICON_EAP_METHOD_TLS
	ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_cert_key(client_crt_start, client_crt_bytes,\
			client_key_start, client_key_bytes, NULL, 0) );
#endif /* CONFIG_WIFICON_EAP_METHOD_TLS */

#if defined CONFIG_WIFICON_EAP_METHOD_PEAP || CONFIG_WIFICON_EAP_METHOD_TTLS
	ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_username((uint8_t *)CONFIG_WIFICON_EAP_USERNAME, strlen(CONFIG_WIFICON_EAP_USERNAME)) );
	ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_password((uint8_t *)CONFIG_WIFICON_EAP_PASSWORD, strlen(CONFIG_WIFICON_EAP_PASSWORD)) );
#endif /* CONFIG_WIFICON_EAP_METHOD_PEAP || CONFIG_WIFICON_EAP_METHOD_TTLS */
	ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_enable() );
#endif

	ESP_ERROR_CHECK( esp_wifi_start() );
}

static void _wificon_task(void *pvParameters)
{
	tcpip_adapter_ip_info_t ip;
	memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));
	vTaskDelay(2000 / portTICK_PERIOD_MS);

	while (1) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);

		if (tcpip_adapter_get_ip_info(ESP_IF_WIFI_STA, &ip) == 0) {
			ESP_LOGI(TAG, "~~~~~~~~~~~");
			ESP_LOGI(TAG, "IP:"IPSTR, IP2STR(&ip.ip));
			ESP_LOGI(TAG, "MASK:"IPSTR, IP2STR(&ip.netmask));
			ESP_LOGI(TAG, "GW:"IPSTR, IP2STR(&ip.gw));
			ESP_LOGI(TAG, "~~~~~~~~~~~");
		}
	}
}


esp_err_t wifi_connect(void)
{
	const TickType_t xTicksToWait = 45000 / portTICK_PERIOD_MS;
	EventBits_t uxBits;
	ESP_LOGI(TAG, "Initialize Wifi");
	_initialise_wifi();
	xTaskCreate(&_wificon_task, "_wificon_task", 4096, NULL, 5, NULL);

	ESP_LOGI(TAG, "Wait for WIFI");
	// Wait a maximum of 100ms for either bit 0 or bit 4 to be set within
	// the event group.  Clear the bits before exiting.
	uxBits = xEventGroupWaitBits(
			wifi_event_group,	// The event group being tested.
			CONNECTED_BIT,	// The bits within the event group to wait for.
			pdFALSE,		// CONNECTED_BIT should not be cleared before returning.
			pdFALSE,		// Don't wait for other bits, CONNECTED_BIT bit will do.
			xTicksToWait );	// Wait a maximum of 100ms for either bit to be set.

	if( !(uxBits & CONNECTED_BIT) ) {
		ESP_LOGI(TAG, "Can't connect to wifi for %d ms", xTicksToWait*portTICK_PERIOD_MS);
		return ESP_FAIL;
	}

	return ESP_OK;

}

