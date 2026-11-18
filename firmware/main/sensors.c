// sensors.c — AP3216C + QMA6100P（I2C）
//   行为：
//     晃一下        -> oo_shake()
//     倾斜          -> oo_input_tilt()   视线跟着倾斜方向偏
//     光变暗        -> oo_input_light()  自动压暗
#include "sensors.h"

#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"

static const char *TAG = "sensors";

#define PIN_SDA  41
#define PIN_SCL  42

#define ADDR_AP3216C  0x1E
#define ADDR_QMA6100P 0x12

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_ap = NULL;
static i2c_master_dev_handle_t s_qma = NULL;
static sensors_state_t s_state;

static oo_face_t *s_face = NULL;

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── I2C 小工具 ───────────────────────────────────────── */
static esp_err_t reg_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t n)
{
    return i2c_master_transmit_receive(dev, &reg, 1, buf, n, 100);
}

static esp_err_t reg_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(dev, b, 2, 100);
}

static esp_err_t add_dev(uint8_t addr, i2c_master_dev_handle_t *out)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(s_bus, &cfg, out);
}

/* ── 初始化 ───────────────────────────────────────────── */
static bool init_bus(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t e = i2c_new_master_bus(&cfg, &s_bus);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "I2C 总线初始化失败: %s", esp_err_to_name(e));
        return false;
    }
    return true;
}

static void init_ap3216c(void)
{
    if (i2c_master_probe(s_bus, ADDR_AP3216C, 100) != ESP_OK) {
        ESP_LOGW(TAG, "没找到 AP3216C (0x%02X)", ADDR_AP3216C);
        return;
    }
    if (add_dev(ADDR_AP3216C, &s_ap) != ESP_OK) {
        return;
    }
    /* 正点原子序列：软复位 -> 等 50ms -> 开 ALS+PS+IR */
    reg_write(s_ap, 0x00, 0x04);
    vTaskDelay(pdMS_TO_TICKS(50));
    reg_write(s_ap, 0x00, 0x03);
    vTaskDelay(pdMS_TO_TICKS(20));
    uint8_t mode = 0;
    reg_read(s_ap, 0x00, &mode, 1);
    if (mode == 0x03) {
        s_state.ok_light = 1;
        ESP_LOGI(TAG, "AP3216C 就绪 (mode=0x%02X)", mode);
    } else {
        ESP_LOGW(TAG, "AP3216C 回读 mode=0x%02X，可能没生效", mode);
    }
}

static void init_qma6100p(void)
{
    if (i2c_master_probe(s_bus, ADDR_QMA6100P, 100) != ESP_OK) {
        ESP_LOGW(TAG, "没找到 QMA6100P (0x%02X)", ADDR_QMA6100P);
        return;
    }
    if (add_dev(ADDR_QMA6100P, &s_qma) != ESP_OK) {
        return;
    }
    /* 厂商完整序列（qma6100p.c 的 qma6100p_comfig）*/
    reg_write(s_qma, 0x36, 0xB6);          /* 软复位 */
    vTaskDelay(pdMS_TO_TICKS(5));
    reg_write(s_qma, 0x36, 0x00);          /* 清复位 */
    vTaskDelay(pdMS_TO_TICKS(10));

    reg_write(s_qma, 0x11, 0x80);          /* active */
    reg_write(s_qma, 0x11, 0x84);          /* active + digital */
    reg_write(s_qma, 0x4a, 0x20);
    reg_write(s_qma, 0x56, 0x01);
    reg_write(s_qma, 0x5f, 0x80);
    vTaskDelay(pdMS_TO_TICKS(1));
    reg_write(s_qma, 0x5f, 0x00);
    vTaskDelay(pdMS_TO_TICKS(10));

    reg_write(s_qma, 0x0F, 0x04);          /* 量程 ±8G（枚举: 2G=01,4G=02,8G=04,16G=08）*/
    reg_write(s_qma, 0x10, 0x00);          /* 带宽 100Hz */
    reg_write(s_qma, 0x11, 0x84);          /* MCLK 51.2K | active */
    reg_write(s_qma, 0x21, 0x03);          /* step latch mode */
    vTaskDelay(pdMS_TO_TICKS(30));

    uint8_t id = 0, pm = 0, rg = 0;
    reg_read(s_qma, 0x00, &id, 1);
    reg_read(s_qma, 0x11, &pm, 1);
    reg_read(s_qma, 0x0F, &rg, 1);
    ESP_LOGI(TAG, "QMA6100P ID=0x%02X 0x11=0x%02X range=0x%02X", id, pm, rg);
    s_state.ok_accel = 1;
}

