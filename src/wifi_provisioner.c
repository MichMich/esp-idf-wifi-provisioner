/*
 * SPDX-FileCopyrightText: 2026 Michael Teeuw
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Main orchestration: boot flow, connect-or-provision logic.
 */

#include "wifi_prov_internal.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"

#define CONNECTED_BIT BIT0

static const char *TAG = "wifi_prov";

static wifi_prov_config_t s_config;
static esp_netif_t       *s_sta_netif = NULL;
static EventGroupHandle_t s_connected_event;
static bool               s_connected = false;

/* Event base declared/defined in http_server.c */
ESP_EVENT_DECLARE_BASE(WIFI_PROV_EVENT);
enum { WIFI_PROV_EVENT_CREDENTIALS_SET };

typedef struct {
    char ssid[33];
    char password[65];
} wifi_prov_creds_t;

/* ── Portal credential callback ─────────────────────────────────────── */

static void on_credentials_set(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    wifi_prov_creds_t *creds = (wifi_prov_creds_t *)data;
    ESP_LOGI(TAG, "Credentials received via portal, switching to STA …");

    /* Tear down portal */
    http_server_stop();
    dns_server_stop();
    wifi_ap_stop();

    /* Try connecting with the new credentials */
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init);

    esp_err_t err = wifi_sta_connect(creds->ssid, creds->password,
                                     s_config.max_retries);
    if (err == ESP_OK) {
        s_connected = true;
        xEventGroupSetBits(s_connected_event, CONNECTED_BIT);
        if (s_config.on_connected) {
            s_config.on_connected();
        }
    } else {
        ESP_LOGW(TAG, "Connection with new credentials failed, restarting portal");
        /* Re-launch AP + portal */
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        wifi_ap_start(&s_config);
        dns_server_start();
        http_server_start(s_config.http_port);
    }
}

/* ── Public API ─────────────────────────────────────────────────────── */

esp_err_t wifi_prov_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    return ESP_OK;
}

esp_err_t wifi_prov_start(const wifi_prov_config_t *config)
{
    s_config = *config;
    s_connected = false;
    s_connected_event = xEventGroupCreate();

    /* Register for portal credential events */
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_PROV_EVENT, WIFI_PROV_EVENT_CREDENTIALS_SET,
        on_credentials_set, NULL));

    /* Try loading stored credentials */
    char ssid[33]     = {0};
    char password[65] = {0};
    esp_err_t err = nvs_store_load(ssid, sizeof(ssid),
                                   password, sizeof(password));

    if (err == ESP_OK && ssid[0] != '\0') {
        ESP_LOGI(TAG, "Found stored credentials, attempting STA connection …");

        s_sta_netif = esp_netif_create_default_wifi_sta();
        wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wifi_init));

        err = wifi_sta_connect(ssid, password, s_config.max_retries);
        if (err == ESP_OK) {
            s_connected = true;
            xEventGroupSetBits(s_connected_event, CONNECTED_BIT);
            if (s_config.on_connected) {
                s_config.on_connected();
            }
            return ESP_OK;
        }

        ESP_LOGW(TAG, "STA connection failed, starting provisioning portal");
        /* wifi_sta_connect already called esp_wifi_stop() on failure,
           clean up the STA netif before starting AP */
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    } else {
        ESP_LOGI(TAG, "No stored credentials, starting provisioning portal");
    }

    /* Start AP + captive portal */
    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init));

    wifi_ap_start(&s_config);
    dns_server_start();
    http_server_start(s_config.http_port);

    if (s_config.on_portal_start) {
        s_config.on_portal_start();
    }

    return ESP_OK;
}

esp_err_t wifi_prov_stop(void)
{
    http_server_stop();
    dns_server_stop();
    wifi_ap_stop();
    esp_wifi_stop();
    esp_wifi_deinit();

    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }

    if (s_connected_event) {
        vEventGroupDelete(s_connected_event);
        s_connected_event = NULL;
    }

    esp_event_handler_unregister(WIFI_PROV_EVENT,
                                 WIFI_PROV_EVENT_CREDENTIALS_SET,
                                 on_credentials_set);

    s_connected = false;
    return ESP_OK;
}

esp_err_t wifi_prov_wait_for_connection(TickType_t timeout_ticks)
{
    if (s_connected) {
        return ESP_OK;
    }

    EventBits_t bits = xEventGroupWaitBits(s_connected_event,
        CONNECTED_BIT, pdFALSE, pdTRUE, timeout_ticks);

    return (bits & CONNECTED_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t wifi_prov_erase_credentials(void)
{
    return nvs_store_erase();
}

bool wifi_prov_is_connected(void)
{
    return s_connected;
}

esp_err_t wifi_prov_get_ip_info(esp_netif_ip_info_t *ip_info)
{
    if (!s_connected || !s_sta_netif) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_netif_get_ip_info(s_sta_netif, ip_info);
}
