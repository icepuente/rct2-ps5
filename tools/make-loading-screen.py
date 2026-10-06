#!/usr/bin/env python3
"""Draw ps5/assets/loading.bmp, the screen shown while OpenRCT2 starts.

Original artwork: a night sky and a coaster track silhouette. Needs Pillow and
the DejaVu fonts; scripts/make-art.sh runs it in a container.
"""
import math
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 1920, 1080
OUT = sys.argv[1] if len(sys.argv) > 1 else "ps5/assets/loading.bmp"
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

img = Image.new("RGB", (W, H))
draw = ImageDraw.Draw(img)

# Sky: deep blue at the top fading to a warm dusk near the horizon.
top, bottom = (14, 22, 52), (196, 104, 70)
for y in range(H):
    t = (y / H) ** 1.6
    draw.line([(0, y), (W, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)))

# A few stars in the upper sky (deterministic).
seed = 12345
for _ in range(140):
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
    x = seed % W
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
    y = seed % (H // 2)
    if 400 < x < 1520 and 230 < y < 540:
        continue  # keep the text area clear
    r = 1 if seed % 5 else 2
    draw.ellipse([x - r, y - r, x + r, y + r], fill=(230, 232, 255))

# Ground.
ground_y = H - 120
draw.rectangle([0, ground_y, W, H], fill=(10, 14, 24))


def track_y(x):
    """Height of the track: a lift hill, a drop and smaller airtime hills."""
    u = x / W
    lift = 520 * math.exp(-((u - 0.22) / 0.11) ** 2)
    hills = 150 * math.exp(-((u - 0.55) / 0.07) ** 2) + 110 * math.exp(-((u - 0.78) / 0.06) ** 2)
    return ground_y - 140 - lift - hills


silhouette = (8, 12, 22)

# Lattice supports under the track.
for x in range(0, W + 1, 48):
    y = track_y(x)
    draw.line([(x, y + 8), (x, ground_y)], fill=silhouette, width=6)
    step = 60
    yy = y + 20
    while yy + step < ground_y:
        draw.line([(x - 24, yy), (x + 24, yy + step)], fill=silhouette, width=3)
        draw.line([(x + 24, yy), (x - 24, yy + step)], fill=silhouette, width=3)
        yy += step

# Rails and ties.
points = [(x, track_y(x)) for x in range(0, W + 1, 4)]
draw.line(points, fill=silhouette, width=10)
draw.line([(x, y + 16) for x, y in points], fill=silhouette, width=6)
for x in range(0, W + 1, 22):
    y = track_y(x)
    draw.line([(x, y), (x, y + 16)], fill=silhouette, width=4)

# A train cresting the lift hill.
for i in range(4):
    cx = W * 0.22 - 81 + i * 54
    cy = track_y(cx)
    draw.rounded_rectangle([cx - 24, cy - 34, cx + 24, cy - 4], radius=8, fill=(232, 72, 60))
    draw.ellipse([cx - 16, cy - 10, cx - 6, cy], fill=silhouette)
    draw.ellipse([cx + 6, cy - 10, cx + 16, cy], fill=silhouette)

# Text.
title = ImageFont.truetype(FONT_BOLD, 132)
body = ImageFont.truetype(FONT, 40)
small = ImageFont.truetype(FONT, 30)


def centred(y, text, font, fill):
    w = draw.textlength(text, font=font)
    draw.text(((W - w) / 2 + 3, y + 3), text, font=font, fill=(0, 0, 0))
    draw.text(((W - w) / 2, y), text, font=font, fill=fill)


centred(250, "OpenRCT2", title, (255, 255, 255))
centred(420, "Loading…", body, (235, 235, 245))
centred(480, "The first launch indexes objects and scenarios and can take a minute.",
        small, (210, 210, 225))
draw.text((40, H - 70), "PS5 port · github.com/icepuente/rct2-ps5", font=small, fill=(150, 156, 180))

img.quantize(colors=256, dither=Image.Dither.NONE).save(OUT)
print(f"wrote {OUT}")
