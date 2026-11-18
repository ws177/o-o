# -*- coding: utf-8 -*-
"""mouth_debug.py — 把嘴型在各种权重组合下单独画出来，找出"最小的时候"哪里怪。"""
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

from PIL import Image, ImageDraw, ImageFont

MOUTH_N = 44
W, H = 420, 260


def edge(kind, t):
    if kind == "chev":
        return (-1 + 2 * t, 0.44 * (2 * abs(2 * t - 1) - 1))
    if kind == "omega":
        return (-1 + 2 * t, -0.20 * __import__("math").cos(2 * __import__("math").pi * t) - 0.02)
    return (-1 + 2 * t, 0.70 * (1 - 4 * (t - 0.5) ** 2) - 0.35)


def pts(kind):
    import math
    n = 22
    spec = {
        "chev":  (lambda t: 0.44 * (2 * abs(2 * t - 1) - 1), lambda t: 0.20),
        "omega": (lambda t: -0.20 * math.cos(2 * math.pi * t) - 0.02, lambda t: 0.20),
        "smile": (lambda t: 0.70 * (1 - 4 * (t - 0.5) ** 2) - 0.35, lambda t: 0.24),
        "blob":  (lambda t: 0.06, lambda t: 0.95),
    }[kind]
    X = lambda t: -1 + 2 * t
    p = []
    for i in range(n):
        t = i / (n - 1)
        p.append((X(t), spec[0](t) - spec[1](t) / 2))
    r = spec[1](1) / 2; cy = spec[0](1); cx = X(1)
    for i in range(1, 8):
        a = -math.pi / 2 + math.pi * i / 8
        p.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    for i in range(n - 1, -1, -1):
        t = i / (n - 1)
        p.append((X(t), spec[0](t) + spec[1](t) / 2))
    r = spec[1](0) / 2; cy = spec[0](0); cx = X(0)
    for i in range(1, 8):
        a = math.pi / 2 + math.pi * i / 8
        p.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    return p


MP = {k: pts(k) for k in ("chev", "omega", "smile", "blob")}

CASES = [
    ("chev 1", 1, 0, 0, 0), ("omega 1", 0, 1, 0, 0), ("smile 1", 0, 0, 1, 0), ("blob 1", 0, 0, 0, 1),
    ("chev+smile", .5, 0, .5, 0), ("chev+omega", .5, .5, 0, 0),
    ("smile+blob", 0, 0, .5, .5), ("chev+blob", .5, 0, 0, .5),
]


def render(w_ch, w_om, w_sm, w_bl, mw):
    img = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(img)
    s = (w_ch + w_om + w_bl + w_sm) or 1
    cx, cy = W / 2, H / 2
    poly = []
    for i in range(MOUTH_N):
        a, b, c, e = MP["chev"][i], MP["omega"][i], MP["smile"][i], MP["blob"][i]
        x = (a[0] * w_ch + b[0] * w_om + c[0] * w_sm + e[0] * w_bl) / s
        y = (a[1] * w_ch + b[1] * w_om + c[1] * w_sm + e[1] * w_bl) / s
        poly.append((cx + x * mw, cy + y * mw))
    d.polygon(poly, fill=(255, 255, 255))
    return img


def main():
    out = os.path.join(ROOT, "docs", "mouth_debug.png")
    cols, tw, th, pad, lab = 4, W, H, 6, 20
    rows = (len(CASES) + cols - 1) // cols
    sheet = Image.new("RGB", (pad + cols * (tw + pad), pad + rows * (th + lab + pad)), (26, 26, 30))
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 15)
    except Exception:
        font = ImageFont.load_default()
    for i, (name, a, b, c, e) in enumerate(CASES):
        # 上排用正常尺寸，下排用很小的尺寸
        for k, mw in enumerate((126, 40)):
            r, col = divmod(i, cols)
            if k == 1:
                continue
        r, col = divmod(i, cols)
        x = pad + col * (tw + pad)
        y = pad + r * (th + lab + pad)
        sheet.paste(render(a, b, c, e, 126), (x, y))
        d.text((x + 4, y + th + 2), name + "  (mw=126)", font=font, fill=(200, 200, 210))
    sheet.save(out)
    print("saved", out)

    # 再来一张：极小尺寸
    sheet2 = Image.new("RGB", (pad + cols * (tw + pad), pad + rows * (th + lab + pad)), (26, 26, 30))
    d2 = ImageDraw.Draw(sheet2)
    for i, (name, a, b, c, e) in enumerate(CASES):
        r, col = divmod(i, cols)
        x = pad + col * (tw + pad)
        y = pad + r * (th + lab + pad)
        sheet2.paste(render(a, b, c, e, 18), (x, y))
        d2.text((x + 4, y + th + 2), name + "  (mw=18 极小)", font=font, fill=(200, 200, 210))
    out2 = os.path.join(ROOT, "docs", "mouth_debug_small.png")
    sheet2.save(out2)
    print("saved", out2)


if __name__ == "__main__":
    main()
