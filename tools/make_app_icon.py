#!/usr/bin/env python3
"""Generates the app icons: a rusty radiation symbol (the trefoil on a worn
yellow disc, its paint chipped and pitted with rust), drawn from scratch. The
radiation trefoil is the standard symbol; nothing here comes from the game's
own artwork.

    python3 tools/make_app_icon.py

Writes icons/app.png, icons/app.ico and icons/app.res (the desktop builds;
app.res needs llvm-rc, it wraps app.ico as icon 1 for the Windows .exe), and
the Android launcher icons (android/app/src/main/res: the legacy icon, and the
adaptive icon's foreground over ic_launcher_background).
"""
import math, os, subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
icons = os.path.join(root, "icons")
res = os.path.join(root, "android", "app", "src", "main", "res")
os.makedirs(icons, exist_ok=True)

S = 1024
rng = np.random.default_rng(1999)  # fixed: the same icon every run


def noise(scale, octaves=4):
    """Smooth value noise in 0..1, S x S, features about `scale` pixels."""
    out = np.zeros((S, S))
    amp, total = 1.0, 0.0
    for o in range(octaves):
        cells = max(2, int(S / (scale / (2 ** o))))
        grid = rng.random((cells + 1, cells + 1))
        img = Image.fromarray((grid * 255).astype(np.uint8)).resize((S, S), Image.BICUBIC)
        out += np.asarray(img, dtype=float) / 255.0 * amp
        total += amp
        amp *= 0.5
    return out / total


yy, xx = np.mgrid[0:S, 0:S].astype(float)
cx = cy = (S - 1) / 2
dx, dy = xx - cx, yy - cy
r = np.hypot(dx, dy)
ang = (np.degrees(np.arctan2(-dy, dx)) + 360.0) % 360.0  # 0 = right, counterclockwise, y up

R = S * 0.47            # the disc
rim = S * 0.035         # its darker worn rim
r_hub = R * 0.15        # the trefoil's centre
r_in, r_out = R * 0.24, R * 0.80  # its blades

big = noise(260)
mid = noise(90)
fine = noise(18, 3)
grain = rng.random((S, S))

# --- The disc: weathered yellow-orange paint over rusty metal. ------------
paint = np.zeros((S, S, 3))
yellow = np.array([236, 178, 38], float)
orange = np.array([196, 104, 26], float)
t = np.clip(r / R, 0, 1) ** 1.6 * 0.6 + big * 0.5
paint[:] = yellow
paint = paint * (1 - t[..., None]) + orange * t[..., None]
paint *= (0.88 + 0.18 * mid + 0.08 * (grain - 0.5))[..., None]

rust_dark = np.array([92, 40, 16], float)
rust_mid = np.array([150, 70, 26], float)
# Rust patches where the paint has gone: blotches from the noise, more towards the rim.
rust_amt = np.clip((mid * 0.7 + fine * 0.5 + (r / R) ** 3 * 0.45) - 0.78, 0, 1) * 4.0
rust_amt = np.clip(rust_amt, 0, 1)
rust_col = rust_mid * (1 - fine[..., None]) + rust_dark * fine[..., None]
disc = paint * (1 - rust_amt[..., None]) + rust_col * rust_amt[..., None]

# The rim: a darker ring, pitted.
rim_amt = np.clip((r - (R - rim)) / rim, 0, 1)
disc = disc * (1 - rim_amt[..., None] * 0.75) + rust_dark * rim_amt[..., None] * 0.75

# Light from the top left, a soft dome.
shade = 1.0 + 0.16 * (-(dx + dy) / (R * 1.4)) - 0.10 * (r / R) ** 2
disc *= shade[..., None]

# --- The trefoil: three blades and a hub, black paint worn by rust. -------
blade = np.zeros((S, S), bool)
for centre in (90.0, 210.0, 330.0):  # one blade up, two down, as the symbol stands
    d_ang = np.abs((ang - centre + 180.0) % 360.0 - 180.0)
    blade |= (d_ang <= 30.0) & (r >= r_in) & (r <= r_out)
