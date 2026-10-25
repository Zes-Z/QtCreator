#!/usr/bin/env python3
"""Generate the macOS app icon (assets/DailyUp.icns) for DailyUp.

Rounded-square icon with a soft blue gradient and a white check mark,
following the macOS Big Sur icon grid (824 pt artwork inside 1024 pt canvas).
"""
from pathlib import Path
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
ASSETS.mkdir(exist_ok=True)

S = 1024
PAD = 100
N = S - PAD * 2
RADIUS = 190
SCALE = 2  # supersample for smooth edges


def rounded_mask(size: int, radius: int, ss: int = 4) -> Image.Image:
    m = Image.new("L", (size * ss, size * ss), 0)
    d = ImageDraw.Draw(m)
    d.rounded_rectangle(
        (0, 0, size * ss - 1, size * ss - 1),
        radius=radius * ss,
        fill=255,
    )
    return m.resize((size, size), Image.LANCZOS)


def linear_gradient(size: int) -> Image.Image:
    """Diagonal blue -> indigo gradient."""
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32) / (size - 1)
    t = np.clip(xx * 0.45 + yy * 0.75, 0.0, 1.0)

    c0 = np.array([124, 181, 255], dtype=np.float32)   # sky
    c1 = np.array([61, 123, 247], dtype=np.float32)    # blue
    c2 = np.array([92, 92, 240], dtype=np.float32)     # indigo

    rgb = np.zeros((size, size, 3), dtype=np.float32)
    lo = t < 0.55
    k = np.clip(t / 0.55, 0, 1)[..., None]
    rgb[lo] = (c0 * (1 - k) + c1 * k)[lo]
    k = np.clip((t - 0.55) / 0.45, 0, 1)[..., None]
    rgb[~lo] = (c1 * (1 - k) + c2 * k)[~lo]
    return Image.fromarray(rgb.clip(0, 255).astype(np.uint8), "RGB")


def main() -> None:
    canvas = Image.new("RGBA", (S, S), (0, 0, 0, 0))

    # --- soft ambient shadow -------------------------------------------------
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle(
        (PAD + 6, PAD + 26, PAD + N - 6, PAD + N + 14),
        radius=RADIUS,
        fill=(23, 43, 99, 92),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(34))
    canvas.alpha_composite(shadow)

    # --- body ---------------------------------------------------------------
    body = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    body.paste(linear_gradient(N), (0, 0))

    # inner top light: simulates a soft bevel
    glow = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for i in range(140):
        a = int(38 * (1 - i / 140) ** 2)
        gd.rounded_rectangle(
            (i, i, N - 1 - i, N - 1 - i),
            radius=max(RADIUS - i, 8),
            outline=(255, 255, 255, a),
            width=1,
        )
    body = Image.alpha_composite(body, glow)

    # top sheen
    sheen = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    sh = ImageDraw.Draw(sheen)
    sh.ellipse((N * 0.06, -N * 0.42, N * 0.94, N * 0.52), fill=(255, 255, 255, 46))
    sheen = sheen.filter(ImageFilter.GaussianBlur(70))
    body = Image.alpha_composite(body, sheen)

    body.putalpha(rounded_mask(N, RADIUS, ss=4))
    canvas.alpha_composite(body, (PAD, PAD))

    # --- check mark ---------------------------------------------------------
    check_layer = Image.new("RGBA", (S * SCALE, S * SCALE), (0, 0, 0, 0))
    cd = ImageDraw.Draw(check_layer)
    pts = [(328, 516), (458, 648), (706, 392)]
    pts = [(x * SCALE, y * SCALE) for x, y in pts]
    width = 78 * SCALE
    # shadow under the glyph
    sh_pts = [(x + 3 * SCALE, y + 9 * SCALE) for x, y in pts]
    cd.line(sh_pts, fill=(20, 42, 105, 55), width=width, joint="curve")
    for x, y in (sh_pts[0], sh_pts[-1]):
        r = width // 2
        cd.ellipse((x - r, y - r, x + r, y + r), fill=(20, 42, 105, 55))
    check_layer = check_layer.filter(ImageFilter.GaussianBlur(5 * SCALE))
    check_layer = check_layer.resize((S, S), Image.LANCZOS)
    canvas.alpha_composite(check_layer)

    check_layer = Image.new("RGBA", (S * SCALE, S * SCALE), (0, 0, 0, 0))
    cd = ImageDraw.Draw(check_layer)
    cd.line(pts, fill=(255, 255, 255, 255), width=width, joint="curve")
    for x, y in (pts[0], pts[-1]):
        r = width // 2
        cd.ellipse((x - r, y - r, x + r, y + r), fill=(255, 255, 255, 255))
    check_layer = check_layer.resize((S, S), Image.LANCZOS)
    canvas.alpha_composite(check_layer)

    # --- export -------------------------------------------------------------
    png_1024 = ASSETS / "DailyUp-1024.png"
    canvas.save(png_1024)

    iconset = ASSETS / "DailyUp.iconset"
    iconset.mkdir(exist_ok=True)
    sizes = [(16, 1), (16, 2), (32, 1), (32, 2), (128, 1), (128, 2),
             (256, 1), (256, 2), (512, 1), (512, 2)]
    for base, scale in sizes:
        px = base * scale
        name = f"icon_{base}x{base}{'@2x' if scale == 2 else ''}.png"
        canvas.resize((px, px), Image.LANCZOS).save(iconset / name)

    print("wrote", png_1024)
    print("iconset", iconset)


if __name__ == "__main__":
    main()
