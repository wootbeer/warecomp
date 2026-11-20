#!/usr/bin/env python3
"""Generates the app icon (icons/app.png, icons/app.ico, icons/app.res): an
original placeholder, a "WA" monogram in the UI's Exo 2 font on a rounded
square. Nothing from the game's own artwork.

    python3 tools/make_app_icon.py

app.res needs llvm-rc (it wraps app.ico as icon 1 for the Windows .exe).
"""
import os, subprocess
from PIL import Image, ImageDraw, ImageFont

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
icons = os.path.join(root, "icons")
os.makedirs(icons, exist_ok=True)
S = 1024
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
# background: deep blue, a lighter band at the bottom like rolling ground
d.rounded_rectangle([24, 24, S - 24, S - 24], radius=200, fill=(22, 38, 82, 255))
d.pieslice([-300, 640, S + 300, 1500], 180, 360, fill=(54, 140, 72, 255))
d.rounded_rectangle([24, 24, S - 24, S - 24], radius=200, outline=(255, 255, 255, 255), width=28)
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle([24, 24, S - 24, S - 24], radius=200, fill=255)
img.putalpha(Image.composite(img.getchannel("A"), Image.new("L", (S, S), 0), mask))
font = ImageFont.truetype(os.path.join(root, "assets", "Exo2.ttf"), 470)
try:
    font.set_variation_by_name("Black")
except Exception:
    pass
d = ImageDraw.Draw(img)
box = d.textbbox((0, 0), "WA", font=font)
x = (S - (box[2] - box[0])) / 2 - box[0]
y = (S - (box[3] - box[1])) / 2 - box[1] - 40
d.text((x + 14, y + 14), "WA", font=font, fill=(0, 0, 0, 140))
d.text((x, y), "WA", font=font, fill=(255, 214, 64, 255))
img.resize((256, 256), Image.LANCZOS).save(os.path.join(icons, "app.png"))
img.save(os.path.join(icons, "app.ico"), sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)])
rc = os.path.join(icons, "app.rc")
with open(rc, "w") as f:
    f.write('1 ICON "app.ico"\n')
try:
    subprocess.run(["llvm-rc", "/FO", "app.res", "app.rc"], cwd=icons, check=True)
finally:
    os.remove(rc)
print("wrote icons/app.png, app.ico, app.res")
