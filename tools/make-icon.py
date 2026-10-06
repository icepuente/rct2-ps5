#!/usr/bin/env python3
"""Draw ps5/sce_sys/icon0.png, the 512x512 home-screen tile.

Original artwork matching the loading screen: a dusk sky, a coaster with a
lift hill and a vertical loop, a red train and the OpenRCT2 wordmark. Drawn at
4x and downsampled for smooth edges. Needs Pillow and the DejaVu fonts;
scripts/make-art.sh runs it in a container.
"""
import math
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = sys.argv[1] if len(sys.argv) > 1 else "ps5/sce_sys/icon0.png"
S = 4                      # supersampling factor
W = H = 512 * S
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

img = Image.new("RGB", (W, H))
draw = ImageDraw.Draw(img)

# Sky: deep blue at the top to a warm dusk at the horizon.
top, mid, bottom = (12, 18, 48), (58, 40, 92), (232, 120, 72)
for y in range(H):
    t = y / H
    if t < 0.55:
        a, b, u = top, mid, t / 0.55
    else:
        a, b, u = mid, bottom, (t - 0.55) / 0.45
    draw.line([(0, y), (W, y)], fill=tuple(int(p + (q - p) * u) for p, q in zip(a, b)))

# A few stars.
seed = 7
for _ in range(36):
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
    x = seed % W
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
    y = seed % int(H * 0.45)
    r = S * (2 if seed % 4 == 0 else 1)
    draw.ellipse([x - r, y - r, x + r, y + r], fill=(235, 236, 255))

# Ground and the track.
ground = int(H * 0.80)
ink = (10, 12, 24)


def lift(x):
    """The lift hill and drop on the left."""
    u = x / W
    return ground - H * 0.08 - H * 0.42 * math.exp(-((u - 0.24) / 0.12) ** 2)


track = [(x, lift(x)) for x in range(-W // 10, int(W * 0.42), S * 2)]

# A vertical loop that drifts right as it turns, like a real one.
cx, cy, r, drift = W * 0.62, ground - H * 0.08 - H * 0.21, H * 0.21, W * 0.012
loop = [(cx + r * math.sin(t) + drift * t, cy + r * math.cos(t))
        for t in [i / 200 * 2 * math.pi for i in range(201)]]
x0, y0 = track[-1]
lx, ly = loop[0]
track += [(x0 + (lx - x0) * k / 20, y0 + (ly - y0) * k / 20) for k in range(1, 21)]
track += loop[1:]
ex, ey = loop[-1]
track += [(ex + (W * 1.1 - ex) * k / 20, ey) for k in range(1, 21)]

# Lattice supports under the low parts of the track.
for x in range(0, W, 18 * S):
    ys = [py for px, py in track if abs(px - x) < S * 2]
    if not ys:
        continue
    y = max(ys)
    if y > ground - H * 0.02:
        continue
    draw.line([(x, y), (x, ground)], fill=ink, width=3 * S)
    step = 22 * S
    yy = y + 6 * S
    while yy + step < ground:
        draw.line([(x - 8 * S, yy), (x + 8 * S, yy + step)], fill=ink, width=S)
        draw.line([(x + 8 * S, yy), (x - 8 * S, yy + step)], fill=ink, width=S)
        yy += step

# A warm glow behind the rails so the track reads against the dark sky.
glow = Image.new("L", (W, H), 0)
ImageDraw.Draw(glow).line(track, fill=255, width=16 * S, joint="curve")
glow = glow.filter(ImageFilter.GaussianBlur(7 * S))
img.paste((255, 176, 96), (0, 0), glow.point(lambda v: v * 3 // 4))
draw = ImageDraw.Draw(img)

# Twin rails.
draw.line(track, fill=ink, width=7 * S, joint="curve")
draw.line([(x, y + 7 * S) for x, y in track], fill=ink, width=4 * S, joint="curve")
draw.rectangle([0, ground, W, H], fill=ink)

# The train cresting the lift hill: cars tilted to follow the track.
for i in range(3):
    tx = W * 0.24 - 34 * S + i * 34 * S
    ty = lift(tx)
    angle = math.degrees(math.atan2(lift(tx + S) - lift(tx - S), 2 * S))
    car = Image.new("RGBA", (56 * S, 44 * S), (0, 0, 0, 0))
    cd = ImageDraw.Draw(car)
    cd.rounded_rectangle([12 * S, 8 * S, 44 * S, 30 * S], radius=7 * S, fill=(240, 62, 54))
    cd.rounded_rectangle([16 * S, 12 * S, 40 * S, 17 * S], radius=2 * S, fill=(255, 150, 120))
    cd.ellipse([15 * S, 26 * S, 23 * S, 34 * S], fill=ink)
    cd.ellipse([33 * S, 26 * S, 41 * S, 34 * S], fill=ink)
    car = car.rotate(-angle, resample=Image.BICUBIC, center=(28 * S, 34 * S))
    img.paste(car, (int(tx - 28 * S), int(ty - 34 * S)), car)
draw = ImageDraw.Draw(img)

# Wordmark with a soft shadow.
font = ImageFont.truetype(FONT_BOLD, 80 * S)
text = "OpenRCT2"
tw = draw.textlength(text, font=font)
tx, ty = (W - tw) / 2, H * 0.825
shadow = Image.new("L", (W, H), 0)
ImageDraw.Draw(shadow).text((tx + 4 * S, ty + 5 * S), text, font=font, fill=200)
shadow = shadow.filter(ImageFilter.GaussianBlur(5 * S))
img.paste((0, 0, 0), (0, 0), shadow)
draw = ImageDraw.Draw(img)
draw.text((tx, ty), text, font=font, fill=(255, 255, 255))

img.resize((512, 512), Image.LANCZOS).save(OUT, optimize=True)
print(f"wrote {OUT}")
