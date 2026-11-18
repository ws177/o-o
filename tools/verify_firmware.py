# -*- coding: utf-8 -*-
"""verify_firmware.py — 烧完自检：读帧、统计范围、戳一下看变化。
用法: python verify_firmware.py [COM12]
"""
import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM12"
FIELDS = ["t1L", "t2L", "angL", "t1R", "t2R", "angR", "sc",
          "mChev", "mOmega", "mSmile", "mBlob",
          "lookX", "lookY", "tilt", "shakeX", "dim", "power"]
SCALE = {k: 1000.0 for k in FIELDS}
SCALE["tilt"] = 10000.0
SCALE["shakeX"] = 10000.0
SCALE["power"] = 1.0


def read_frames(sp, seconds):
    buf = b""
    out = []
    end = time.time() + seconds
    while time.time() < end:
        buf += sp.read(512)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.strip()
            if line.startswith(b"F,"):
                parts = line[2:].split(b",")
                if len(parts) >= 17:
                    try:
                        out.append([int(p) for p in parts[:17]])
                    except ValueError:
                        pass
    return out


def stats(frames):
    if not frames:
        return None
    st = {}
    for i, k in enumerate(FIELDS):
        vals = [f[i] / SCALE[k] for f in frames]
        st[k] = (min(vals), max(vals))
    return st


def main():
    sp = serial.Serial(PORT, 115200, timeout=0.1)
    print("== 采集 14 秒静止 ==")
    a = read_frames(sp, 14)
    print("帧数 %d (%.1f fps)" % (len(a), len(a) / 14.0))
    s = stats(a)
    if not s:
        print("没收到帧！"); return
    print("\n%-8s %9s %9s %9s" % ("字段", "最小", "最大", "波动"))
    for k in FIELDS:
        lo, hi = s[k]
        print("%-8s %9.3f %9.3f %9.3f" % (k, lo, hi, hi - lo))

    # 判断"自己会动"
    moved = [k for k in ("t1L", "t2L", "mOmega", "mBlob", "tilt") if s[k][1] - s[k][0] > 0.02]
    print("\n静止时在变的量:", ", ".join(moved) if moved else "(完全没动 —— 有问题)")

    print("\n== 戳眼睛 ==")
    sp.write(b"e")
    b = read_frames(sp, 2.0)
    if b:
        print("  t1L %.2f -> %.2f,  t2L %.2f -> %.2f" %
              (a[-1][0] / 1000, b[0][0] / 1000, a[-1][1] / 1000, b[0][1] / 1000))
        print("  mChev %.2f -> %.2f, mBlob %.2f -> %.2f" %
              (a[-1][7] / 1000, b[0][7] / 1000, a[-1][10] / 1000, b[0][10] / 1000))

    print("\n== 摸头 ==")
    sp.write(b"h")
    c = read_frames(sp, 2.0)
    if c:
        print("  t2L %.2f -> %.2f (戴上 ∩), mOmega %.2f -> %.2f, mBlob %.2f -> %.2f" %
              (b[-1][1] / 1000, c[0][1] / 1000, b[-1][8] / 1000, c[0][8] / 1000,
               b[-1][10] / 1000, c[0][10] / 1000))

    sp.close()
    ok = bool(moved) and len(a) > 100
    print("\n结果:", "OK" if ok else "有问题")


if __name__ == "__main__":
    main()
