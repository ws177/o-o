// face_engine.h — O-O 表情引擎（形状语言版，纯 C，不依赖 ESP-IDF）
//
// 模型（与 preview/o-o.html 完全一致）：
//   · 眼睛 = 实心圆 ∩ 可旋转的「眼皮带」 参数：t1/t2（带子两条边，单位=眼半径）、ang（旋转）
//   · 嘴   = 四种姿态（∧ / ω / ∪ / 团）的多边形，按权重混合
//   · 行为层：空闲时自己排动作；被戳时有编排好的反应序列
//   · 所有参数走弹簧（会过冲、会回弹）
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OO_POWER_ACTIVE = 0,
    OO_POWER_AOD,
    OO_POWER_SLEEP,
} oo_power_t;

typedef enum {
    OO_CMD_EYE = 0,
    OO_CMD_HEAD,
    OO_CMD_CHEEK,
    OO_CMD_CHIN,
} oo_cmd_t;

/* 渲染一帧所需的全部量 */
typedef struct {
    float t1L, t2L, angL;   /* 左眼眼皮带 */
    float t1R, t2R, angR;   /* 右眼 */
    float sc;               /* 眼球缩放（惊讶时略大） */

    float mChev, mOmega, mSmile, mBlob, mFrown;

    float lookX, lookY;     /* 视线偏移（单位：眼半径） */
    float tilt;             /* 整脸倾斜（弧度） */
    float shakeX;           /* 横向抖动（单位：眼半径） */

    float dim;              /* 0 正常 .. 1 全黑 */
    oo_power_t power;

    /* 调试用 */
    float happy, anger, surprise;
} oo_frame_t;

/* 弹簧 */
typedef struct {
    float x, v;
} oo_spring_t;

typedef struct {
    float t, last_input;
    oo_power_t power;

    float anger, happy, surprise;
    int happy_kind;          /* 0 无, 1 摸头, 2 戳脸 */

    float blink_t, blink_next, blink_lid;

    oo_spring_t s_t1L, s_t2L, s_angL, s_scL;
    oo_spring_t s_t1R, s_t2R, s_angR, s_scR;
    oo_spring_t s_omega, s_smile, s_blob, s_frown;
    oo_spring_t s_lookX, s_lookY, s_tilt;

    float react_t;
    int react_kind;          /* 0 无, 1 眼, 2 头, 3 脸, 4 下巴 */
    float react_gx, react_gy;

    float idle_next, idle_t, idle_dur;
    int idle_kind;           /* 见 idle_acts */
    float gaze_x, gaze_y;

    float shake_x;
    float dim;

    /* 外部输入（传感器） */
    float ext_gx, ext_gy;      /* 倾斜，-1..1 */
    float ext_tilt_age;        /* 距上次更新多少秒 */
    float light_dark;          /* 0 亮 .. 1 黑 */
    float emotion_hold;        /* 松手后保持表情的剩余秒数 */

    uint32_t rng;
} oo_face_t;

void oo_init(oo_face_t *f);
void oo_update(oo_face_t *f, float dt);
void oo_get_frame(const oo_face_t *f, oo_frame_t *out);

/* 交互 */
void oo_poke(oo_face_t *f, oo_cmd_t what);
void oo_shake(oo_face_t *f);
void oo_wake(oo_face_t *f);

/* 传感器输入 */
void oo_input_tilt(oo_face_t *f, float gx, float gy);
void oo_input_light(oo_face_t *f, float dark);

/* 传感器事件 */
enum { OO_EV_PICKUP = 1, OO_EV_SHAKE, OO_EV_COVER };
void oo_event(oo_face_t *f, int ev);

#ifdef __cplusplus
}
#endif
