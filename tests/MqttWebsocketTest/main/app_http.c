/*
 * app_http.c
 *
 *  Created on: Jan 9, 2020
 *      Author: vvasilev
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_tls.h"

/* Constants that aren't configurable in menuconfig */
#define WEB_URL "https://"CONFIG_OTA_WEB_SERVER":"CONFIG_OTA_WEB_PORT"/upgrade/hw_upgrade.bin"

static const char *TAG = "app_http";


static const char *REQUEST = "GET " WEB_URL " HTTP/1.1\r\n"
    "Host: "CONFIG_OTA_WEB_SERVER":"CONFIG_OTA_WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32"
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
	"Accept-Language: en-US,en;q=0.5\r\n"
	"Accept-Encoding: gzip, deflate, br\r\n"
	"Connection: close\r\n"
    "\r\n";

/* Root CA root certificate */
extern const uint8_t ca_cert_pem_start[] asm("_binary_myCA_pem_start");
extern const uint8_t ca_cert_pem_end[]   asm("_binary_myCA_pem_end");


esp_http_client_config_t config = {
    .url = WEB_URL,
    .cert_pem = (char *)ca_cert_pem_start,
	.skip_cert_common_name_check = true,
};
esp_https_ota_config_t ota_config = {
   .http_config = &config,
};

esp_err_t do_firmware_upgrade()
{
	esp_err_t ota_finish_err = ESP_OK;

    ESP_LOGI(TAG, "ESP HTTPS OTA Begin from URL: %s", WEB_URL);
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        return ESP_FAIL;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    else {
    	ESP_LOGI(TAG, "app_desc: %s", app_desc.app_elf_sha256);
    	ESP_LOGI(TAG, "idf version: %s", app_desc.idf_ver);
    	ESP_LOGI(TAG, "version: %s", app_desc.version);
    	ESP_LOGI(TAG, "sha: %s", app_desc.app_elf_sha256);
    }

ota_end:
    ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
        ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
     //TODO Enable restart when OTA is implemented   esp_restart();
    } else {
        ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed...");
    }

    return ESP_OK;
}

#define HTTP_DATA "GET " WEB_URL " HTTP/1.1\r\n"

esp_err_t app_https_read_with_client(void) {

	esp_http_client_config_t config = {
	    .url = WEB_URL,
	    .cert_pem = (char *)ca_cert_pem_start,
		.skip_cert_common_name_check = true,
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);

	if(client == NULL) {
		return ESP_FAIL;
	}

	esp_http_client_write(client, HTTP_DATA, sizeof(HTTP_DATA));


	esp_http_client_read(client, (char *)REQUEST, strlen(REQUEST) + 1);

	return 0;
}

static void https_get_task(void *pvParameters)
{
    char buf[512];
    int ret, len;

    while(1) {
        esp_tls_cfg_t cfg = {
            .cacert_buf  = ca_cert_pem_start,
            .cacert_bytes = ca_cert_pem_end - ca_cert_pem_start,
	     	.skip_common_name = true,/*If it is false then Commmon Name should be the same as CONFIG_OTA_WEB_SERVER*/
        };

        struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, &cfg);

        if(tls != NULL) {
            ESP_LOGI(TAG, "Connection established...");
        } else {
            ESP_LOGE(TAG, "Connection failed...");
            goto exit;
        }

        size_t written_bytes = 0;
        do {
            ret = esp_tls_conn_write(tls,
                                     REQUEST + written_bytes,
                                     strlen(REQUEST) - written_bytes);
            if (ret >= 0) {
                ESP_LOGI(TAG, "%d bytes written", ret);
                written_bytes += ret;
            } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
                ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
                goto exit;
            }
        } while(written_bytes < strlen(REQUEST));

        ESP_LOGI(TAG, "Reading HTTP response...");

        do
        {
            len = sizeof(buf) - 1;
            bzero(buf, sizeof(buf));
            ret = esp_tls_conn_read(tls, (char *)buf, len);

            if( len > 0) {
				len = ret;
				ESP_LOGI(TAG, "%d bytes read", len);
				/* Print response directly to stdout as it is read */
				for(int i = 0; i < len; i++) {
					putchar(buf[i]);
				}
            }

            if(ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ)
                continue;

            if(ret < 0)
           {
                ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -ret);
                break;
            }

            if(ret == 0)
            {
                ESP_LOGI(TAG, "connection closed");
                break;
            }


        } while(1);

    exit:
        esp_tls_conn_delete(tls);
        putchar('\n'); // JSON output doesn't have a newline at end

        static int request_count;
        ESP_LOGI(TAG, "Completed %d requests", ++request_count);

        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d...", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}


esp_err_t app_http_task(void)
{
    return (xTaskCreate(&https_get_task, "https_get_task", 8192, NULL, 5, NULL) == pdPASS ?  ESP_OK:ESP_FAIL);
}
