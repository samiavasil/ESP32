#include "esp32_camera_lib.h"
#include "esp_log.h"

static const char *TAG = "esp32_camera_lib";

// Камера конфигурация
static camera_config_t camera_config = {
    .pin_pwdn = 32,
    .pin_reset = -1,
    .pin_xclk = 0,
    .pin_sccb_sda = 26,
    .pin_sccb_scl = 27,
    .pin_d7 = 35,
    .pin_d6 = 34,
    .pin_d5 = 39,
    .pin_d4 = 36,
    .pin_d3 = 21,
    .pin_d2 = 19,
    .pin_d1 = 18,
    .pin_d0 = 5,
    .pin_vsync = 25,
    .pin_href = 23,
    .pin_pclk = 22,

    .xclk_freq_hz = 16000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_UXGA,
    .jpeg_quality = 12,
    .fb_count = 2,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

// Инициализиране на камерата
esp_err_t init_camera(void)
{
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
    return ESP_OK;
}

// Вземане на снимка
size_t take_picture(void)
{
    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic)
    {
        ESP_LOGE(TAG, "Camera capture failed");
        return 0;
    }

    size_t size = pic->len;
    ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", size);
    esp_camera_fb_return(pic);
    return size;
}