symbol = blade | (r <= r_hub)
# Rounded edges a little, then eaten at by rust: chips along the edges, pits inside.
sym_img = Image.fromarray((symbol * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3))
sym = np.asarray(sym_img, float) / 255.0
edge_wear = np.clip((fine * 0.9 + mid * 0.4) - 0.62, 0, 1) * 3.5
pits = (fine > 0.80) & (grain > 0.55)
sym_alpha = np.clip(sym * 1.6 - 0.3 - edge_wear * (1 - sym) * 2.0, 0, 1)
sym_alpha = np.where(pits, sym_alpha * 0.35, sym_alpha)
sym_alpha *= 1.0 - np.clip(rust_amt - 0.55, 0, 1) * 0.8

black = np.array([30, 22, 18], float)
black_worn = black * (0.85 + 0.5 * fine[..., None]) + rust_dark * 0.15
out_rgb = disc * (1 - sym_alpha[..., None]) + black_worn * sym_alpha[..., None]

# Scratches: thin light lines across everything.
scr = Image.new("L", (S, S), 0)
sd = ImageDraw.Draw(scr)
for _ in range(26):
    a = rng.uniform(0, math.pi)
    length = rng.uniform(S * 0.05, S * 0.22)
    px, py = rng.uniform(S * 0.15, S * 0.85, 2)
    sd.line([(px, py), (px + math.cos(a) * length, py + math.sin(a) * length)],
            fill=int(rng.uniform(60, 140)), width=int(rng.integers(2, 5)))
scr_a = np.asarray(scr.filter(ImageFilter.GaussianBlur(0.8)), float) / 255.0
out_rgb = out_rgb * (1 - scr_a[..., None] * 0.5) + np.array([250, 220, 160]) * scr_a[..., None] * 0.5

# The disc's edge, anti-aliased and a little uneven.
edge = R + (mid - 0.5) * S * 0.012
alpha = np.clip(edge - r + 0.5, 0, 1)
rgba = np.dstack([np.clip(out_rgb, 0, 255), alpha * 255]).astype(np.uint8)
icon = Image.fromarray(rgba, "RGBA")

# A soft shadow under the disc, so it sits on any background.
shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ImageDraw.Draw(shadow).ellipse([cx - R + 6, cy - R + 18, cx + R + 6, cy + R + 18], fill=(0, 0, 0, 110))
shadow = shadow.filter(ImageFilter.GaussianBlur(14))
desktop = Image.alpha_composite(shadow, icon)

# --- Desktop icons. ---------------------------------------------------------
desktop.resize((256, 256), Image.LANCZOS).save(os.path.join(icons, "app.png"))
desktop.save(os.path.join(icons, "app.ico"), sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)])
rc = os.path.join(icons, "app.rc")
with open(rc, "w") as f:
    f.write('1 ICON "app.ico"\n')
try:
    subprocess.run(["llvm-rc", "/FO", "app.res", "app.rc"], cwd=icons, check=True)
except (OSError, subprocess.CalledProcessError) as e:
    print(f"app.res not written ({e})")
finally:
    os.remove(rc)

# --- Android launcher icons. -----------------------------------------------
# Adaptive (Android 8+): the disc within the 66% safe zone of a 432 x 432 foreground, over a dark
# rusty background colour (values/ic_launcher_background.xml). Legacy: the disc on that colour.
bg = (46, 26, 16, 255)
if os.path.isdir(res):
    fg = Image.new("RGBA", (432, 432), (0, 0, 0, 0))
    art = desktop.resize((300, 300), Image.LANCZOS)
    fg.paste(art, (66, 66), art)
    fg.save(os.path.join(res, "drawable-nodpi", "ic_launcher_foreground.png"))

    legacy = Image.new("RGBA", (144, 144), (0, 0, 0, 0))
    m = Image.new("L", (144 * 4, 144 * 4), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, 144 * 4 - 1, 144 * 4 - 1], radius=28 * 4, fill=255)
    legacy.paste(Image.new("RGBA", (144, 144), bg), (0, 0), m.resize((144, 144), Image.LANCZOS))
    art = desktop.resize((128, 128), Image.LANCZOS)
    legacy.alpha_composite(art, (8, 8))
    legacy.save(os.path.join(res, "mipmap-xxhdpi", "ic_launcher.png"))

    with open(os.path.join(res, "values", "ic_launcher_background.xml"), "w") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n<resources>\n'
                f'    <color name="ic_launcher_background">#{bg[0]:02X}{bg[1]:02X}{bg[2]:02X}</color>\n'
                '</resources>\n')

print("wrote icons/app.png, app.ico, app.res and the Android launcher icons")
