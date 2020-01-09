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

#include "esp_log.h"
#include "esp_tls.h"

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "10.44.188.29"
#define WEB_PORT "8080"
#define WEB_URL "https://10.44.188.29:"WEB_PORT"/upgrade/hw_upgrade.bin"

static const char *TAG = "example";


static const char *REQUEST = "GET " WEB_URL " HTTP/1.1\r\n"
    "Host: "WEB_SERVER":"WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32"
	"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
	"Accept-Language: en-US,en;q=0.5\r\n"
	"Accept-Encoding: gzip, deflate, br\r\n"
	"Connection: close\r\n"
    "\r\n";

/* Root CA root certificate */
extern const uint8_t test_cert_pem_start[] asm("_binary_test_cert_pem_start");
extern const uint8_t test_cert_pem_end[]   asm("_binary_test_cert_pem_end");


static void https_get_task(void *pvParameters)
{
    char buf[512];
    int ret, len;

    while(1) {
        esp_tls_cfg_t cfg = {
            .cacert_buf  = test_cert_pem_start,
            .cacert_bytes = test_cert_pem_end - test_cert_pem_start,
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
