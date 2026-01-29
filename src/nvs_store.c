/*
 * SPDX-FileCopyrightText: 2026 Michael Teeuw
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * NVS helpers: read/write/erase WiFi credentials.
 */

#include "wifi_prov_internal.h"
#include "nvs_flash.h"
#include "nvs.h"

#define NVS_NAMESPACE "wifi_prov"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "pass"

static const char *TAG = "wifi_prov_nvs";

esp_err_t nvs_store_load(char *ssid, size_t ssid_len,
                         char *password, size_t pass_len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No stored credentials (nvs_open: %s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_str(handle, NVS_KEY_SSID, ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No stored SSID (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_get_str(handle, NVS_KEY_PASS, password, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No stored password (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded credentials for SSID \"%s\"", ssid);
    return ESP_OK;
}

esp_err_t nvs_store_save(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, NVS_KEY_SSID, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save SSID (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_set_str(handle, NVS_KEY_PASS, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save password (%s)", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved credentials for SSID \"%s\"", ssid);
    }
    return err;
}

esp_err_t nvs_store_erase(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    nvs_erase_all(handle);
    err = nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "Erased stored credentials");
    return err;
}
