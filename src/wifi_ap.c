/*
 * SPDX-FileCopyrightText: 2026 Michael Teeuw
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Soft-AP setup and teardown.
 */

#include "wifi_prov_internal.h"
#include "esp_wifi.h"

static const char *TAG = "wifi_prov_ap";

static esp_netif_t *s_ap_netif = NULL;

esp_err_t wifi_ap_start(const wifi_prov_config_t *config)
{
    s_ap_netif = esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta(); /* needed for scan in APSTA mode */

    wifi_config_t wifi_config = {
        .ap = {
            .channel        = config->ap_channel,
            .max_connection = config->ap_max_connections,
            .authmode       = WIFI_AUTH_OPEN,
        },
    };

    strncpy((char *)wifi_config.ap.ssid, config->ap_ssid,
            sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(config->ap_ssid);

    if (config->ap_password && config->ap_password[0] != '\0') {
        strncpy((char *)wifi_config.ap.password, config->ap_password,
                sizeof(wifi_config.ap.password) - 1);
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started – SSID: \"%s\", channel: %d",
             config->ap_ssid, config->ap_channel);
    return ESP_OK;
}

esp_err_t wifi_ap_stop(void)
{
    esp_err_t err = esp_wifi_stop();

    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }

    ESP_LOGI(TAG, "AP stopped");
    return err;
}
