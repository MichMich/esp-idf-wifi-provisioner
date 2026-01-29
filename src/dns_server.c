/*
 * SPDX-FileCopyrightText: 2026 Michael Teeuw
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Lightweight DNS server that redirects all queries to the AP IP,
 * triggering captive portal detection on client devices.
 */

#include "wifi_prov_internal.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DNS_PORT       53
#define DNS_BUF_SIZE   512

static const char *TAG = "wifi_prov_dns";

static TaskHandle_t s_task = NULL;
static int          s_sock = -1;

/*
 * Minimal DNS response: copy the query and flip header bits to make it
 * a valid reply that points every name at the AP gateway (192.168.4.1).
 */
static void dns_task(void *arg)
{
    uint8_t buf[DNS_BUF_SIZE];
    struct sockaddr_in client;
    socklen_t client_len;

    /* AP gateway address – default for esp_netif soft-AP */
    const uint32_t ap_ip = htonl(0xC0A80401); /* 192.168.4.1 */

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(s_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind DNS socket");
        close(s_sock);
        s_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS server listening on port %d", DNS_PORT);

    while (1) {
        client_len = sizeof(client);
        int len = recvfrom(s_sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&client, &client_len);
        if (len < 0) {
            break; /* socket closed by dns_server_stop() */
        }
        if (len < 12) {
            continue; /* too short for a DNS header */
        }

        /* Build response in-place */
        buf[2] = 0x81; /* QR=1, Opcode=0, AA=1 */
        buf[3] = 0x80; /* RA=1, RCODE=0 (No error) */
        /* Answer count = 1 */
        buf[6] = 0x00;
        buf[7] = 0x01;

        /* Append answer section right after the query */
        uint8_t *p = buf + len;

        /* Name pointer to the question name (offset 12) */
        *p++ = 0xC0;
        *p++ = 0x0C;
        /* Type A */
        *p++ = 0x00;
        *p++ = 0x01;
        /* Class IN */
        *p++ = 0x00;
        *p++ = 0x01;
        /* TTL = 60 seconds */
        *p++ = 0x00;
        *p++ = 0x00;
        *p++ = 0x00;
        *p++ = 0x3C;
        /* Data length = 4 */
        *p++ = 0x00;
        *p++ = 0x04;
        /* IP address */
        memcpy(p, &ap_ip, 4);
        p += 4;

        sendto(s_sock, buf, (size_t)(p - buf), 0,
               (struct sockaddr *)&client, client_len);
    }

    ESP_LOGI(TAG, "DNS server stopped");
    vTaskDelete(NULL);
}

esp_err_t dns_server_start(void)
{
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xTaskCreate(dns_task, "dns_server", 4096, NULL, 5, &s_task);
    return ESP_OK;
}

esp_err_t dns_server_stop(void)
{
    if (s_sock >= 0) {
        shutdown(s_sock, SHUT_RDWR);
        close(s_sock);
        s_sock = -1;
    }
    s_task = NULL;
    return ESP_OK;
}
