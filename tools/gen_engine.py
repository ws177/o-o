# -*- coding: utf-8 -*-
"""
gen_engine.py — 把 engine/engine.json 生成为两侧代码

    python tools/gen_engine.py            # 生成
    python tools/gen_engine.py --check    # 只检查是否一致（CI 用）

产物：
    firmware/main/engine_poses.h      C 侧的常数表
    preview/o-o.html                  替换 // <<<GEN_BEGIN>>> … // <<<GEN_END>>> 之间
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "engine", "engine.json")
HDR = os.path.join(ROOT, "firmware", "main", "engine_poses.h")
HTML = os.path.join(ROOT, "preview", "o-o.html")

BEGIN = "// <<<GEN_BEGIN>>>"
END = "// <<<GEN_END>>>"

MOUTH = ["chev", "omega", "smile", "blob", "frown"]


def f(v):
    """C 浮点字面量"""
    s = ("%.6f" % float(v)).rstrip("0")
    if s.endswith("."):
        s += "0"
    return s + "f"


def gen_header(cfg):
    L = []
    L.append("// 自动生成，请勿手改。源: engine/engine.json")
    L.append("// 生成命令: python tools/gen_engine.py")
    L.append("#pragma once")
    L.append("")

    poses = cfg["poses"]
    order = ["idle", "blink", "happy", "pet", "annoyed", "surprise", "sleepy"]
    order = [n for n in order if n in poses] + [n for n in poses if n not in order]

    L.append("/* 姿态：左眼/右眼 各自 t1,t2,ang,sc + 嘴权重 " + "/".join(MOUTH) + " */")
    L.append("typedef struct { float eL[4], eR[4]; float m[%d]; } oo_pose_t;" % len(MOUTH))
    for i, k in enumerate(MOUTH):
        L.append("#define M_%s %d" % (k.upper(), i))
    L.append("#define M_COUNT %d" % len(MOUTH))
    for i, n in enumerate(order):
        L.append("#define P_%s %d" % (n.upper(), i))
    L.append("#define P_COUNT %d" % len(order))
    L.append("")
    L.append("static const oo_pose_t OO_POSES[P_COUNT] = {")
    for n in order:
        e = poses[n]["eye"]
        er = poses[n].get("eyeR", e)
        m = poses[n]["mouth"]
        mv = [float(m.get(k, 0)) for k in MOUTH]
        L.append("    /* %-9s */ { { %s }, { %s }, { %s } }," % (
            n,
            ", ".join(f(x) for x in e),
            ", ".join(f(x) for x in er),
            ", ".join(f(x) for x in mv)))
    L.append("};")
    L.append("")

    b = cfg["blend"]
    L.append("/* 权重增益 */")
    for name, val in b.items():
        if name.startswith("_"):
            continue
        up = name.upper()
        if name == "idle_action":
            L.append("#define OO_G_IDLE_ACTION  %s" % f(val["gain"]))
            continue
        if "gain" in val:
            L.append("#define OO_G_%s %s" % (up, f(val["gain"])))
        if "gain_other" in val:
            L.append("#define OO_G_HAPPY_POKE   %s" % f(val["gain_other"]))
        if "gain_cheek" in val:
            L.append("#define OO_G_HAPPY_PET    %s" % f(val["gain_cheek"]))
        if "gain_pet" in val:
            L.append("#define OO_G_%s_SELF %s" % (up, f(val["gain_pet"])))
        if "lo" in val:
            L.append("#define OO_LO_%s %s" % (up, f(val["lo"])))
            L.append("#define OO_HI_%s %s" % (up, f(val["hi"])))
    L.append("")

    t = cfg["timing"]
    tl = cfg.get("timeline", {})
    L.append("/* 时序 */")
    L.append("#define OO_DECAY_ANGER    %s" % f(t["decay"]["anger"]))
    L.append("#define OO_DECAY_HAPPY    %s" % f(t["decay"]["happy"]))
    L.append("#define OO_DECAY_SURPRISE %s" % f(t["decay"]["surprise"]))
    L.append("#define OO_BLINK_DOWN     %s" % f(t["blink"]["down"]))
    L.append("#define OO_BLINK_HOLD     %s" % f(t["blink"]["hold"]))
    L.append("#define OO_BLINK_UP       %s" % f(t["blink"]["up"]))
    L.append("#define OO_BLINK_NEXT_LO  %s" % f(t["blink"]["next_lo"]))
    L.append("#define OO_BLINK_NEXT_HI  %s" % f(t["blink"]["next_hi"]))
    L.append("#define OO_BLINK_SUPPRESS %s" % f(t["blink"].get("suppress_happy", 0.30)))
    L.append("#define OO_IDLE_NEXT_LO   %s" % f(t["idle"]["next_lo"]))
    L.append("#define OO_IDLE_NEXT_HI   %s" % f(t["idle"]["next_hi"]))
    L.append("#define OO_REACT_HOLD     %s" % f(t["react"]["hold"]))
    L.append("#define OO_LINGER_AFTER   %s" % f(t.get("linger", {}).get("after_touch", 3.5)))
    L.append("#define OO_AOD_AFTER      %s" % f(tl.get("aod_after", 240)))
    L.append("#define OO_SLEEP_AFTER    %s" % f(tl.get("sleep_after", 600)))
    L.append("#define OO_TL_SLEEPY_AFTER %s" % f(tl.get("sleepy_after", 45)))
    L.append("#define OO_TL_ASLEEP_AFTER %s" % f(tl.get("asleep_after", 150)))
    L.append("#define OO_DIM_AOD        %s" % f(t["dim"]["aod"]))
    L.append("#define OO_DIM_RATE       %s" % f(t["dim"]["rate"]))
    L.append("")

    r = cfg["reaction"]
    L.append("/* 反应编排 */")
    for nm, key in (("EYE", "eye"), ("HEAD", "head"), ("CHIN", "chin")):
        for k, v in r[key].items():
            if isinstance(v, list):
                for i, x in enumerate(v):
                    L.append("#define OO_R_%s_%s_%d %s" % (nm, k.upper(), i, f(x)))
            else:
                L.append("#define OO_R_%s_%s %s" % (nm, k.upper(), f(v)))
    L.append("")

    s = cfg["smooth"]
    L.append("/* 平滑 */")
    L.append("#define OO_EYE_FREQ       %s" % f(s["eye_freq"]))
    L.append("#define OO_LOOK_FREQ      %s" % f(s["look_freq"]))
    L.append("#define OO_TILT_FREQ      %s" % f(s["tilt_freq"]))
    L.append("#define OO_OMEGA_FREQ     %s" % f(s["mouth"]["omega"][0]))
    L.append("#define OO_OMEGA_DAMP     %s" % f(s["mouth"]["omega"][1]))
    L.append("#define OO_BLOB_FREQ      %s" % f(s["mouth"]["blob"][0]))
    L.append("#define OO_BLOB_DAMP      %s" % f(s["mouth"]["blob"][1]))
    L.append("#define OO_SMILE_FREQ     %s" % f(s["mouth"]["smile"][0]))
    L.append("#define OO_SMILE_DAMP     %s" % f(s["mouth"]["smile"][1]))
    L.append("")

    ia = cfg["idle_actions"]
    kinds = ia["kinds"]
    L.append("/* 空闲动作 */")
    for i, k in enumerate(kinds):
        L.append("#define I_%s %d" % (k.upper(), i))
    L.append("#define I_COUNT %d" % len(kinds))
    L.append("static const float OO_IDLE_DUR[I_COUNT] = { %s };" % (
        ", ".join(f(ia["durations"][k]) for k in kinds)))
    L.append("")

    lay = cfg["layout"]
    L.append("/* 布局（相对短边/屏宽的比例） */")
    for k, v in lay.items():
        L.append("#define OO_LAY_%s %s" % (k.upper(), f(v)))
    L.append("")
    return "\n".join(L)


def gen_js(cfg):
    poses = cfg["poses"]
    order = ["idle", "blink", "happy", "pet", "annoyed", "surprise", "sleepy"]
    order = [n for n in order if n in poses] + [n for n in poses if n not in order]

    lines = []
    lay = cfg["layout"]
    tl = cfg.get("timeline", {})
    lines.append("  const CFG = {")
    lines.append("    eyeR: %s, eyeDX: %s, eyeY: %s, mouthW: %s, mouthY: %s," % (
        lay["eye_r"], lay["eye_dx"], lay["eye_y"], lay["mouth_w"], lay["mouth_y"]))
    lines.append("    idleMin: %s, idleMax: %s," % (cfg["timing"]["idle"]["next_lo"], cfg["timing"]["idle"]["next_hi"]))
    lines.append("  };")
    lines.append("")
    lines.append("  const POSES = {")
    for n in order:
        e = poses[n]["eye"]
        er = poses[n].get("eyeR", e)
        m = poses[n]["mouth"]
        mouth = ", ".join("%s: %s" % (k, v) for k, v in m.items())
        extra = ""
        if "eyeR" in poses[n]:
            extra = ", eyeR: { t1: %s, t2: %s, ang: %s, sc: %s }" % tuple(er)
        lines.append("    %-9s: { eye: { t1: %s, t2: %s, ang: %s, sc: %s }%s, mouth: { %s } }," % (
            n, e[0], e[1], e[2], e[3], extra, mouth))
    lines.append("  };")
    lines.append("")
    t = cfg["timing"]
    s = cfg["smooth"]
    b = cfg["blend"]
    r = cfg["reaction"]
    lines.append("  const TIMING = {")
    lines.append("    decay: { anger: %s, happy: %s, surprise: %s }," % (
        t["decay"]["anger"], t["decay"]["happy"], t["decay"]["surprise"]))
    lines.append("    blink: { down: %s, hold: %s, up: %s, nextLo: %s, nextHi: %s }," % (
        t["blink"]["down"], t["blink"]["hold"], t["blink"]["up"], t["blink"]["next_lo"], t["blink"]["next_hi"]))
    lines.append("    react: { hold: %s }," % t["react"]["hold"])
    lines.append("    linger: { afterTouch: %s }," % t.get("linger", {}).get("after_touch", 3.5))
    lines.append("    power: { aodAfter: %s, sleepAfter: %s }," % (
        tl.get("aod_after", 240), tl.get("sleep_after", 600)))
    lines.append("    dim: { aod: %s, rate: %s }," % (t["dim"]["aod"], t["dim"]["rate"]))
    lines.append("    blend: {")
    for name, val in b.items():
        if name.startswith("_"):
            continue
        key = "idleAction" if name == "idle_action" else name
        g = val.get("gain", val.get("gain_pet", val.get("gain_other", 1.0)))
        lines.append("      %s: { gain: %s, lo: %s, hi: %s }," % (
            key, g, val.get("lo", 0.0), val.get("hi", 1.0)))
    lines.append("    },")
    lines.append("    reactBlend: {")
    for k in ("eye", "head", "chin"):
        arr = r[k].get("annoyed") or r[k].get("pet") or r[k].get("surprise")
        pose = "annoyed" if "annoyed" in r[k] else ("pet" if "pet" in r[k] else "surprise")
        lines.append("      %s: { pose: '%s', lo: %s, hi: %s, gain: %s }," % (k, pose, arr[0], arr[1], arr[2]))
    lines.append("    },")
    lines.append("    smooth: { eyeFreq: %s, lookFreq: %s, tiltFreq: %s," % (
        s["eye_freq"], s["look_freq"], s["tilt_freq"]))
    lines.append("      omega: [%s, %s], blob: [%s, %s], smile: [%s, %s] }," % (
        s["mouth"]["omega"][0], s["mouth"]["omega"][1],
        s["mouth"]["blob"][0], s["mouth"]["blob"][1],
        s["mouth"]["smile"][0], s["mouth"]["smile"][1]))
    lines.append("  };")
    lines.append("")
    ia = cfg["idle_actions"]
    lines.append("  const IDLE_KINDS = %s;" % json.dumps(ia["kinds"]))
    lines.append("  const IDLE_DUR = %s;" % json.dumps(ia["durations"]))
    mouth = cfg.get("mouth", {})
    lines.append("  const MOUTH_CHEV_PEAK = %s;" % mouth.get("chev_peak", 0.44))
    lines.append("  const BLINK_SUPPRESS_HAPPY = %s;" % t["blink"].get("suppress_happy", 0.30))
    st = cfg.get("states", {})
    if st:
        lines.append("  const STATES = %s;" % json.dumps(
            {k: v for k, v in st.items() if not k.startswith("_")}))
    tl = cfg.get("timeline", {})
    if tl:
        lines.append("  const TIMELINE = %s;" % json.dumps(
            {k: v for k, v in tl.items() if not k.startswith("_")}))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    cfg = json.load(open(SRC, encoding="utf-8"))

    hdr = gen_header(cfg)
    js = gen_js(cfg)

    html = open(HTML, encoding="utf-8").read()
    m = re.search(re.escape(BEGIN) + r"(.*?)" + re.escape(END), html, re.S)
    if not m:
        print("错误: o-o.html 里找不到 GEN 标记", BEGIN, "/", END)
        sys.exit(1)
    new_html = html[:m.start(1)] + "\n" + js + "\n  " + html[m.end(1):]

    if args.check:
        bad = []
        if not os.path.exists(HDR) or open(HDR, encoding="utf-8").read() != hdr:
            bad.append("engine_poses.h 与 engine.json 不一致")
        if new_html != html:
            bad.append("o-o.html 的 GEN 段与 engine.json 不一致")
        if bad:
            print("\n".join(bad))
            print("跑 python tools/gen_engine.py 重新生成")
            sys.exit(1)
        print("一致 OK")
        return

    os.makedirs(os.path.dirname(HDR), exist_ok=True)
    with open(HDR, "w", encoding="utf-8") as fp:
        fp.write(hdr)
    with open(HTML, "w", encoding="utf-8") as fp:
        fp.write(new_html)
    print("已生成:", os.path.relpath(HDR, ROOT))
    print("已更新:", os.path.relpath(HTML, ROOT))
    print("姿态数: %d  空闲动作: %d" % (len(cfg["poses"]), len(cfg["idle_actions"]["kinds"])))


if __name__ == "__main__":
    main()
