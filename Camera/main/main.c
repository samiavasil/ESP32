#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp32_camera_lib.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "mqtt_client.h"

#include "esp_http_server.h"
#include "cJSON.h"

#include "esp_event.h"
#include "nvs_flash.h"
#include <string.h>
// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

static const char *TAG = "CAM_APP";

// Вашите Wi-Fi настройки
static char WIFI_SSID[20];
static char WIFI_PASSWORD[20];
esp_err_t load_wifi_credentials(char *ssid, size_t ssid_size, char *password, size_t pass_size);

#include "esp_timer.h"
uint32_t millis()
{
    return (uint32_t)(esp_timer_get_time() / 1000); // Връща времето в милисекунди
}

#if 1
uint8_t _State = 0;
// Event Handler за обработка на събития при свързване и разкачване от Wi-Fi
static void event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (strcmp(event_base, ESP_HTTP_SERVER_EVENT) == 0)
    {
        switch (event_id)
        {
        case HTTP_SERVER_EVENT_ERROR:
        { /*!< This event occurs when there are any errors during execution */
            ESP_LOGI(TAG, "EVT: HTTP_SERVER_EVENT_ERROR: %d", *((httpd_err_code_t *)event_data));
            break;
        }
        case HTTP_SERVER_EVENT_START:
        { /*!< This event occurs when HTTP Server is started */
            ESP_LOGI(TAG, "EVT: HTTP_SERVER_EVENT_START server=%p", (void *)event_data);
            break;
        }
        case HTTP_SERVER_EVENT_ON_CONNECTED:
        { /*!< Once the HTTP Server has been connected to the client: { no data exchange has been performed */
            ESP_LOGI(TAG, "EVT: HTTP_SERVER_EVENT_ON_CONNECTED fd=%d", *((int *)event_data));
            break;
        }
        case HTTP_SERVER_EVENT_ON_HEADER:
        { /*!< Occurs when receiving each header sent from the client */
            ESP_LOGI(TAG, "EVT: HTTP_SERVER_EVENT_ON_HEADER - Ne ochakwam da go ima  fd=%d", *((int *)event_data));
            break;
        }
        case HTTP_SERVER_EVENT_HEADERS_SENT:
        { /*!< After sending all the headers to the client */
            ESP_LOGI(TAG, "EVT: HTTP_SERVER_EVENT_HEADERS_SENT  fd=%d", *((int *)event_data));
            break;
        }
        case HTTP_SERVER_EVENT_ON_DATA:
        { /*!< Occurs when receiving data from the client */
            esp_http_server_event_data *evt_data = event_data;
            ESP_LOGI(TAG, "EVT: HTTP_SERVER_EVENT_ON_DATA received fd=%d, len=%d", evt_data->fd, evt_data->data_len);
            break;
        }
        case HTTP_SERVER_EVENT_SENT_DATA:
        { /*!< Occurs when an ESP HTTP server session is finished */
            esp_http_server_event_data *evt_data = event_data;
            ESP_LOGI(TAG, "EVT: HTTP_SERVER_EVENT_SENT_DATA sent fd=%d, len=%d", evt_data->fd, evt_data->data_len);
            break;
        }
        case HTTP_SERVER_EVENT_DISCONNECTED:
        { /*!< The connection has been disconnected */
            ESP_LOGI(TAG, "EVT: c fd=%d", *((int *)event_data));
            break;
        }
        case HTTP_SERVER_EVENT_STOP:
        { /*!< This event occurs when HTTP Server is stopped */
            ESP_LOGI(TAG, "EVT: HTTP_SERVER_EVENT_STOP server=%p", (void *)event_data);
            break;
        }
        default:
        {
        }
            ESP_LOGI(TAG, "WiFi event: %d", (int)event_id);
            break;
        }
    }
    else if (strcmp(event_base, WIFI_EVENT) == 0)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi started, connecting...");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from WiFi, reconnecting...");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Connected to WiFi.");
            break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            ESP_LOGI(TAG, "WiFi auth mode changed.");
            break;
        case WIFI_EVENT_STA_WPS_ER_FAILED:
            ESP_LOGI(TAG, "WPS failed.");
            break;
        default:
            ESP_LOGI(TAG, "WiFi event: %d", (int)event_id);
            break;
        }
    }
    else if (strcmp(event_base, IP_EVENT) == 0)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
            _State = 1;
            break;
        }
        case IP_EVENT_STA_LOST_IP:
        {
            ESP_LOGI(TAG, "Lost IP Address.");
            break;
        }
        default:
        {
            ESP_LOGI(TAG, "IP event: %d", (int)event_id);
            break;
        }
        }
    }
}
#else
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "Wi-Fi клиент стартиран");
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "Свързан с мрежата: %s", WIFI_SSID);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "Получен IP адрес: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "Разкачен от мрежата");
        esp_wifi_connect();
        break;
    default:
        break;
    }
    return ESP_OK;
}
#endif
// Настройка на Wi-Fi мрежата
void wifi_init_sta()
{
    // Инициализация на мрежовия интерфейс
    esp_netif_init();
    esp_event_loop_create_default();

    // Създаване на Wi-Fi конфигурация
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // Създаване на STA мрежов интерфейс
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    // Регистрирайте събитията за WiFi и IP
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
   

    // Настройка на Wi-Fi мрежата в станция (STA) режим
    wifi_config_t wifi_config = { 0 };
    if(ESP_OK != load_wifi_credentials((char*)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid),(char*)wifi_config.sta.password, sizeof(wifi_config.sta.password)))
    {
        ESP_LOGE(TAG, "Can't load WIFI Credential!");
     //   start_softap();  
        return;
    }
    ESP_LOGE(TAG, "U:%s, P:%s" ,(char*)wifi_config.sta.ssid, (char*)wifi_config.sta.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));                   // Режим за клиент (STA)
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config)); // Настройка на мрежата
    ESP_ERROR_CHECK(esp_wifi_start());                                   // Старт на Wi-Fi
}

