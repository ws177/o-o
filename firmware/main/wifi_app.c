// wifi_app.c — STA 连接 + 一个极简 TCP 服务器（默认 3333 端口）
//   连上之后：串口会打印 IP；PC 用 `nc <ip> 3333` 或 tools/wifi_view.py 就能收到帧
#include "wifi_app.h"
#include "wifi_creds.h"

#include <string.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

static const char *TAG = "wifi";

#define TCP_PORT 3333

static volatile bool s_connected = false;
static volatile int s_client = -1;
static SemaphoreHandle_t s_mtx;

/* ── WiFi 事件 ─────────────────────────────────────────── */
static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "断开 (reason=%d)，2 秒后重连", d ? d->reason : -1);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        s_connected = true;
        ESP_LOGI(TAG, "已连上 %s  IP=%s  端口 %d",
                 WIFI_SSID, ip4addr_ntoa((const ip4_addr_t *)&e->ip_info.ip), TCP_PORT);
    }
}

/* ── 极简 TCP 服务器：一次只服务一个客户端 ─────────────── */
static void tcp_task(void *arg)
{
    (void)arg;
    char rx[64];

    for (;;) {
        int srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (srv < 0) {
            ESP_LOGE(TAG, "socket 失败 errno=%d", errno);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        int yes = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(TCP_PORT);

        if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
            listen(srv, 1) < 0) {
            ESP_LOGE(TAG, "bind/listen 失败 errno=%d", errno);
            close(srv);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        ESP_LOGI(TAG, "TCP 监听 :%d", TCP_PORT);

        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int c = accept(srv, (struct sockaddr *)&peer, &plen);
        if (c < 0) {
            close(srv);
            continue;
        }
        ESP_LOGI(TAG, "客户端接入: %s", inet_ntoa(peer.sin_addr));
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        s_client = c;
        xSemaphoreGive(s_mtx);

        /* 收命令：一个字符一个命令，和串口一致 */
        for (;;) {
            int n = recv(c, rx, sizeof(rx), 0);
            if (n <= 0) {
                break;
            }
            for (int i = 0; i < n; i++) {
                /* 命令转发给主循环，通过串口命令同一个入口 */
                extern void oo_cmd_char(char c);
                oo_cmd_char(rx[i]);
            }
        }

        xSemaphoreTake(s_mtx, portMAX_DELAY);
        close(s_client);
        s_client = -1;
        xSemaphoreGive(s_mtx);
        close(srv);
        ESP_LOGI(TAG, "客户端断开");
    }
}

/* ── 对外 ─────────────────────────────────────────────── */
void wifi_app_broadcast(const char *line, int len)
{
    if (s_client < 0) {
        return;
    }
    xSemaphoreTake(s_mtx, pdMS_TO_TICKS(20));
    if (s_client >= 0) {
        send(s_client, line, len, MSG_DONTWAIT);
    }
    xSemaphoreGive(s_mtx);
}

bool wifi_app_connected(void)
{
    return s_connected;
}

void wifi_app_start(void)
{
    s_mtx = xSemaphoreCreateMutex();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ip_event, NULL, NULL));

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* ESP32-S3 只支持 2.4GHz，显式锁一下 */
    esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);

    ESP_LOGI(TAG, "正在连接 %s …", WIFI_SSID);

    xTaskCreate(tcp_task, "tcp", 4096, NULL, 4, NULL);
}
