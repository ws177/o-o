# -*- coding: utf-8 -*-
"""pose_sheet.py — 把 engine.json 里所有姿态渲染成一张对照图

前提：本地已跑起虚拟屏幕服务（tools/vscreen.py），或自己指定 --url。
用法：
    python tools/pose_sheet.py
    python tools/pose_sheet.py --url http://127.0.0.1:8777/o-o --out docs/pose_sheet.png
"""
import argparse
import json
import os
import subprocess

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CW, CH = 560, 320
COLS = 4


def find_edge():
    """找一个可用的 Chromium 内核浏览器（Edge / Chrome）"""
    for p in [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
    ]:
        if os.path.exists(p):
            return p
    return os.environ.get("BROWSER", "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:8777/o-o")
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "pose_sheet.png"))
    ap.add_argument("--keep", default=os.path.join(ROOT, "docs", "poses"),
                    help="单张图输出目录")
    args = ap.parse_args()

    edge = find_edge()
    if not edge:
        print("找不到 Edge/Chrome，可以用环境变量 BROWSER 指定路径")
        return 1

    engine = os.path.join(ROOT, "engine", "engine.json")
    poses = list(json.load(open(engine, encoding="utf-8"))["poses"].keys())

    os.makedirs(args.keep, exist_ok=True)
    imgs = []
    for name in poses:
        p = os.path.join(args.keep, "%s.png" % name)
        if os.path.exists(p):
            os.remove(p)
        subprocess.run([edge, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                        "--window-size=%d,%d" % (CW, CH), "--virtual-time-budget=1500",
                        "--screenshot=" + p, args.url + "?pose=%s&still=1" % name],
                       capture_output=True, timeout=90)
        imgs.append(p if os.path.exists(p) else None)
        print("  %-10s %s" % (name, "OK" if imgs[-1] else "失败"), flush=True)

    rows = (len(poses) + COLS - 1) // COLS
    pad, lab = 5, 22
    sheet = Image.new("RGB", (pad + COLS * (CW + pad), pad + rows * (CH + lab + pad)), (18, 18, 22))
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 16)
    except Exception:
        font = ImageFont.load_default()
    for i, (name, p) in enumerate(zip(poses, imgs)):
        r, c = divmod(i, COLS)
        x, y = pad + c * (CW + pad), pad + r * (CH + lab + pad)
        if p and os.path.exists(p):
            sheet.paste(Image.open(p).convert("RGB"), (x, y))
        d.text((x + 4, y + CH + 2), name, font=font, fill=(150, 230, 190))
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    sheet.save(args.out)
    print("对照图:", args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
