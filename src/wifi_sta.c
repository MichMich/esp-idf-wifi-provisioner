/*
 * SPDX-FileCopyrightText: 2026 Michael Teeuw
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Station (STA) connect and retry logic.
 */

#include "wifi_prov_internal.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"

#define STA_CONNECTED_BIT BIT0
#define STA_FAILED_BIT    BIT1

static const char *TAG = "wifi_prov_sta";

static EventGroupHandle_t s_event_group;
static uint8_t s_retries;
static uint8_t s_max_retries;

static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retries < s_max_retries) {
            s_retries++;
            ESP_LOGI(TAG, "Retry %d/%d …", s_retries, s_max_retries);
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "Connection failed after %d retries", s_max_retries);
            xEventGroupSetBits(s_event_group, STA_FAILED_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Connected – IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retries = 0;
        xEventGroupSetBits(s_event_group, STA_CONNECTED_BIT);
    }
}

esp_err_t wifi_sta_connect(const char *ssid, const char *password,
                           uint8_t max_retries)
{
    s_retries     = 0;
    s_max_retries = max_retries;
    s_event_group = xEventGroupCreate();

    esp_event_handler_instance_t wifi_handler;
    esp_event_handler_instance_t ip_handler;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
        &event_handler, NULL, &wifi_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        &event_handler, NULL, &ip_handler));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to \"%s\" …", ssid);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_event_group,
        STA_CONNECTED_BIT | STA_FAILED_BIT,
        pdTRUE, pdFALSE, portMAX_DELAY);

    esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_handler);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_handler);
    vEventGroupDelete(s_event_group);
    s_event_group = NULL;

    if (bits & STA_CONNECTED_BIT) {
        return ESP_OK;
    }

    esp_wifi_stop();
    return ESP_FAIL;
}