/* ── 采样任务 ─────────────────────────────────────────── */
static void sensors_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(50);   /* 20Hz */
    int shake_cool = 0;
    float still_sec = 0.0f;      /* 已经静止了多少秒 */
    float light_smooth = -1.0f;

    /* 自动标定：开机后前 2 秒的合模长当作 1g，免去猜量程寄存器 */
    float cal_sum = 0.0f;
    int cal_n = 0;
    float acc_scale = 0.0f;

    for (;;) {
        /* 光感：ALS 数据在 0x0C(低) / 0x0D(高) */
        if (s_ap) {
            uint8_t b[2] = { 0, 0 };
            if (reg_read(s_ap, 0x0C, b, 2) == ESP_OK) {
                int als = ((int)b[1] << 8) | b[0];
                if (als & 0x8000) {          /* 溢出位 */
                    als = 0xFFFF - als;
                }
                s_state.als = als;
                float dark = 1.0f - clampf((float)als / 900.0f, 0.0f, 1.0f);
                light_smooth = (light_smooth < 0) ? dark : light_smooth + (dark - light_smooth) * 0.2f;
                if (s_face) {
                    oo_input_light(s_face, light_smooth);
                }
            }
        }

        /* 加速度：数据在 0x01~0x06，14 位补码（低字节高 6 位 + 高字节） */
        if (s_qma) {
            uint8_t b[6] = { 0 };
            if (reg_read(s_qma, 0x01, b, 6) == ESP_OK) {
                s_state.raw[0] = b[0]; s_state.raw[1] = b[1]; s_state.raw[2] = b[2];
                s_state.raw[3] = b[3]; s_state.raw[4] = b[4]; s_state.raw[5] = b[5];
                int ax = ((b[1] << 6) | (b[0] >> 2)) & 0x3FFF;
                int ay = ((b[3] << 6) | (b[2] >> 2)) & 0x3FFF;
                int az = ((b[5] << 6) | (b[4] >> 2)) & 0x3FFF;
                if (ax & 0x2000) ax -= 0x4000;
                if (ay & 0x2000) ay -= 0x4000;
                if (az & 0x2000) az -= 0x4000;
                s_state.ax = ax; s_state.ay = ay; s_state.az = az;

                float raw_mag = sqrtf((float)(ax * ax + ay * ay + az * az));

                if (acc_scale <= 0.0f) {
                    /* 标定阶段：不产生任何行为，免得开机就乱动 */
                    cal_sum += raw_mag;
                    if (++cal_n >= 40) {
                        acc_scale = cal_sum / cal_n;
                        if (acc_scale < 100.0f) {
                            acc_scale = 1024.0f;   /* 没读到数据时的兜底 */
                        }
                        ESP_LOGI(TAG, "加速度标定完成: 1g = %.0f counts", acc_scale);
                    }
                    s_state.g_mag = 1.0f;
                } else {
                    float gx = ax / acc_scale;
                    float gy = ay / acc_scale;
                    float gz = az / acc_scale;
                    float mag = sqrtf(gx * gx + gy * gy + gz * gz);
                    s_state.g_mag = mag;

                    if (s_face) {
                        if (fabsf(mag - 1.0f) > 0.60f && shake_cool <= 0) {
                            oo_event(s_face, OO_EV_SHAKE);
                            shake_cool = 30;
                        }
                        /* 静止很久后突然动 → 被拿起来了 */
                        if (still_sec > 20.0f) {
                            if (fabsf(mag - 1.0f) > 0.18f) {
                                oo_event(s_face, OO_EV_PICKUP);
                            }
                            still_sec = 0.0f;
                        } else if (fabsf(mag - 1.0f) < 0.05f) {
                            still_sec += 0.05f;      /* 平稳就累计静止时长 */
                        } else {
                            still_sec = 0.0f;
                        }
                        oo_input_tilt(s_face,
                                      clampf(gx * 1.6f, -1.0f, 1.0f),
                                      clampf(gy * 1.6f, -1.0f, 1.0f));
                    }
                }
            }
        }
        if (shake_cool > 0) {
            shake_cool--;
        }

        /* 每 2 秒打一次读数，方便校阈值 */
        static int dbg = 0;
        if (++dbg >= 40) {
            dbg = 0;
            ESP_LOGI(TAG, "als=%d acc=(%d,%d,%d) |g|=%.2f tilt=(%.2f,%.2f) look=(%.3f,%.3f) dark=%.2f",
                     s_state.als, s_state.ax, s_state.ay, s_state.az,
                     s_state.g_mag,
                     s_face ? s_face->ext_gx : -9.0f,
                     s_face ? s_face->ext_gy : -9.0f,
                     s_face ? s_face->s_lookX.x : -9.0f,
                     s_face ? s_face->s_lookY.x : -9.0f,
                     s_face ? s_face->light_dark : -1.0f);
        }
        vTaskDelay(period);
    }
}

void sensors_start(oo_face_t *face)
{
    s_face = face;
    if (!init_bus()) {
        return;
    }
    init_ap3216c();
    init_qma6100p();
    if (s_state.ok_light || s_state.ok_accel) {
        xTaskCreate(sensors_task, "sensors", 4096, NULL, 3, NULL);
    }
}

void sensors_get(sensors_state_t *out)
{
    *out = s_state;
}

/* 寄存器转储：串口发 'i' 调，用来在没有逻辑分析仪时看芯片状态 */
void sensors_dump(void)
{
    char line[96];
    if (s_qma) {
        ESP_LOGI(TAG, "QMA6100P regs:");
        for (int base = 0x00; base < 0x30; base += 8) {
            int n = 0;
            n += snprintf(line + n, sizeof(line) - n, "  %02X:", base);
            for (int i = 0; i < 8 && base + i < 0x38; i++) {
                uint8_t v = 0;
                if (reg_read(s_qma, base + i, &v, 1) == ESP_OK) {
                    n += snprintf(line + n, sizeof(line) - n, " %02X", v);
                } else {
                    n += snprintf(line + n, sizeof(line) - n, " --");
                }
            }
            ESP_LOGI(TAG, "%s", line);
        }
    }
    if (s_ap) {
        ESP_LOGI(TAG, "AP3216C regs:");
        int n = snprintf(line, sizeof(line), "  ");
        for (int r = 0x00; r <= 0x0D; r++) {
            uint8_t v = 0;
            if (reg_read(s_ap, r, &v, 1) == ESP_OK) {
                n += snprintf(line + n, sizeof(line) - n, "%02X:%02X ", r, v);
            }
        }
        ESP_LOGI(TAG, "%s", line);
    }
    ESP_LOGI(TAG, "als=%d acc=(%d,%d,%d) |g|=%.2f",
             s_state.als, s_state.ax, s_state.ay, s_state.az, s_state.g_mag);
}
