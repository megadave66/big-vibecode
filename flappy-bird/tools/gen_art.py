#!/usr/bin/env python3
"""Generate the original pipe and ground art for this project (needs Pillow).

Run from the project root:  python3 tools/gen_art.py
Writes assets/sprites/pipe_body.png, pipe_cap.png and ground.png.
All output is original work for this project (CC0).
"""
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "assets" / "sprites"

OUTLINE = (40, 56, 30, 255)


def shade_row(width, base, light, dark, img, y, x0=0):
    """Horizontal cylinder shading: dark edges, light band at 1/4 width."""
    for x in range(width):
        t = x / max(1, width - 1)
        if t < 0.08 or t > 0.92:
            c = dark
        elif 0.18 < t < 0.32:
            c = light
        elif t > 0.75:
            c = tuple(int(b * 0.85) for b in base[:3]) + (255,)
        else:
            c = base
        img.putpixel((x0 + x, y), c)


def pipe_body():
    w, h = 52, 16  # tiles vertically
    img = Image.new("RGBA", (w, h))
    for y in range(h):
        shade_row(w, (92, 176, 64, 255), (170, 230, 120, 255), (52, 110, 40, 255), img, y)
        img.putpixel((0, y), OUTLINE)
        img.putpixel((w - 1, y), OUTLINE)
    img.save(OUT / "pipe_body.png")


def pipe_cap():
    w, h = 58, 26
    img = Image.new("RGBA", (w, h))
    for y in range(h):
        shade_row(w, (100, 190, 70, 255), (185, 240, 135, 255), (55, 118, 42, 255), img, y)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w - 1, h - 1], outline=OUTLINE, width=2)
    img.save(OUT / "pipe_cap.png")


def ground():
    w, h = 336, 112  # 288 world width + 48 so a 24 px scroll offset always covers the screen
    img = Image.new("RGBA", (w, h), (222, 206, 140, 255))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w, 2], fill=OUTLINE)
    d.rectangle([0, 3, w, 14], fill=(115, 191, 46, 255))
    # diagonal grass stripes, period 24 px so a scroll offset in [0, 24) loops seamlessly
    for x0 in range(-24, w + 24, 24):
        d.polygon([(x0, 3), (x0 + 12, 3), (x0 + 6, 14), (x0 - 6, 14)], fill=(150, 220, 80, 255))
    d.rectangle([0, 15, w, 18], fill=(84, 140, 40, 255))
    d.rectangle([0, 19, w, 22], fill=(200, 180, 110, 255))
    for x0 in range(0, w, 24):
        d.rectangle([x0 + 4, 40, x0 + 7, 42], fill=(205, 188, 122, 255))
        d.rectangle([x0 + 16, 70, x0 + 19, 72], fill=(205, 188, 122, 255))
    img.save(OUT / "ground.png")


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    pipe_body()
    pipe_cap()
    ground()
    print("wrote", ", ".join(p.name for p in sorted(OUT.glob("*.png"))))
