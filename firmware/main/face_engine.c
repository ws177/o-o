// face_engine.c — O-O 表情引擎（形状语言版）
#include "face_engine.h"
#include "engine_poses.h"     /* 自动生成：姿态表/权重/时序/布局 */

#include <math.h>
#include <string.h>

/* ── 基础 ─────────────────────────────────────────────── */
static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float smoothstepf(float v, float a, float b)
{
    float t = clampf((v - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* 半正弦脉冲：u 落在 [a,b] 内才有值 */
static inline float pulse(float u, float a, float b)
{
    if (u < a || u > b) {
        return 0.0f;
    }
    return sinf((float)M_PI * (u - a) / (b - a));
}

static uint32_t xs_next(oo_face_t *f)
{
    uint32_t x = f->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    f->rng = x;
    return x;
}

static float randf(oo_face_t *f, float a, float b)
{
    return a + (b - a) * ((xs_next(f) >> 8) * (1.0f / 16777216.0f));
}

/* 弹簧：半隐式欧拉 + 子步进，和网页里的 spr() 完全一致 */
static float spring(oo_spring_t *s, float target, float dt, float freq, float damp)
{
    const float k = freq * freq;
    const float c = 2.0f * damp * freq;
    float rem = dt;
    while (rem > 1e-6f) {
        float h = rem < (1.0f / 240.0f) ? rem : (1.0f / 240.0f);
        s->v += (-k * (s->x - target) - c * s->v) * h;
        s->x += s->v * h;
        rem -= h;
    }
    return s->x;
}

/* 空闲动作的枚举与时长来自 engine_poses.h（I_* / OO_IDLE_DUR） */

/* 空闲动作 */


/* ── 初始化 ───────────────────────────────────────────── */
void oo_init(oo_face_t *f)
{
    memset(f, 0, sizeof(*f));
    f->rng = 0x9E3779B9u;
    f->power = OO_POWER_ACTIVE;
    f->blink_t = -1.0f;
    f->blink_next = 2.0f;
    f->idle_next = 2.2f;
    f->idle_t = -1.0f;
    f->react_t = -1.0f;

    f->s_t1L.x = f->s_t1R.x = OO_POSES[P_IDLE].eL[0];
    f->s_t2L.x = f->s_t2R.x = OO_POSES[P_IDLE].eL[1];
    f->s_angL.x = f->s_angR.x = 0.0f;
    f->s_scL.x = f->s_scR.x = 1.0f;
    f->s_tilt.x = 0.0f;
    f->s_lookX.x = f->s_lookY.x = 0.0f;
}

void oo_wake(oo_face_t *f)
{
    f->last_input = f->t;
    f->power = OO_POWER_ACTIVE;
}

void oo_shake(oo_face_t *f)
{
    oo_wake(f);
    f->react_t = 0.0f;
    f->react_kind = 1;               /* 复用"戳眼"的急促反应 */
    f->emotion_hold = OO_LINGER_AFTER;
    f->anger = clampf(f->anger + 0.30f, 0.0f, 1.0f);
}

void oo_input_tilt(oo_face_t *f, float gx, float gy)
{
    f->ext_gx = gx;
    f->ext_gy = gy;
    f->ext_tilt_age = 0.0f;
}

void oo_input_light(oo_face_t *f, float dark)
{
    f->light_dark = dark;
}

/* 传感器事件：被拿起 / 被摇晃 / 光被盖住 */
void oo_event(oo_face_t *f, int ev)
{
    if (ev == OO_EV_PICKUP) {
        const float idle_for = f->t - f->last_input;
        const int was_down = (f->power != OO_POWER_ACTIVE) ||
                             (idle_for > OO_TL_SLEEPY_AFTER * 0.6f);
        f->react_t = 0.0f;
        f->react_kind = 5;                       /* 5 = 拿起 */
        f->surprise = was_down ? 1.0f : 0.45f;   /* 睡着被抓起惊得更大 */
        f->last_input = f->t;
        f->emotion_hold = OO_LINGER_AFTER;
        f->power = OO_POWER_ACTIVE;
    } else if (ev == OO_EV_SHAKE) {
        f->react_t = 0.0f;
        f->react_kind = 6;
        f->anger = clampf(f->anger + 0.35f, 0.0f, 1.0f);
        f->surprise = 0.7f;
        f->last_input = f->t;
        f->emotion_hold = OO_LINGER_AFTER;
        f->power = OO_POWER_ACTIVE;
    } else if (ev == OO_EV_COVER) {
        float want = f->t - OO_TL_SLEEPY_AFTER * 0.8f;
        if (want < f->last_input) {
            f->last_input = want;
        }
    }
}

void oo_poke(oo_face_t *f, oo_cmd_t what)
{
    oo_wake(f);
    f->react_t = 0.0f;
    f->react_kind = what + 1;        /* 1 眼 2 头 3 脸 4 下巴 */
    f->emotion_hold = OO_LINGER_AFTER;   /* 松手后保持几秒 */
    switch (what) {
    case OO_CMD_EYE:
        f->anger = clampf(f->anger + 0.55f, 0.0f, 1.0f);
        f->happy = 0.0f;
        f->happy_kind = 0;
        break;
    case OO_CMD_HEAD:
        f->happy = clampf(f->happy + 0.55f, 0.0f, 1.0f);
        f->anger = clampf(f->anger - 0.50f, 0.0f, 1.0f);
        f->happy_kind = 1;
        break;
    case OO_CMD_CHEEK:
        f->happy = clampf(f->happy + 0.30f, 0.0f, 1.0f);
        f->happy_kind = 2;
        break;
    case OO_CMD_CHIN:
    default:
        f->surprise = 1.0f;
        break;
    }
}

/* ── 每帧 ─────────────────────────────────────────────── */
void oo_update(oo_face_t *f, float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    if (dt > 0.1f) {
        dt = 0.1f;
    }

    f->t += dt;
    const float idle_sec = f->t - f->last_input;
    f->power = idle_sec > OO_SLEEP_AFTER ? OO_POWER_SLEEP
             : idle_sec > OO_AOD_AFTER  ? OO_POWER_AOD
             : OO_POWER_ACTIVE;

    /* 情绪衰减 */
    /* 松手后先保持当前表情几秒，再往回走 */
    if (f->emotion_hold > 0.0f) {
        f->emotion_hold -= dt;
    } else {
        f->anger = clampf(f->anger - OO_DECAY_ANGER * dt, 0.0f, 1.0f);
        f->happy = clampf(f->happy - OO_DECAY_HAPPY * dt, 0.0f, 1.0f);
        f->surprise = clampf(f->surprise - OO_DECAY_SURPRISE * dt, 0.0f, 1.0f);
    }

    /* 眨眼：摸头/很开心的时候不眨，一直眯着眼享受 */
    if (f->react_kind == 2 || f->happy > OO_BLINK_SUPPRESS) {
        f->blink_t = -1.0f;
        f->blink_lid = 0.0f;
        if (f->blink_next < 0.6f) {
            f->blink_next = randf(f, 0.6f, 1.4f);
        }
    } else if (f->blink_t < 0.0f) {
        f->blink_next -= dt;
        if (f->blink_next <= 0.0f) {
            f->blink_t = 0.0f;
            f->blink_next = randf(f, OO_BLINK_NEXT_LO, OO_BLINK_NEXT_HI);
        }
    } else {
        f->blink_t += dt;
        float d = f->blink_t;
        if (d < OO_BLINK_DOWN) {
            f->blink_lid = d / OO_BLINK_DOWN;
        } else if (d < OO_BLINK_DOWN + OO_BLINK_HOLD) {
            f->blink_lid = 1.0f;
        } else if (d < OO_BLINK_DOWN + OO_BLINK_HOLD + OO_BLINK_UP) {
            f->blink_lid = 1.0f - (d - OO_BLINK_DOWN - OO_BLINK_HOLD) / OO_BLINK_UP;
        } else {
            f->blink_lid = 0.0f;
            f->blink_t = -1.0f;
        }
    }

    /* 空闲动作调度：一直在动 */
    if (f->power == OO_POWER_ACTIVE) {
        if (f->idle_t < 0.0f) {
            f->idle_next -= dt;
            if (f->idle_next <= 0.0f) {
                f->idle_kind = (int)(xs_next(f) % I_COUNT);
                f->idle_t = 0.0f;
                f->idle_dur = OO_IDLE_DUR[f->idle_kind];
                if (f->idle_kind == I_BLINK) {
                    f->blink_t = 0.0f;
                }
                if (f->idle_kind == I_LOOK) {
                    f->gaze_x = randf(f, -1.0f, 1.0f);
                    f->gaze_y = randf(f, -0.6f, 0.6f);
                }
                f->idle_next = randf(f, OO_IDLE_NEXT_LO, OO_IDLE_NEXT_HI);
            }
        } else {
            f->idle_t += dt;
            if (f->idle_t > f->idle_dur) {
                f->idle_t = -1.0f;
                f->idle_kind = -1;
            }
        }
    }

    if (f->react_t >= 0.0f) {
        f->react_t += dt;
        if (f->react_t > OO_REACT_HOLD) {
            f->react_t = -1.0f;
        }
    }

    /* ── 姿态权重：情绪越强，idle 越让位 ── */
    const float anger = f->anger, happy = f->happy, sur = f->surprise;
    float idle_w = 0.0f;
    if (f->idle_t >= 0.0f && f->idle_dur > 0.001f) {
        float u = clampf(f->idle_t / f->idle_dur, 0.0f, 1.0f);
        idle_w = sinf((float)M_PI * u);
    }

    float w[P_COUNT] = { 0 };
    w[P_ANNOYED]  += smoothstepf(anger, OO_LO_ANNOYED, OO_HI_ANNOYED) * OO_G_ANNOYED;
    w[P_HAPPY]    += smoothstepf(happy, OO_LO_HAPPY, OO_HI_HAPPY) * (f->happy_kind == 2 ? OO_G_HAPPY_PET : OO_G_HAPPY_POKE);
    w[P_JOY]      += smoothstepf(happy, OO_LO_JOY, OO_HI_JOY) * OO_G_JOY;
    w[P_ECSTATIC] += smoothstepf(happy, OO_LO_ECSTATIC, OO_HI_ECSTATIC) * OO_G_ECSTATIC;
    w[P_PET]      += smoothstepf(happy, OO_LO_HAPPY, OO_HI_HAPPY) * (f->happy_kind == 1 ? OO_G_PET_SELF : OO_G_HAPPY_POKE);
    w[P_SURPRISE] += smoothstepf(sur, 0.10f, 0.55f) * OO_G_ANNOYED;
    /* 眨眼不参与混合：由 oo_get_frame 直接叠加 */

    if (f->idle_kind == I_DOME)     w[P_HAPPY]   += idle_w * OO_G_IDLE_ACTION;
    if (f->idle_kind == I_CRESCENT) w[P_ANNOYED] += idle_w * OO_G_IDLE_ACTION;

    if (f->react_kind == 1 && f->react_t >= 0.0f) w[P_ANNOYED]  += pulse(f->react_t, OO_R_EYE_ANNOYED_0, OO_R_EYE_ANNOYED_1) * OO_R_EYE_ANNOYED_2;
    if (f->react_kind == 2 && f->react_t >= 0.0f) w[P_PET]      += pulse(f->react_t, 0.02f, 0.55f) * OO_G_ANNOYED;
    if (f->react_kind == 4 && f->react_t >= 0.0f) w[P_SURPRISE] += pulse(f->react_t, OO_R_CHIN_SURPRISE_0, OO_R_CHIN_SURPRISE_1) * OO_R_CHIN_SURPRISE_2;
    if (f->react_kind == 5 && f->react_t >= 0.0f) w[P_SURPRISE] += pulse(f->react_t, 0.0f, 0.55f) * 1.8f;
    if (f->react_kind == 6 && f->react_t >= 0.0f) w[P_ANNOYED]  += pulse(f->react_t, 0.0f, 0.60f) * 1.3f;

    float max_other = 0.0f;
    for (int i = 1; i < P_COUNT; i++) {
        if (w[i] > max_other) max_other = w[i];
    }
    w[P_IDLE] = 1.0f - (max_other > 1.0f ? 1.0f : max_other);

    float wsum = 0.0f;
    for (int i = 0; i < P_COUNT; i++) wsum += w[i];
    if (wsum <= 0.0f) { w[P_IDLE] = 1.0f; wsum = 1.0f; }

    oo_pose_t mix = { 0 };
    for (int i = 0; i < P_COUNT; i++) {
        if (w[i] <= 0.0f) continue;
        for (int e = 0; e < 4; e++) {
            mix.eL[e] += OO_POSES[i].eL[e] * w[i];
            mix.eR[e] += OO_POSES[i].eR[e] * w[i];
        }
        for (int m = 0; m < M_COUNT; m++) mix.m[m] += OO_POSES[i].m[m] * w[i];
    }
    for (int e = 0; e < 4; e++) {
        mix.eL[e] /= wsum;
        mix.eR[e] /= wsum;
    }
    for (int m = 0; m < M_COUNT; m++) mix.m[m] /= wsum;

    float t1 = mix.eL[0], t2 = mix.eL[1], ang = mix.eL[2], sc = mix.eL[3];
    float t1r = mix.eR[0], t2r = mix.eR[1], angr = mix.eR[2], scr = mix.eR[3];

    /* 戳眼时的急促抖动与弹一下 */
    if (f->react_kind == 1 && f->react_t >= 0.0f) {
        sc *= 1.0f + 0.06f * pulse(f->react_t, 0.08f, 0.35f);
    }
    if (f->idle_kind == I_SQUASH) {
        sc *= 1.0f + 0.05f * idle_w;
    }

    /* 视线 */
    float gx = 0.0f, gy = 0.0f;
    if (f->idle_kind == I_LOOK) {
        gx = f->gaze_x * 0.10f * idle_w;
        gy = f->gaze_y * 0.08f * idle_w;
    }
    /* 传感器倾斜优先：拿起来转一下，眼睛就跟着歪 */
    f->ext_tilt_age += dt;
    if (f->ext_tilt_age < 0.6f) {
        gx = clampf(f->ext_gx * 0.11f, -0.12f, 0.12f);
        gy = clampf(f->ext_gy * 0.09f, -0.10f, 0.10f);
    }
    /* 倾斜：一直在动的底噪，再叠空闲动作 */
    float tilt_t = sinf(f->t * 0.41f) * 0.004f + sinf(f->t * 0.13f) * 0.003f;
    if (f->idle_kind == I_TILT) {
        tilt_t += sinf(f->t * 6.0f) * 0.012f * idle_w;
    }
    /* 抖动 */
    f->shake_x = (f->react_kind == 1 && f->react_t >= 0.0f)
        ? sinf(f->react_t * 70.0f) * 0.006f * expf(-f->react_t * 9.0f)
        : 0.0f;

    /* 平滑：和网页一样用指数逼近（不是弹簧），保证两侧响应一致 */
    const float ke = 1.0f - expf(-OO_EYE_FREQ * dt);
    f->s_t1L.x  += (t1  - f->s_t1L.x)  * ke;
    f->s_t2L.x  += (t2  - f->s_t2L.x)  * ke;
    f->s_angL.x += (ang - f->s_angL.x) * ke;
    f->s_scL.x  += (sc  - f->s_scL.x)  * ke;
    f->s_t1R.x  += (t1r  - f->s_t1R.x)  * ke;
    f->s_t2R.x  += (t2r  - f->s_t2R.x)  * ke;
    f->s_angR.x += (angr - f->s_angR.x) * ke;
    f->s_scR.x  += (scr  - f->s_scR.x)  * ke;

    const float kl = 1.0f - expf(-OO_LOOK_FREQ * dt);
    f->s_lookX.x += (gx - f->s_lookX.x) * kl;
    f->s_lookY.x += (gy - f->s_lookY.x) * kl;

    const float kt = 1.0f - expf(-OO_TILT_FREQ * dt);
    f->s_tilt.x += (tilt_t - f->s_tilt.x) * kt;

    /* ── 嘴型：主要来自姿态混合，再叠空闲/反应的小动作 ── */
    float om = mix.m[M_OMEGA], bl = mix.m[M_BLOB], sm = mix.m[M_SMILE];
    float fr = mix.m[M_FROWN];
    if (f->idle_kind == I_OMEGA && idle_w > om) {
        om = idle_w;
    }
    if (f->react_kind == 3 && f->react_t >= 0.0f) {   /* 戳脸 → U 形笑 */
        float s = pulse(f->react_t, 0.0f, 0.60f) * 0.85f;
        if (s > sm) sm = s;
    }

    spring(&f->s_omega, clampf(om, 0, 1), dt, 20.0f, 0.75f);
    spring(&f->s_blob, clampf(bl, 0, 1), dt, 22.0f, 0.60f);
    spring(&f->s_smile, clampf(sm, 0, 1), dt, 18.0f, 0.70f);
    spring(&f->s_frown, clampf(fr, 0, 1), dt, 18.0f, 0.70f);

    /* 压暗 */
    float dim_t = f->power == OO_POWER_AOD ? OO_DIM_AOD : (f->power == OO_POWER_SLEEP ? 1.0f : 0.0f);
    dim_t += f->light_dark * 0.55f;      /* 盖住光感 → 自己变暗 */
    if (dim_t > 1.0f) {
        dim_t = 1.0f;
    }
    f->dim += (dim_t - f->dim) * (1.0f - expf(-OO_DIM_RATE * dt));
}

/* ── 输出 ─────────────────────────────────────────────── */
void oo_get_frame(const oo_face_t *f, oo_frame_t *o)
{
    /* 眨眼：上眼皮往下压，把当前形状压成一条细线（保留它自己的下沿）
       不能跳到固定的闭眼姿态，否则 ∩ 眼会翻成下弦，看起来像眼珠在打转 */
    const float lid = clampf(f->blink_lid, 0.0f, 1.0f);
    o->t1L = f->s_t1L.x + ((f->s_t2L.x - 0.12f) - f->s_t1L.x) * lid;
    o->t2L = f->s_t2L.x;
    o->angL = f->s_angL.x;
    o->t1R = f->s_t1R.x + ((f->s_t2R.x - 0.12f) - f->s_t1R.x) * lid;
    o->t2R = f->s_t2R.x;
    o->angR = f->s_angR.x;
    o->sc = 0.5f * (f->s_scL.x + f->s_scR.x);

    float om = clampf(f->s_omega.x, 0.0f, 1.0f);
    float bl = clampf(f->s_blob.x, 0.0f, 1.0f);
    float sm = clampf(f->s_smile.x, 0.0f, 1.0f);
    float fr = clampf(f->s_frown.x, 0.0f, 1.0f);
    float ch = clampf(1.0f - fmaxf(om, fmaxf(bl, fmaxf(sm, fr))), 0.0f, 1.0f);
    o->mChev = ch;
    o->mOmega = om;
    o->mSmile = sm;
    o->mBlob = bl;
    o->mFrown = fr;

    o->lookX = f->s_lookX.x;
    o->lookY = f->s_lookY.x;
    o->tilt = f->s_tilt.x;
    o->shakeX = f->shake_x;
    o->dim = clampf(f->dim, 0.0f, 1.0f);
    o->power = f->power;

    o->happy = f->happy;
    o->anger = f->anger;
    o->surprise = f->surprise;
}