// Base64 конверсия
int base64_encode(char *output, const char *input, int length)
{
    const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, j;
    for (i = 0, j = 0; i < length; i += 3)
    {
        unsigned char b3[3], b4[4];
        int n = 0;

        // Вземи 3 байта от входа
        for (int k = 0; k < 3; k++)
        {
            if (i + k < length)
            {
                b3[k] = input[i + k];
                n++;
            }
            else
            {
                b3[k] = 0;
            }
        }

        // Преобразувай 3 байта в 4 base64 символа
        b4[0] = (b3[0] >> 2) & 0x3F;
        b4[1] = ((b3[0] & 0x03) << 4) | (b3[1] >> 4);
        b4[2] = ((b3[1] & 0x0F) << 2) | (b3[2] >> 6);
        b4[3] = b3[2] & 0x3F;

        for (int k = 0; k < n + 1; k++)
        {
            output[j++] = base64_table[b4[k]];
        }

        // Добавяне на "=" за допълване на base64 низ
        while (n++ < 3)
        {
            output[j++] = '=';
        }
    }
    output[j] = '\0';
    return j;
}
static uint32_t publicated;
//(esp_mqtt_event_handle_t event)
static void mqtt_event_handler(void *event_handler_arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    //    esp_mqtt_client_handle_t  client = event->client;
    //    esp_mqtt_client_handle_t client = event_data;
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        _State = 3;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        _State = 2;
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        publicated = 1;
        break;
    case MQTT_EVENT_DATA:
        event->data[event->data_len] = 0;
        ESP_LOGI(TAG, "MQTT_EVENT_DATA, topic=%s, data=%s", event->topic, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", (int)event_id);
        break;
    }
}

#define MQTT_TOPIC "homeassistant/camera/esp32_camera_1/image"
#define MQTT_CFG_TOPIC "homeassistant/camera/esp32_camera_1/config"

 
 

