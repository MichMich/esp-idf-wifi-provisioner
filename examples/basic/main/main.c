/*
 * SPDX-FileCopyrightText: 2026 Michael Teeuw
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Basic example: connect to a stored network or start captive portal.
 */

#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "wifi_provisioner.h"

static const char *TAG = "example";

static void on_connected(void)
{
    ESP_LOGI(TAG, "WiFi connected!");
}

static void on_portal_start(void)
{
    ESP_LOGI(TAG, "Captive portal started — connect to the AP to configure WiFi.");
}

void app_main(void)
{
    /* Initialise NVS — required for credential storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Configure the provisioner */
    wifi_prov_config_t config = WIFI_PROV_DEFAULT_CONFIG();
    config.ap_ssid        = "MyDevice-Setup";
    config.on_connected   = on_connected;
    config.on_portal_start = on_portal_start;

    ESP_ERROR_CHECK(wifi_prov_start(&config));

    /* Block until we have a WiFi connection */
    ESP_ERROR_CHECK(wifi_prov_wait_for_connection(portMAX_DELAY));

    ESP_LOGI(TAG, "Connected — application continues here.");
}
