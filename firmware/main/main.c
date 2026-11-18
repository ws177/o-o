// main.c — O-O 固件入口（第一阶段：无屏，验证表情引擎）
//
// 串口命令（115200，UART0）：
//   h 摸头     e 戳眼     c 戳脸     k 戳下巴    s 摇晃
//   w 唤醒     p 打印当前帧           ? 帮助
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "face_engine.h"
#include "wifi_app.h"
#include "sensors.h"

static const char *TAG = "O-O";

static oo_face_t s_face;

static const char *power_name(oo_power_t p)
{
    switch (p) {
    case OO_POWER_ACTIVE: return "ACTIVE";
    case OO_POWER_AOD:    return "AOD";
    case OO_POWER_SLEEP:  return "SLEEP";
    default:              return "?";
    }
}

/* ── 表情引擎：60Hz 推进 ─────────────────────────────── */
static void engine_task(void *arg)
{
    (void)arg;
    int64_t last = esp_timer_get_time();
    oo_power_t prev_power = OO_POWER_ACTIVE;

    for (;;) {
        const int64_t now = esp_timer_get_time();
        float dt = (float)(now - last) / 1000000.0f;
        last = now;

        oo_update(&s_face, dt);

        if (s_face.power != prev_power) {
            ESP_LOGI(TAG, "power -> %s", power_name(s_face.power));
            prev_power = s_face.power;
        }
        vTaskDelay(pdMS_TO_TICKS(16));   /* 60fps */
    }
}

/* ── 串口命令 ────────────────────────────────────────── */
static void print_frame(void)
{
    oo_frame_t f;
    oo_get_frame(&s_face, &f);
    ESP_LOGI(TAG, "eye L(%.2f,%.2f,%.2f) R(%.2f,%.2f,%.2f) sc=%.2f",
             f.t1L, f.t2L, f.angL, f.t1R, f.t2R, f.angR, f.sc);
    ESP_LOGI(TAG, "mouth chev=%.2f om=%.2f smile=%.2f blob=%.2f",
             f.mChev, f.mOmega, f.mSmile, f.mBlob);
    ESP_LOGI(TAG, "state angry=%.2f happy=%.2f surprise=%.2f power=%s dim=%.2f",
             s_face.anger, s_face.happy, s_face.surprise, power_name(f.power), f.dim);
}

static void handle_cmd(char c)
{
    switch (c) {
    case 'h': oo_poke(&s_face, OO_CMD_HEAD);  ESP_LOGI(TAG, "pet head"); break;
    case 'e': oo_poke(&s_face, OO_CMD_EYE);   ESP_LOGI(TAG, "poke eye"); break;
    case 'c': oo_poke(&s_face, OO_CMD_CHEEK); ESP_LOGI(TAG, "poke cheek"); break;
    case 'k': oo_poke(&s_face, OO_CMD_CHIN);  ESP_LOGI(TAG, "poke chin"); break;
    case 's': oo_shake(&s_face);             ESP_LOGI(TAG, "shake"); break;
    case 'w': oo_wake(&s_face);              ESP_LOGI(TAG, "wake"); break;
    case 'p': print_frame();                 break;
    case 'i': sensors_dump();                break;
    case '?':
        ESP_LOGI(TAG, "cmd: h=head e=eye c=cheek k=chin s=shake w=wake p=print ?=help");
        break;
    default:
        break;
    }
}

/* 串口和 TCP 共用的命令入口 */
void oo_cmd_char(char c)
{
    handle_cmd(c);
}

static void cmd_task(void *arg)
{
    (void)arg;
    uint8_t ch;
    for (;;) {
        int n = uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(200));
        if (n == 1) {
            handle_cmd((char)ch);
        } else if (n < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));   /* 没装驱动/读失败时别空转刷错误 */
        }
    }
}

/* ── 帧上报：给电脑端"虚拟屏幕"用 ─────────────────────
 * 每 20ms 一行（50fps），17 个数：
 *   F,t1L,t2L,angL, t1R,t2R,angR, sc, mChev,mOmega,mSmile,mBlob,
 *     lookX,lookY, tilt,shakeX, dim, power
 * 角度/权重 ×1000；tilt/shake ×10000
 */
static void report_task(void *arg)
{
    (void)arg;
    char buf[160];
    for (;;) {
        oo_frame_t f;
        oo_get_frame(&s_face, &f);
        int n = snprintf(buf, sizeof(buf),
                         "F,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                         (int)(f.t1L * 1000.0f), (int)(f.t2L * 1000.0f), (int)(f.angL * 1000.0f),
                         (int)(f.t1R * 1000.0f), (int)(f.t2R * 1000.0f), (int)(f.angR * 1000.0f),
                         (int)(f.sc * 1000.0f),
                         (int)(f.mChev * 1000.0f), (int)(f.mOmega * 1000.0f),
                         (int)(f.mSmile * 1000.0f), (int)(f.mBlob * 1000.0f),
                         (int)(f.mFrown * 1000.0f),
                         (int)(f.lookX * 1000.0f), (int)(f.lookY * 1000.0f),
                         (int)(f.tilt * 10000.0f), (int)(f.shakeX * 10000.0f),
                         (int)(f.dim * 1000.0f), (int)f.power);
        printf("%s", buf);
        wifi_app_broadcast(buf, n);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "O-O 启动");
    ESP_LOGI(TAG, "芯片=%s 核数=%d", CONFIG_IDF_TARGET, portNUM_PROCESSORS);

    oo_init(&s_face);

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "chip rev=%d.%d  cores=%d", chip.revision / 100, chip.revision % 100, chip.cores);

    /* 读 UART0 必须先装驱动，否则 uart_read_bytes 会一直报 driver error */
    esp_err_t uerr = uart_driver_install(UART_NUM_0, 512, 0, 0, NULL, 0);
    if (uerr != ESP_OK) {
        ESP_LOGW(TAG, "uart_driver_install 失败: %s", esp_err_to_name(uerr));
    }

    xTaskCreate(engine_task, "engine", 4096, NULL, 5, NULL);
    xTaskCreate(cmd_task, "cmd", 3072, NULL, 3, NULL);
    xTaskCreate(report_task, "report", 3072, NULL, 2, NULL);

    wifi_app_start();
    sensors_start(&s_face);

    ESP_LOGI(TAG, "就绪，输入 ? 查看命令");
}
