/*
 * SPDX-FileCopyrightText: 2026 Michael Teeuw
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Captive portal HTTP server: serves the config page and handles form submissions.
 */

#include "wifi_prov_internal.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_event.h"

#include <stdlib.h>

static const char *TAG = "wifi_prov_http";

static httpd_handle_t s_server = NULL;

/* ── Event posted when the user submits credentials ─────────────────── */

ESP_EVENT_DECLARE_BASE(WIFI_PROV_EVENT);
ESP_EVENT_DEFINE_BASE(WIFI_PROV_EVENT);

enum {
    WIFI_PROV_EVENT_CREDENTIALS_SET,
};

typedef struct {
    char ssid[33];
    char password[65];
} wifi_prov_creds_t;

/* ── HTML page ──────────────────────────────────────────────────────── */

static const char PORTAL_HTML[] =
    "<!DOCTYPE html>"
    "<html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WiFi Setup</title>"
    "<style>"
    "body{font-family:sans-serif;margin:0;padding:20px;background:#f5f5f5}"
    ".c{max-width:400px;margin:0 auto;background:#fff;padding:20px;border-radius:8px;"
    "box-shadow:0 2px 8px rgba(0,0,0,.1)}"
    "h1{margin-top:0;font-size:1.4em;color:#333}"
    "label{display:block;margin:12px 0 4px;font-size:.9em;color:#555}"
    "input[type=text],input[type=password],select{"
    "width:100%%;box-sizing:border-box;padding:10px;border:1px solid #ccc;"
    "border-radius:4px;font-size:1em}"
    "button{margin-top:16px;width:100%%;padding:12px;background:#2196F3;color:#fff;"
    "border:none;border-radius:4px;font-size:1em;cursor:pointer}"
    "button:hover{background:#1976D2}"
    ".net{padding:8px 12px;margin:4px 0;background:#f9f9f9;border-radius:4px;"
    "cursor:pointer;display:flex;justify-content:space-between}"
    ".net:hover{background:#e3f2fd}"
    ".rssi{color:#999;font-size:.85em}"
    "</style></head><body>"
    "<div class='c'>"
    "<h1>WiFi Setup</h1>"
    "<div id='nets'>Scanning&hellip;</div>"
    "<form method='POST' action='/save'>"
    "<label for='s'>SSID</label>"
    "<input type='text' id='s' name='ssid' required maxlength='32'>"
    "<label for='p'>Password</label>"
    "<input type='password' id='p' name='password' maxlength='64'>"
    "<button type='submit'>Connect</button>"
    "</form></div>"
    "<script>"
    "fetch('/scan').then(r=>r.json()).then(d=>{"
    "let h='';"
    "d.forEach(n=>{"
    "h+='<div class=\"net\" onclick=\"document.getElementById(\\'s\\').value=\\''+n.ssid+'\\';\">'+"
    "n.ssid+'<span class=\"rssi\">'+n.rssi+' dBm</span></div>';"
    "});"
    "document.getElementById('nets').innerHTML=h||'No networks found.';"
    "}).catch(()=>{document.getElementById('nets').innerHTML='Scan failed.';});"
    "</script>"
    "</body></html>";

/* ── Handlers ───────────────────────────────────────────────────────── */

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t scan_handler(httpd_req_t *req)
{
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_records = NULL;

    wifi_scan_config_t scan_cfg = {
        .show_hidden = false,
    };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }

    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "[]", 2);
    }

    ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_records) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_ERR_NO_MEM;
    }
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    /* Build JSON array */
    char *json = malloc(ap_count * 80 + 4);
    if (!json) {
        free(ap_records);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    char *p = json;
    *p++ = '[';
    for (int i = 0; i < ap_count; i++) {
        if (i > 0) *p++ = ',';
        p += sprintf(p, "{\"ssid\":\"%s\",\"rssi\":%d}",
                     (char *)ap_records[i].ssid, ap_records[i].rssi);
    }
    *p++ = ']';
    *p   = '\0';

    free(ap_records);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char buf[256];
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[received] = '\0';

    wifi_prov_creds_t creds = {0};

    /* Parse "ssid=...&password=..." */
    char *ssid_start = strstr(buf, "ssid=");
    char *pass_start = strstr(buf, "password=");

    if (!ssid_start) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }

    ssid_start += 5; /* skip "ssid=" */
    char *ssid_end = strchr(ssid_start, '&');
    size_t ssid_len = ssid_end ? (size_t)(ssid_end - ssid_start)
                               : strlen(ssid_start);
    if (ssid_len >= sizeof(creds.ssid)) ssid_len = sizeof(creds.ssid) - 1;
    memcpy(creds.ssid, ssid_start, ssid_len);

    if (pass_start) {
        pass_start += 9; /* skip "password=" */
        char *pass_end = strchr(pass_start, '&');
        size_t pass_len = pass_end ? (size_t)(pass_end - pass_start)
                                   : strlen(pass_start);
        if (pass_len >= sizeof(creds.password)) pass_len = sizeof(creds.password) - 1;
        memcpy(creds.password, pass_start, pass_len);
    }

    ESP_LOGI(TAG, "Received credentials – SSID: \"%s\"", creds.ssid);

    /* Save to NVS */
    nvs_store_save(creds.ssid, creds.password);

    /* Send confirmation page */
    const char *resp =
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>WiFi Setup</title>"
        "<style>body{font-family:sans-serif;margin:0;padding:20px;background:#f5f5f5}"
        ".c{max-width:400px;margin:0 auto;background:#fff;padding:20px;"
        "border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,.1);text-align:center}"
        "</style></head><body><div class='c'>"
        "<h1>Saved!</h1>"
        "<p>Connecting to the network. You can close this page.</p>"
        "</div></body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    /* Post event so the orchestrator can restart in STA mode */
    esp_event_post(WIFI_PROV_EVENT, WIFI_PROV_EVENT_CREDENTIALS_SET,
                   &creds, sizeof(creds), pdMS_TO_TICKS(100));

    return ESP_OK;
}

/* Redirect any unknown path to "/" for captive portal detection */
static esp_err_t redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

/* ── Start / Stop ───────────────────────────────────────────────────── */

esp_err_t http_server_start(uint16_t port)
{
    if (s_server) {
        return ESP_ERR_INVALID_STATE;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port     = port;
    config.uri_match_fn    = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server (%s)", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t uri_root = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = root_handler,
    };
    const httpd_uri_t uri_scan = {
        .uri     = "/scan",
        .method  = HTTP_GET,
        .handler = scan_handler,
    };
    const httpd_uri_t uri_save = {
        .uri     = "/save",
        .method  = HTTP_POST,
        .handler = save_handler,
    };
    const httpd_uri_t uri_catch_all = {
        .uri     = "/*",
        .method  = HTTP_GET,
        .handler = redirect_handler,
    };

    httpd_register_uri_handler(s_server, &uri_root);
    httpd_register_uri_handler(s_server, &uri_scan);
    httpd_register_uri_handler(s_server, &uri_save);
    httpd_register_uri_handler(s_server, &uri_catch_all);

    ESP_LOGI(TAG, "HTTP server started on port %d", port);
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    if (!s_server) {
        return ESP_OK;
    }
    esp_err_t err = httpd_stop(s_server);
    s_server = NULL;
    ESP_LOGI(TAG, "HTTP server stopped");
    return err;
}
