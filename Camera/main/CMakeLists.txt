
idf_component_register(SRCS main.c
                        PRIV_INCLUDE_DIRS . 
                        PRIV_REQUIRES esp_wifi mqtt esp32_camera_lib nvs_flash esp_psram spi_flash  littlefs esp_http_server json)