static const char *get_cam_format_name(pixformat_t pixel_format)
{
    switch (pixel_format)
    {
    case PIXFORMAT_JPEG:
        return "JPEG";
    case PIXFORMAT_RGB565:
        return "RGB565";
    case PIXFORMAT_RGB888:
        return "RGB888";
    case PIXFORMAT_YUV422:
        return "YUV422";
    // PIXFORMAT_RGB565,    // 2BPP/RGB565
    // PIXFORMAT_YUV422,    // 2BPP/YUV422
    // PIXFORMAT_YUV420,    // 1.5BPP/YUV420
    // PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
    // PIXFORMAT_JPEG,      // JPEG/COMPRESSED
    // PIXFORMAT_RGB888,    // 3BPP/RGB888
    // PIXFORMAT_RAW,       // RAW
    // PIXFORMAT_RGB444,    // 3BP2P/RGB444
    // PIXFORMAT_RGB555,    // 3BP2P/RGB555
    default:
        break;
    }
    return "UNKNOW";
}

int send_camera_config_via_mqtt(esp_mqtt_client_handle_t client)
{
    // Буфер за съхранение на JSON низа
    char json_string[256];

    // Създаване на JSON като текстов низ
    sprintf(json_string,
            "{"
            "\"name\": \"ESP32 Camera 1\","
            "\"unique_id\": \"esp32_camera_1\","
            "\"topic\": \"%s\","
            "\"device\": {"
            "\"identifiers\": \"esp32_camera_1\","
            "\"name\": \"ESP32 Camera Device\","
            "\"manufacturer\": \"ESP32\","
            "\"model\": \"ESP32-CAM\""
            "}"
            "}",
            MQTT_TOPIC);

    // Публикуване на JSON текста през MQTT
    int msg_id = esp_mqtt_client_publish(client, MQTT_CFG_TOPIC, json_string, 0, 1, 0);
    if (msg_id >= 0)
    {
        ESP_LOGI(TAG, "JSON published with msg_id=%d", msg_id);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish JSON");
    }
    return (msg_id >= 0);
}

// Функция за заснемане на изображение и публикуването му през MQTT
void send_camera_image_via_mqtt(esp_mqtt_client_handle_t client)
{
    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic)
    {
        ESP_LOGE(TAG, "Failed to capture image");
        return;
    }

    // Публикуване на изображението като binary payload
    esp_mqtt_client_publish(client, MQTT_TOPIC, (const char *)pic->buf, pic->len, 1, 0);
    // // Конвертиране на изображението в Base64
    // char base64Image[(pic->len * 4 / 3) + 4]; // За Base64 резултат
    // int base64_len = base64_encode(base64Image, (const char*)pic->buf, pic->len);
    // // Публикуване на изображението като binary payload
    // esp_mqtt_client_publish(client, MQTT_TOPIC, (const char *)base64Image, base64_len, 0, 0);

    // Освобождаване на буфера
    esp_camera_fb_return(pic);

    //   ESP_LOGI(TAG, "Image format '%s' size '%dx%d' sent to topic %s", get_cam_format_name(pic->format), pic->height, pic->width, MQTT_TOPIC);
}

#if 1
#include "nvs_flash.h"
#include "esp_littlefs.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265
#define SAMPLE_INTERVAL_MS 1000
static httpd_handle_t server = NULL;
static int ws_fd = -1;
static bool send_sine_wave = false;

#define CHECK_RESULT(x, y)                                                       \
    if ((x) != (y))                                                              \
    {                                                                            \
        ESP_LOGE(TAG, "Error in File: %s, Line %d ", __FILE__, __LINE__); \
        return ESP_FAIL;                                                         \
    }

typedef struct
{
    uint16_t gzip_header : 1;
    uint16_t conn_close : 1;
} add_http_headers_t;

typedef struct
{
    const char *artefact;
    add_http_headers_t hdrs;
} http_request_ctx;


