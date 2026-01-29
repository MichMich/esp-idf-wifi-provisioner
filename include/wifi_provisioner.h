/*
 * SPDX-FileCopyrightText: 2026 Michael Teeuw
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_netif_types.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback fired when the device successfully connects as a station.
 */
typedef void (*wifi_prov_on_connected_cb_t)(void);

/**
 * Callback fired when the captive portal AP is started.
 */
typedef void (*wifi_prov_on_portal_start_cb_t)(void);

/**
 * Provisioner configuration.
 * Use WIFI_PROV_DEFAULT_CONFIG() to initialise with Kconfig defaults.
 */
typedef struct {
    const char *ap_ssid;
    const char *ap_password;
    uint8_t     ap_channel;
    uint8_t     ap_max_connections;
    uint8_t     max_retries;
    uint16_t    portal_timeout;          /* seconds, 0 = no timeout */
    uint16_t    http_port;
    wifi_prov_on_connected_cb_t    on_connected;
    wifi_prov_on_portal_start_cb_t on_portal_start;
} wifi_prov_config_t;

#define WIFI_PROV_DEFAULT_CONFIG() {                                        \
    .ap_ssid           = CONFIG_WIFI_PROV_AP_SSID,                          \
    .ap_password       = CONFIG_WIFI_PROV_AP_PASSWORD,                      \
    .ap_channel        = CONFIG_WIFI_PROV_AP_CHANNEL,                       \
    .ap_max_connections = CONFIG_WIFI_PROV_AP_MAX_CONNECTIONS,               \
    .max_retries       = CONFIG_WIFI_PROV_STA_MAX_RETRIES,                  \
    .portal_timeout    = CONFIG_WIFI_PROV_PORTAL_TIMEOUT,                   \
    .http_port         = CONFIG_WIFI_PROV_HTTP_PORT,                        \
    .on_connected      = NULL,                                              \
    .on_portal_start   = NULL,                                              \
}

/**
 * Initialise NVS, netif and the default event loop.
 * Call once from app_main() before wifi_prov_start().
 */
esp_err_t wifi_prov_init(void);

/**
 * Start the WiFi provisioner.
 *
 * Reads stored credentials from NVS and attempts to connect.
 * Falls back to AP + captive portal on failure.
 * Requires wifi_prov_init() to have been called first.
 */
esp_err_t wifi_prov_start(const wifi_prov_config_t *config);

/**
 * Stop the WiFi provisioner and release all resources.
 */
esp_err_t wifi_prov_stop(void);

/**
 * Block until a station connection is established.
 *
 * @param timeout_ticks  FreeRTOS ticks to wait (portMAX_DELAY for forever).
 * @return ESP_OK on connection, ESP_ERR_TIMEOUT on timeout.
 */
esp_err_t wifi_prov_wait_for_connection(TickType_t timeout_ticks);

/**
 * Erase stored WiFi credentials from NVS.
 */
esp_err_t wifi_prov_erase_credentials(void);

/**
 * Check whether the device is currently connected as a station.
 */
bool wifi_prov_is_connected(void);

/**
 * Retrieve the current station IP information.
 */
esp_err_t wifi_prov_get_ip_info(esp_netif_ip_info_t *ip_info);

#ifdef __cplusplus
}
#endif
