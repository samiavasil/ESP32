#ifndef ESP32_CAMERA_LIB_H
#define ESP32_CAMERA_LIB_H

#include "esp_err.h"
#include "esp_camera.h"

// Инициализация на камерата
esp_err_t init_camera(void);

// Вземане на снимка и връщане на размера й
size_t take_picture(void);

#endif // ESP32_CAMERA_LIB_H