// Handler for serving the root page
esp_err_t root_get_handler(httpd_req_t *req)
{
    char fName[40] = "/littlefs";
    int sockfd = httpd_req_to_sockfd(req);
    http_request_ctx* ctx = req->user_ctx;
    // if(ctx) {
    // //httpd_resp_set_hdr(req, "Connection", "close");
    //     add_response_headers(req, &ctx->hdrs);
    // }
    // CHECK_RESULT(ESP_OK, httpd_resp_set_type(req, "text/css"));
    // CHECK_RESULT(ESP_OK, httpd_resp_set_hdr(req, "Connection", "close"));

    if (ctx != NULL && ctx->artefact != NULL)
    {
        strncat(fName, ctx->artefact, 39);
    }
    else
    {
        strncat(fName, req->uri, 39);
    }

    fName[39] = 0;
    ESP_LOGI(TAG, "HTTP[%d] URI %s fname: %s", sockfd, req->uri, fName);
    // Read the HTML file and serve it
    FILE *f = fopen(fName, "r");
    if (f == NULL)
    {
        f = fopen("/littlefs/index.html", "r");
        if (f == NULL)
        {
            ESP_LOGE(TAG, "Can't open %s", fName);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }

    char buffer[512];
    size_t read_len;
    httpd_resp_set_type(req, "text/css");
    while ((read_len = fread(buffer, 1, sizeof(buffer), f)) > 0)
    {
        httpd_resp_send_chunk(req, buffer, read_len);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // End the response
    httpd_resp_send(req, "Done", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t add_response_headers(httpd_req_t *req, add_http_headers_t *hdrs)
{

    if (NULL == hdrs){
        return ESP_FAIL;
    }
    if (hdrs->gzip_header)
    {
        CHECK_RESULT(ESP_OK, httpd_resp_set_hdr(req, "Content-Encoding", "gzip"));
        CHECK_RESULT(ESP_OK, httpd_resp_set_type(req, "text/html"));
    }
    if (hdrs->gzip_header)
    {
        CHECK_RESULT(ESP_OK, httpd_resp_set_hdr(req, "Connection", "close"));
    }
    return ESP_OK;
}

esp_err_t set_content_type(httpd_req_t *req, const char *file_name) {
    if (strstr(file_name, ".html") != NULL) {
        httpd_resp_set_type(req, "text/html");
    } else if (strstr(file_name, ".css") != NULL) {
        httpd_resp_set_type(req, "text/css");
    } else if (strstr(file_name, ".js") != NULL) {
        httpd_resp_set_type(req, "application/javascript");
    } else if (strstr(file_name, ".jpg") != NULL || strstr(file_name, ".jpeg") != NULL) {
        httpd_resp_set_type(req, "image/jpeg");
    } else if (strstr(file_name, ".png") != NULL) {
        httpd_resp_set_type(req, "image/png");
    } else if (strstr(file_name, ".gz") != NULL) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    } else {
        httpd_resp_set_type(req, "application/octet-stream");
    }
    return ESP_OK;
}




esp_err_t root_post_handler(httpd_req_t *req)
{
        // Размер на буфера за данни, които ще получим
        char buffer[512];
        int ret, remaining = req->content_len;
        ESP_LOGI(TAG, "Received POST data");
        // Четене на данни от POST заявката
        if (remaining > sizeof(buffer)) {
            ESP_LOGE(TAG, "Request body is too large for the buffer.");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    
        // Четене на данни в буфера
        ret = httpd_req_recv(req, buffer, remaining);
        if (ret <= 0) {
            ESP_LOGE(TAG, "Failed to read POST data.");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    
        // Нулиране на последния символ (ако е необходимо)
        buffer[ret] = '\0';
    
        // Логване на получените данни
        ESP_LOGI(TAG, "Received POST data: %s", buffer);
    
        
        cJSON *json = cJSON_Parse((char *)buffer);
        if (json == NULL)
        {
            ESP_LOGE(TAG, "Error parsing JSON");
            return ESP_FAIL;
        }
            // Извличане на стойностите за "username" и "password"
        cJSON *username_item = cJSON_GetObjectItem(json, "username");
        cJSON *password_item = cJSON_GetObjectItem(json, "password");
        const char *response = "{\"success\" : false}";
        if (cJSON_IsString(username_item) && (username_item->valuestring != NULL) &&
            cJSON_IsString(password_item) && (password_item->valuestring != NULL)) {
            
            const char *username = username_item->valuestring;
            const char *password = password_item->valuestring;

            ESP_LOGI(TAG, "Username: %s, Password: %s", username, password);
            if (!strcmp(username, "admin")) {
                if(!strcmp(password, "password")){
                     response = "{\"success\":true}";
                     ESP_LOGE(TAG, "URA: Login request is ok");
                }
                else{
                    ESP_LOGE(TAG, "Error: user: %s password %s", username, password);
                }
            } else {
                ESP_LOGE(TAG, "Error: No user name");
            }
            // Тук може да се извърши проверка за валидност на потребителските данни
        } else {
            ESP_LOGE(TAG, "Invalid JSON structure for username/password.");
            cJSON_Delete(json);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        cJSON_Delete(json);
        // Отговор към клиента (може да е JSON, текст или друг формат)
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    
        return ESP_OK;
    }

// Handler for serving the root page
esp_err_t root_get_handler_new(httpd_req_t *req)
{
    char fName[40] = "/littlefs";
    int sockfd = httpd_req_to_sockfd(req);
    http_request_ctx* ctx = req->user_ctx;
    // if(ctx) {
    // //httpd_resp_set_hdr(req, "Connection", "close");
    //     add_response_headers(req, &ctx->hdrs);
    // }
    httpd_resp_set_type(req, "text/css");
    if (ctx != NULL && ctx->artefact != NULL)
    {   nvs_handle_t nvs_handle;
        strncat(fName, ctx->artefact, 39);
    }
    else
    {
        strncat(fName, req->uri, 39);
    }
    
    set_content_type(req, fName);
    fName[39] = 0;
    ESP_LOGI(TAG, "HTTP[%d] URI %s fname: %s", sockfd, req->uri, fName);
    // Read the HTML file and serve it
    FILE *f = fopen(fName, "r");
    if(f == NULL)
    {
        // f = fopen("/littlefs/index.html", "r");
        // if (f == NULL)
        // {
        //     ESP_LOGE(TAG, "Can't open %s", fName);
        //     httpd_resp_send_500(req);
        //     return ESP_FAIL;
        // }
        // ESP_LOGI(TAG, "HTTP[%d] URI %s fname: /littlefs/index.html", sockfd, req->uri);
        return ESP_FAIL;
    }

    char buffer[512];
    size_t read_len;
    while ((read_len = fread(buffer, 1, sizeof(buffer), f)) > 0)
    {
        httpd_resp_send_chunk(req, buffer, read_len);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // End the response
    httpd_resp_send(req, "Done", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}



static void send_sine_wave_task(void *arg)
{
    float angle = 0;
    while (1)
    {
#if 0

        if (send_sine_wave && ws_fd != -1) {
            char buffer[32];
            float value = 100*sin(angle);
            snprintf(buffer, sizeof(buffer), "%.3f", value);
            
            httpd_ws_frame_t ws_pkt = {
                .final = true,
                .fragmented = false,
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)buffer,
                .len = strlen(buffer)
            };
            httpd_ws_send_frame_async(server, ws_fd, &ws_pkt);
            
            angle += 2 * PI / 100; // Increase angle for next sample
            if (angle >= 2 * PI) angle -= 2 * PI;
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
#else
        if (send_sine_wave && ws_fd != -1)
        {
            camera_fb_t *pic = esp_camera_fb_get();
            httpd_ws_frame_t ws_pkt;
            if (!pic)
            {
                ESP_LOGE(TAG, "Failed to capture image");
             //   return;
            }

            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.type = HTTPD_WS_TYPE_BINARY;
            ws_pkt.payload = pic->buf;
            ws_pkt.len = pic->len;

            httpd_ws_send_frame_async(server, ws_fd, &ws_pkt);
            esp_camera_fb_return(pic);
        }
      //  vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
#endif
    }
}

typedef struct
{
    ledc_timer_t ledc_timer;     /*!< LEDC timer to be used for generating XCLK  */
    ledc_channel_t ledc_channel; /*!< LEDC channel to be used for generating XCLK  */

    pixformat_t pixel_format; /*!< Format of the pixel data: PIXFORMAT_ + YUV422|GRAYSCALE|RGB565|JPEG  */
    framesize_t frame_size;   /*!< Size of the output image: FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA  */

    int jpeg_quality;                 /*!< Quality of JPEG output. 0-63 lower means higher quality  */
    size_t fb_count;                  /*!< Number of frame buffers to be allocated. If more than one, then each frame will be acquired (double speed)  */
    camera_fb_location_t fb_location; /*!< The location where the frame buffer will be allocated */
    camera_grab_mode_t grab_mode;     /*!< When buffers should be filled */
#if CONFIG_CAMERA_CONVERTER_ENABLED
    camera_conv_mode_t conv_mode; /*!< RGB<->YUV Conversion mode */
#endif

    int sccb_i2c_port; /*!< If pin_sccb_sda is -1, use the already configured I2C bus by number */
} cam_config_t;

static cam_config_t camConfig = {.pixel_format = PIXFORMAT_YUV422, .frame_size = 10, .jpeg_quality = 75, .fb_count = 2};

void trans_complete_cb(esp_err_t err, int socket, void *arg)
{
    ESP_LOGI(TAG, "httpd_ws_send_data_async sent %d", (int)arg);
}


// Функция за обработка на WebSocket съобщения
esp_err_t ws_handler(httpd_req_t *req)
{
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    int len;
    int sockfd = httpd_req_to_sockfd(req);
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Websocket Handshake done, the new connection was opened fd = %d", sockfd);
        ws_fd = sockfd;
        return ESP_OK;
    }
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "WS [%d] Packet type:%d  Frame len is %d", sockfd, ws_pkt.type, ws_pkt.len);
    if (ws_pkt.len)
    {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);

        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
    }
    // Проверка за тип на съобщението
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
    {
        ESP_LOGI(TAG, "WebSocket closed, fd=%d", sockfd);
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
    {

        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "WS [%d] packet with message: %s", sockfd, ws_pkt.payload);
        if (strcmp((char *)ws_pkt.payload, "start") == 0)
        {
            send_sine_wave = true;
        }
        else if (strcmp((char *)ws_pkt.payload, "stop") == 0)
        {
            send_sine_wave = false;
        }
        cJSON *json = cJSON_Parse((char *)buf);
        if (json == NULL)
        {
            ESP_LOGE(TAG, "Error parsing JSON");
            free(buf);
            return ESP_FAIL;
        }

        const cJSON *command = cJSON_GetObjectItemCaseSensitive(json, "command");
        if (command != NULL)
        {
            if (cJSON_IsString(command))
            {
                if (strcmp(command->valuestring, "set_config") == 0)
                {
                    cJSON *config = cJSON_GetObjectItemCaseSensitive(json, "config");
                    if (config != NULL)
                    {
                        int sockfd = httpd_req_to_sockfd(req);
                        // Извличане на конфигурация от JSON
                        cJSON *pixel_format = cJSON_GetObjectItemCaseSensitive(config, "pixel_format");
                        cJSON *frame_size = cJSON_GetObjectItemCaseSensitive(config, "frame_size");
                        cJSON *jpeg_quality = cJSON_GetObjectItemCaseSensitive(config, "jpeg_quality");
                        cJSON *fb_count = cJSON_GetObjectItemCaseSensitive(config, "fb_count");

                        if (pixel_format != NULL && frame_size != NULL && jpeg_quality != NULL && fb_count != NULL)
                        {
                            // Записване на новата конфигурация
                            //  strncpy(camConfig.pixel_format, pixel_format->valuestring, sizeof(camConfig.pixel_format) - 1);
                            camConfig.frame_size = frame_size->valueint;
                            camConfig.jpeg_quality = jpeg_quality->valueint;
                            camConfig.fb_count = fb_count->valueint;

                            ESP_LOGI(TAG, "WS[%d] Camera config updated: %d, %d, %d, %d", sockfd, camConfig.pixel_format, camConfig.frame_size, camConfig.jpeg_quality, camConfig.fb_count);

                            ESP_LOGI(TAG, "httpd_ws_send_frame error: %d", httpd_ws_send_frame_async(server, sockfd, &ws_pkt));
                        }
                    }
                }
                else if (strcmp(command->valuestring, "start") == 0)
                {
                    // Старт на камерата или видео потока
                    ESP_LOGI(TAG, "Starting camera stream...");
                    // Добави код за стартиране на видеопотока
                    send_sine_wave = true;
                }
                else if (strcmp(command->valuestring, "stop") == 0)
                {
                    // Спиране на камерата или видео потока
                    ESP_LOGI(TAG, "Stopping camera stream...");
                    // Добави код за спиране на видеопотока
                    send_sine_wave = false;
                }
            }
        }
        cJSON_Delete(json);
    }
    else
    {
        ESP_LOGI(TAG, "WebSocket other frame, type=%d, fd=%d", ws_pkt.type, sockfd);
    }
    if (NULL != buf)
    {
        free(buf);
    }
    size_t fds = 10;
    int client_fds[10];
    if (ESP_OK == httpd_get_client_list(server, &fds, client_fds))
    {
        ESP_LOGI(TAG, "---SERVER client list--");
        for (int i = 0; i < fds; i++)
        {
            ESP_LOGI(TAG, "%d ", client_fds[i]);
        }
    }
    return ESP_OK;
}
#endif

// Initialize LittleFS and web server
httpd_handle_t start_webserver(void)
{

    // Initialize the HTTP server
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Mount LittleFS
    esp_vfs_littlefs_conf_t mount_config = {
        .format_if_mount_failed = true,
        .partition_label = "littlefs",
        .base_path = "/littlefs",
        .dont_mount = 0};

    esp_err_t ret = esp_vfs_littlefs_register(&mount_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount LittleFS (error code %d)", ret);
        return server;
    }

    config.uri_match_fn = httpd_uri_match_wildcard;

    ret = httpd_start(&server, &config);
    ESP_LOGE(TAG, "httpd_start (error code %d)", ret);
    if (ret == ESP_OK)
    {
        static http_request_ctx ctx[] = {
            {
                .artefact = NULL,
                .hdrs.conn_close = 1,
                .hdrs.gzip_header = 0,
            },
            {
                .artefact = "/index.html",
                .hdrs.conn_close = 1,
                .hdrs.gzip_header = 0,
            },
            {
                .artefact = "/edit.min.htm.gz",
                .hdrs.conn_close = 1,
                .hdrs.gzip_header = 1,
            },
        };
        
        // Register root URI handler
        httpd_uri_t uri_get_new = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = root_get_handler_new,
            .user_ctx = NULL};
        static const httpd_uri_t ws = {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = ws_handler,
            .user_ctx = NULL,
            .is_websocket = true};       
            
         
        // Register root URI handler
       /* httpd_uri_t uri_get = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = &ctx[1]};



        httpd_register_uri_handler(server, &uri_get);
        uri_get.uri = "/index.html";
        uri_get.user_ctx = &ctx[0];
        httpd_register_uri_handler(server, &uri_get);
        uri_get.uri = "/script.js";
        httpd_register_uri_handler(server, &uri_get);
        uri_get.uri = "/style.css";
        httpd_register_uri_handler(server, &uri_get);
        uri_get.uri = "/edit";
        uri_get.user_ctx = &ctx[2];
        httpd_register_uri_handler(server, &uri_get);*/
        httpd_register_uri_handler(server, &ws);
        uri_get_new.uri = "/";
        uri_get_new.user_ctx = &ctx[1];
        httpd_register_uri_handler(server, &uri_get_new);
        uri_get_new.uri = "/edit";
        uri_get_new.user_ctx = &ctx[2];
        httpd_register_uri_handler(server, &uri_get_new);
        uri_get_new.uri = "/*";
        uri_get_new.user_ctx = NULL;
        httpd_register_uri_handler(server, &uri_get_new);
        uri_get_new.uri = "/login";
        uri_get_new.method = HTTP_POST;
        uri_get_new.handler = root_post_handler;
        uri_get_new.user_ctx = NULL;
        httpd_register_uri_handler(server, &uri_get_new);
    }

    return server;
}



#include "nvs_flash.h"
#include "nvs.h"

esp_err_t save_wifi_credentials(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, "wifi_ssid", ssid);
        err = nvs_set_str(nvs_handle, "wifi_pass", password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    return err;
}

esp_err_t load_wifi_credentials(char *ssid, size_t ssid_size, char *password, size_t pass_size) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        err |= nvs_get_str(nvs_handle, "wifi_ssid", ssid, &ssid_size);
        err |=nvs_get_str(nvs_handle, "wifi_pass", password, &pass_size);
        nvs_close(nvs_handle);
    }
    return err;
}

void app_main(void)
{
    // Инициализация на NVS (Non-Volatile Storage), която е необходима за Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    

    // Инициализация на камерата
    if (init_camera() == ESP_OK)
    {
        ESP_LOGI(TAG, "Camera Initialized Successfully!");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to Initialize Camera.");
        //  return;
    }
    // Инициализация на Wi-Fi клиента
    ESP_LOGI(TAG, "Старт на Wi-Fi");
    wifi_init_sta();
 vTaskDelay(1/ portTICK_RATE_MS); //DELL ME replace with event
    // MQTT конфигурация

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "ws://192.168.88.233:8083/mqtt", // URI с WebSocket (ws:// за нешифрован, wss:// за шифрован)
        // .broker.address.path = "",
        //.broker.address.transport = MQTT_TRANSPORT_OVER_WS,
        //.broker.address.port = 8083,
        .credentials.username = "esp32",                // Потребителско име
        .credentials.authentication.password = "esp32", // Парола
        //.broker.verification.skip_cert_common_name_check = true, // Пропускане на проверката на CN
        //.broker.verification.certificate = NULL,
        .network.timeout_ms = 10000, // Време за изчакване (10 секунди)
    };
    esp_mqtt_client_handle_t client = NULL;
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_DEBUG);
    esp_log_level_set("TRANSPORT", ESP_LOG_DEBUG);
    esp_log_level_set("OUTBOX", ESP_LOG_DEBUG);

#if 1
   
    // Start web server
    server = start_webserver();
    if (server != NULL)
    {
        ESP_LOGI(TAG, "Web server started Successfully!");
        xTaskCreate(send_sine_wave_task, "sine_task", 4096, NULL, 5, NULL);
    }
    else
    {
        ESP_LOGE(TAG, "Can't start Web server!");
    }
#else
    while (1)
    {
        if (1 == _State)
        {
            client = esp_mqtt_client_init(&mqtt_cfg);
            if (client != NULL)
            {
                // Регистриране на обработчик за събития
                esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, client);
                ESP_ERROR_CHECK(esp_mqtt_client_start(client));
                ESP_LOGI(TAG, "Start mqtt client!");
                _State = 2;
            }
            else
            {
                ESP_LOGI(TAG, "Can't initilize mqtt client!");
            }
        }
        else if (3 == _State)
        {
            // // Подписка към топик след успешна връзка
            // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            // ESP_LOGI(TAG, "sent subscribe, msg_id=%d", msg_id);
            if (send_camera_config_via_mqtt(client))
            {
                _State = 4;
            }
        }
        else if (4 == _State)
        {
            publicated = 0;
            send_camera_image_via_mqtt(client);
            do
            {
                vTaskDelay(1 / portTICK_RATE_MS);
            } while (!publicated);
        }

        // vTaskDelay(100 / portTICK_RATE_MS);
    }
    // Може да добавиш още логика тук за работа с камерата
#endif
}
