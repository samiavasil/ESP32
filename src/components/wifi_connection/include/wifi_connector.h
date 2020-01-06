/*
 * wifi_connector.h
 *
 *  Created on: Nov 18, 2019
 *      Author: vvasilev
 */

#ifndef COMPONENTS_WIFI_CONNECTION_INCLUDE_WIFI_CONNECTOR_H_
#define COMPONENTS_WIFI_CONNECTION_INCLUDE_WIFI_CONNECTOR_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "esp_err.h"

typedef enum{
	WIFI_WPA,
	WIFI_WPA_ENT,
	WIFI_WFS,
}ewifi_mode_t;

/**
 * @brief Configure Wi-Fi or Ethernet, connect, wait for IP
 *
 * @return ESP_OK on successful connection
 */

esp_err_t wifi_connect(void);

#if 0
void wifi_disconnect(void);

bool wifi_set_mode( ewifi_mode_t mode );

bool wifi_set_ssid(char* ssid);

bool wifi_set_credentials(char* user_name, char* passw);

bool wifi_set


bool wifi_set_configuration();
bool wifi_set_credentials(char* user_name, char* passw);
bool wifi_set_(char* user_name, char* passw);
#endif

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_WIFI_CONNECTION_INCLUDE_WIFI_CONNECTOR_H_ */
