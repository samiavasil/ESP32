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
/**
 * @brief Configure Wi-Fi or Ethernet, connect, wait for IP
 *
 * @return ESP_OK on successful connection
 */
esp_err_t wifi_connect(void);


#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_WIFI_CONNECTION_INCLUDE_WIFI_CONNECTOR_H_ */
