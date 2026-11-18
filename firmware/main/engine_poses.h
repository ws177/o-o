// 自动生成，请勿手改。源: engine/engine.json
// 生成命令: python tools/gen_engine.py
#pragma once

/* 姿态：左眼/右眼 各自 t1,t2,ang,sc + 嘴权重 chev/omega/smile/blob/frown */
typedef struct { float eL[4], eR[4]; float m[5]; } oo_pose_t;
#define M_CHEV 0
#define M_OMEGA 1
#define M_SMILE 2
#define M_BLOB 3
#define M_FROWN 4
#define M_COUNT 5
#define P_IDLE 0
#define P_BLINK 1
#define P_HAPPY 2
#define P_PET 3
#define P_ANNOYED 4
#define P_SURPRISE 5
#define P_SLEEPY 6
#define P_JOY 7
#define P_ECSTATIC 8
#define P_ASLEEP 9
#define P_CONFUSED 10
#define P_NUMB 11
#define P_HURT 12
#define P_SHY 13
#define P_SMUG 14
#define P_WINK 15
#define P_YAWN 16
#define P_COUNT 17

static const oo_pose_t OO_POSES[P_COUNT] = {
    /* idle      */ { { -1.05f, 1.05f, 0.0f, 1.0f }, { -1.05f, 1.05f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    /* blink     */ { { 0.15f, 1.05f, 0.0f, 1.0f }, { 0.15f, 1.05f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    /* happy     */ { { -1.05f, 0.03f, 0.0f, 1.0f }, { -1.05f, 0.03f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f } },
    /* pet       */ { { -1.05f, 0.03f, 0.0f, 1.0f }, { -1.05f, 0.03f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f } },
    /* annoyed   */ { { -0.75f, -0.45f, 0.45f, 1.0f }, { -0.75f, -0.45f, 0.45f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    /* surprise  */ { { -1.1f, 1.1f, 0.0f, 1.1f }, { -1.1f, 1.1f, 0.0f, 1.1f }, { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f } },
    /* sleepy    */ { { 0.35f, 1.05f, 0.0f, 1.0f }, { 0.35f, 1.05f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    /* joy       */ { { -1.05f, -0.15f, 0.0f, 1.0f }, { -1.05f, -0.15f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f } },
    /* ecstatic  */ { { -1.05f, -0.38f, 0.0f, 1.02f }, { -1.05f, -0.38f, 0.0f, 1.02f }, { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f } },
    /* asleep    */ { { -0.05f, 0.05f, 0.0f, 1.0f }, { -0.05f, 0.05f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    /* confused  */ { { -1.05f, 0.1f, 0.0f, 1.0f }, { -1.05f, -0.15f, 0.22f, 0.94f }, { 0.45f, 0.55f, 0.0f, 0.0f, 0.0f } },
    /* numb      */ { { -0.05f, 0.05f, 0.0f, 1.0f }, { -0.05f, 0.05f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    /* hurt      */ { { -0.62f, -0.18f, 0.12f, 1.0f }, { -0.62f, -0.18f, 0.12f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f } },
    /* shy       */ { { -1.05f, 0.1f, 0.0f, 1.0f }, { -1.05f, 0.1f, 0.0f, 1.0f }, { 0.0f, 0.7f, 0.0f, 0.0f, 0.3f } },
    /* smug      */ { { -1.05f, -0.05f, 0.1f, 1.0f }, { -1.05f, -0.05f, 0.1f, 1.0f }, { 0.0f, 0.0f, 0.8f, 0.0f, 0.2f } },
    /* wink      */ { { -1.05f, 1.05f, 0.0f, 1.0f }, { -0.05f, 0.05f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f } },
    /* yawn      */ { { -0.05f, 0.05f, 0.0f, 1.0f }, { -0.05f, 0.05f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f } },
};

/* 权重增益 */
#define OO_G_ANNOYED 1.4f
#define OO_LO_ANNOYED 0.1f
#define OO_HI_ANNOYED 0.55f
#define OO_G_HAPPY_POKE   0.35f
#define OO_G_HAPPY_PET    1.2f
#define OO_LO_HAPPY 0.12f
#define OO_HI_HAPPY 0.55f
#define OO_G_JOY 1.2f
#define OO_LO_JOY 0.35f
#define OO_HI_JOY 0.68f
#define OO_G_ECSTATIC 1.3f
#define OO_LO_ECSTATIC 0.68f
#define OO_HI_ECSTATIC 0.96f
#define OO_G_HAPPY_POKE   0.35f
#define OO_G_PET_SELF 1.2f
#define OO_LO_PET 0.12f
#define OO_HI_PET 0.55f
#define OO_G_SURPRISE 1.4f
#define OO_LO_SURPRISE 0.1f
#define OO_HI_SURPRISE 0.55f
#define OO_G_BLINK 2.2f
#define OO_G_IDLE_ACTION  1.1f

/* 时序 */
#define OO_DECAY_ANGER    0.45f
#define OO_DECAY_HAPPY    0.55f
#define OO_DECAY_SURPRISE 2.4f
#define OO_BLINK_DOWN     0.06f
#define OO_BLINK_HOLD     0.1f
#define OO_BLINK_UP       0.22f
#define OO_BLINK_NEXT_LO  2.2f
#define OO_BLINK_NEXT_HI  5.5f
#define OO_BLINK_SUPPRESS 0.3f
#define OO_IDLE_NEXT_LO   1.2f
#define OO_IDLE_NEXT_HI   3.6f
#define OO_REACT_HOLD     0.9f
#define OO_LINGER_AFTER   3.5f
#define OO_AOD_AFTER      240.0f
#define OO_SLEEP_AFTER    600.0f
#define OO_TL_SLEEPY_AFTER 45.0f
#define OO_TL_ASLEEP_AFTER 150.0f
#define OO_DIM_AOD        0.55f
#define OO_DIM_RATE       6.0f

/* 反应编排 */
#define OO_R_EYE_ANNOYED_0 0.0f
#define OO_R_EYE_ANNOYED_1 0.14f
#define OO_R_EYE_ANNOYED_2 1.6f
#define OO_R_EYE_SHAKE_AMP 0.006f
#define OO_R_EYE_SHAKE_FREQ 70.0f
#define OO_R_EYE_SHAKE_DECAY 9.0f
#define OO_R_EYE_POP_0 0.08f
#define OO_R_EYE_POP_1 0.35f
#define OO_R_HEAD_PET_0 0.02f
#define OO_R_HEAD_PET_1 0.55f
#define OO_R_HEAD_PET_2 1.4f
#define OO_R_CHIN_SURPRISE_0 0.0f
#define OO_R_CHIN_SURPRISE_1 0.45f
#define OO_R_CHIN_SURPRISE_2 1.5f

/* 平滑 */
#define OO_EYE_FREQ       22.0f
#define OO_LOOK_FREQ      18.0f
#define OO_TILT_FREQ      9.0f
#define OO_OMEGA_FREQ     20.0f
#define OO_OMEGA_DAMP     0.75f
#define OO_BLOB_FREQ      22.0f
#define OO_BLOB_DAMP      0.6f
#define OO_SMILE_FREQ     18.0f
#define OO_SMILE_DAMP     0.7f

/* 空闲动作 */
#define I_BLINK 0
#define I_OMEGA 1
#define I_DOME 2
#define I_CRESCENT 3
#define I_LOOK 4
#define I_TILT 5
#define I_SQUASH 6
#define I_COUNT 7
static const float OO_IDLE_DUR[I_COUNT] = { 0.4f, 0.9f, 0.55f, 0.5f, 1.1f, 0.9f, 0.45f };

/* 布局（相对短边/屏宽的比例） */
#define OO_LAY_EYE_R 0.139f
#define OO_LAY_EYE_DX 0.303f
#define OO_LAY_EYE_Y 0.417f
#define OO_LAY_MOUTH_W 0.1575f
#define OO_LAY_MOUTH_Y 0.652f
