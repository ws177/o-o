# -*- coding: utf-8 -*-
"""e2e_test.py — 端到端测试：走桥接 HTTP，不动串口
   1. 连 /frames 收帧
   2. 通过 /cmd 发命令
   3. 断言板子的反应
"""
import json
import sys
import threading
import time
import urllib.request

BASE = "http://127.0.0.1:8777"
FIELDS = ["t1L", "t2L", "angL", "t1R", "t2R", "angR", "sc",
          "mChev", "mOmega", "mSmile", "mBlob",
          "lookX", "lookY", "tilt", "shakeX", "dim", "power"]
SCALE = {k: 1000.0 for k in FIELDS}
SCALE["tilt"] = 10000.0
SCALE["shakeX"] = 10000.0
SCALE["power"] = 1.0

frames = []
lock = threading.Lock()
stop = False


def reader():
    try:
        r = urllib.request.urlopen(BASE + "/frames", timeout=60)
    except Exception as e:
        print("连不上 /frames:", e)
        return
    while not stop:
        line = r.readline()
        if not line:
            break
        line = line.decode("utf-8", "ignore").strip()
        if line.startswith("data:"):
            body = line[5:].strip()
            if body.startswith("F,"):
                body = body[2:]
            if not body:
                continue
            try:
                p = [int(x) for x in body.split(",")[:17]]
                if len(p) == 17:
                    with lock:
                        frames.append(p)
            except ValueError:
                pass


def take(seconds):
    with lock:
        n0 = len(frames)
    time.sleep(seconds)
    with lock:
        return frames[n0:]


def cmd(c):
    try:
        urllib.request.urlopen(BASE + "/cmd?c=" + c, timeout=5).read()
    except Exception as e:
        print("命令失败:", e)


def rng(fr, idx, scale=1000.0):
    if not fr:
        return (0, 0)
    v = [f[idx] / scale for f in fr]
    return (min(v), max(v))


def main():
    t = threading.Thread(target=reader, daemon=True)
    t.start()
    time.sleep(1.0)
    with lock:
        n = len(frames)
    print("初始帧数: %d" % n)
    if n == 0:
        print("没收到帧，桥接没跑？")
        return 1

    ok = 0
    bad = 0

    # 1) 戳眼
    cmd("e")
    fr = take(1.2)
    t2 = rng(fr, 1)
    ang = rng(fr, 2)
    passed = t2[0] < -0.10 or ang[0] > 0.05
    print("[戳眼]   t2=%.2f..%.2f  ang=%.2f..%.2f  %s" % (t2[0], t2[1], ang[0], ang[1], "OK" if passed else "失败"))
    ok += passed; bad += (not passed)

    time.sleep(1.0)

    # 2) 摸头
    cmd("h")
    fr = take(1.5)
    om = rng(fr, 8)
    t2 = rng(fr, 1)
    passed = om[1] > 0.4 or t2[0] < 0.3
    print("[摸头]   omega=%.2f..%.2f  t2=%.2f..%.2f  %s" % (om[0], om[1], t2[0], t2[1], "OK" if passed else "失败"))
    ok += passed; bad += (not passed)

    time.sleep(1.0)

    # 3) 吓一下
    cmd("k")
    fr = take(1.2)
    bl = rng(fr, 10)
    sc = rng(fr, 6)
    passed = bl[1] > 0.4 or sc[1] > 1.03
    print("[惊吓]   blob=%.2f..%.2f  sc=%.3f..%.3f  %s" % (bl[0], bl[1], sc[0], sc[1], "OK" if passed else "失败"))
    ok += passed; bad += (not passed)

    # 4) 静止时自己会动
    time.sleep(1.5)
    fr = take(4.0)
    t1 = rng(fr, 0)
    moving = (t1[1] - t1[0]) > 0.15
    print("[空闲]   t1 波动 %.2f  %s" % (t1[1] - t1[0], "OK" if moving else "失败"))
    ok += moving; bad += (not moving)

    # 5) 光照 -> 变暗
    fr = take(1.0)
    dm = rng(fr, 15)
    print("[光感]   dim=%.2f..%.2f（覆盖光感应该升高）" % (dm[0], dm[1]))

    global stop
    stop = True
    print("\n结果: %d 通过, %d 失败" % (ok, bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
