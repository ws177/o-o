// sensors.h — 板载传感器：AP3216C（光感/接近）+ QMA6100P（三轴加速度）
#pragma once

#include "face_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 I2C 并启动采样任务；scale 探测失败会自动降级 */
void sensors_start(oo_face_t *face);

/* 调试用：当前读数 */
typedef struct {
    int ok_light, ok_accel;
    int als;            /* 环境光原始值 */
    int ax, ay, az;     /* 加速度原始值 */
    float g_mag;        /* 合加速度（单位 g） */
    uint8_t raw[6];     /* 加速度寄存器原始 6 字节，调试用 */
} sensors_state_t;

void sensors_get(sensors_state_t *out);

/* 寄存器转储（调试用） */
void sensors_dump(void);

#ifdef __cplusplus
}
#endif
