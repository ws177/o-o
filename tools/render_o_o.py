# -*- coding: utf-8 -*-
"""render_o_o.py — 用 Pillow 复刻 ouo.html 的绘制数学，用来出对比图。
   眼 = 实心圆 ∩ 可旋转的眼皮带；嘴 = 四种姿态逐点插值。"""
import math
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw

W, H = 1600, 900
R = min(W, H)

EYE_R, EYE_DX, EYE_Y = 0.139, 0.303, 0.417
MOUTH_W, MOUTH_Y = 0.1575, 0.652

POSE = {
    "open":     (-1.05, 1.05, 0.00),
    "dome":     (-1.05, 0.03, 0.00),
    "blink":    (0.15, 1.05, 0.00),
    "crescent": (-0.85, -0.28, 0.30),
    "bar":      (-0.11, 0.11, 0.00),
}

MOUTH_N = 44


def edge(kind, t):
    if kind == "chevron":
        return (-1 + 2 * t, 0.44 * (2 * abs(2 * t - 1) - 1))
    if kind == "omega":
        return (-1 + 2 * t, -0.20 * math.cos(2 * math.pi * t) - 0.02)
    if kind == "smile":
        return (-1 + 2 * t, 0.70 * (1 - 4 * (t - 0.5) ** 2) - 0.35)
    return (-1 + 2 * t, 0.0)


def mouth_points(kind):
    thick = {"chevron": 0.22, "omega": 0.20, "smile": 0.26}.get(kind, 0.18)
    if kind == "blob":
        return [(math.cos(2 * math.pi * i / MOUTH_N) * 1.0,
                 math.sin(2 * math.pi * i / MOUTH_N) * 0.92 + 0.05) for i in range(MOUTH_N)]
    n = MOUTH_N // 2
    pts = []
    for i in range(n):
        p = edge(kind, i / (n - 1))
        pts.append((p[0], p[1] - thick / 2))
    for i in range(n - 1, -1, -1):
        p = edge(kind, i / (n - 1))
        pts.append((p[0], p[1] + thick / 2))
    return pts


MP = {k: mouth_points(k) for k in ("chevron", "omega", "smile", "blob")}


def rot(x, y, a):
    c, s = math.cos(a), math.sin(a)
    return x * c - y * s, x * s + y * c


def draw_eye(img, cx, cy, er, t1, t2, ang):
    a = Image.new("L", (W, H), 0)
    ImageDraw.Draw(a).ellipse([cx - er, cy - er, cx + er, cy + er], fill=255)

    b = Image.new("L", (W, H), 0)
    corners = [(-2 * er, t1 * er), (2 * er, t1 * er), (2 * er, t2 * er), (-2 * er, t2 * er)]
    poly = []
    for (x, y) in corners:
        rx, ry = rot(x, y, ang)
        poly.append((cx + rx, cy + ry))
    ImageDraw.Draw(b).polygon(poly, fill=255)

    mask = Image.new("L", (W, H), 0)
    mask.paste(Image.eval(a, lambda v: v), (0, 0))
    from PIL import ImageChops
    mask = ImageChops.multiply(mask, b)
    white = Image.new("RGB", (W, H), (255, 255, 255))
    img.paste(white, (0, 0), mask)


def draw_mouth(img, w_ch, w_om, w_bl, w_sm):
    s = w_ch + w_om + w_bl + w_sm or 1
    mw = MOUTH_W * W * 0.5
    cx, cy = W / 2, MOUTH_Y * H
    pts = []
    for i in range(MOUTH_N):
        x = (MP["chevron"][i][0] * w_ch + MP["omega"][i][0] * w_om +
             MP["smile"][i][0] * w_sm + MP["blob"][i][0] * w_bl) / s
        y = (MP["chevron"][i][1] * w_ch + MP["omega"][i][1] * w_om +
             MP["smile"][i][1] * w_sm + MP["blob"][i][1] * w_bl) / s
        pts.append((cx + x * mw, cy + y * mw))
    ImageDraw.Draw(img).polygon(pts, fill=(255, 255, 255))


def render(pose, mouth, mirrored_angle=True):
    img = Image.new("RGB", (W, H), (0, 0, 0))
    t1, t2, ang = POSE[pose]
    er = EYE_R * R
    dx = EYE_DX * W
    cy = EYE_Y * H
    draw_eye(img, W / 2 - dx, cy, er, t1, t2, ang)
    draw_eye(img, W / 2 + dx, cy, er, t1, t2, -ang if mirrored_angle else ang)
    draw_mouth(img, *mouth)
    return img


def main():
    out = os.path.join(ROOT, "docs", "render")
    os.makedirs(out, exist_ok=True)
    cases = {
        "idle":   ("open", (1, 0, 0, 0)),
        "blink":  ("blink", (1, 0, 0, 0)),
        "angry":  ("crescent", (0, 0, 0.35, 0)),
        "happy":  ("dome", (0, 0, 0, 1)),
        "pet":    ("dome", (0, 1, 0, 0)),
        "shock":  ("open", (0, 0, 1, 0)),
    }
    for name, (pose, m) in cases.items():
        render(pose, m).save(os.path.join(out, name + ".png"))
    print("saved", out)


if __name__ == "__main__":
    main()
