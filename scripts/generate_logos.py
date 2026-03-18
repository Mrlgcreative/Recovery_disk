#!/usr/bin/env python3
"""
generate_logos.py
-----------------
Génère tous les assets graphiques du HDD Password Recovery Tool :
  - resources/app.ico          (multi-taille : 16, 32, 48, 64, 128, 256)
  - resources/logo_256.png     (logo PNG 256x256 pour usage in-app / docs)
  - resources/wizard_large.bmp (164×314 pour Inno Setup)
  - resources/wizard_small.bmp (55×58  pour Inno Setup)

Design : Disque dur stylisé avec cadenas, palette bleu profond / blanc.
"""

import math
import os
from PIL import Image, ImageDraw, ImageFont

# --------------------------------------------------------------------------
#  Couleurs
# --------------------------------------------------------------------------
BG_DARK   = (18, 22, 36)       # fond sombre
BLUE_1    = (40, 80, 180)      # bleu principal
BLUE_2    = (60, 120, 220)     # bleu clair
BLUE_3    = (100, 160, 255)    # bleu accent
WHITE     = (230, 235, 245)
GRAY      = (140, 150, 170)
GOLD      = (255, 200, 60)
DARK_GRAY = (50, 55, 70)

RESOURCES = os.path.join(os.path.dirname(__file__), '..', 'resources')


def draw_hdd_icon(img: Image.Image, cx: int, cy: int, r: int):
    """Dessine un disque dur stylisé centré en (cx, cy) de rayon r."""
    draw = ImageDraw.Draw(img)

    # --- Boîtier du disque (rectangle arrondi) ---
    box_w = int(r * 1.5)
    box_h = int(r * 1.8)
    x0 = cx - box_w // 2
    y0 = cy - box_h // 2
    x1 = cx + box_w // 2
    y1 = cy + box_h // 2
    corner = int(r * 0.18)
    draw.rounded_rectangle([x0, y0, x1, y1], radius=corner, fill=BLUE_1, outline=BLUE_2, width=max(1, r // 32))

    # --- Plateau du disque (cercle) ---
    plate_r = int(r * 0.50)
    plate_cy = cy - int(r * 0.15)
    draw.ellipse(
        [cx - plate_r, plate_cy - plate_r, cx + plate_r, plate_cy + plate_r],
        fill=DARK_GRAY, outline=BLUE_3, width=max(1, r // 40)
    )

    # --- Cercles concentriques (pistes) ---
    for ratio in [0.38, 0.26, 0.14]:
        pr = int(r * ratio)
        draw.ellipse(
            [cx - pr, plate_cy - pr, cx + pr, plate_cy + pr],
            outline=(70, 80, 100), width=max(1, r // 64)
        )

    # --- Axe central ---
    axle_r = int(r * 0.06)
    draw.ellipse(
        [cx - axle_r, plate_cy - axle_r, cx + axle_r, plate_cy + axle_r],
        fill=BLUE_3, outline=WHITE, width=max(1, r // 64)
    )

    # --- Bras de lecture ---
    arm_start_x = cx + int(r * 0.45)
    arm_start_y = cy + int(r * 0.55)
    arm_mid_x   = cx + int(r * 0.15)
    arm_mid_y   = plate_cy + int(r * 0.08)
    arm_tip_x   = cx - int(r * 0.10)
    arm_tip_y   = plate_cy - int(r * 0.05)
    arm_w = max(2, r // 20)
    draw.line([(arm_start_x, arm_start_y), (arm_mid_x, arm_mid_y)], fill=GRAY, width=arm_w)
    draw.line([(arm_mid_x, arm_mid_y), (arm_tip_x, arm_tip_y)], fill=WHITE, width=max(1, arm_w // 2))

    # --- Connecteurs bas ---
    conn_y0 = y1 - int(r * 0.22)
    conn_y1 = y1 - int(r * 0.12)
    for dx in [-int(r * 0.30), -int(r * 0.10), int(r * 0.10), int(r * 0.30)]:
        cw = max(2, r // 24)
        draw.rectangle([cx + dx - cw, conn_y0, cx + dx + cw, conn_y1], fill=GOLD)


def draw_lock_badge(img: Image.Image, cx: int, cy: int, r: int):
    """Dessine un petit cadenas en bas à droite de l'icône."""
    draw = ImageDraw.Draw(img)
    lr = int(r * 0.30)
    lx = cx + int(r * 0.50)
    ly = cy + int(r * 0.50)

    # Anse (arc de cercle)
    anse_r = int(lr * 0.50)
    anse_w = max(2, lr // 6)
    draw.arc(
        [lx - anse_r, ly - int(lr * 0.90) - anse_r, lx + anse_r, ly - int(lr * 0.90) + anse_r],
        start=0, end=180, fill=GOLD, width=anse_w
    )
    # Montants de l'anse
    draw.line([(lx - anse_r, ly - int(lr * 0.90)), (lx - anse_r, ly - int(lr * 0.30))], fill=GOLD, width=anse_w)
    draw.line([(lx + anse_r, ly - int(lr * 0.90)), (lx + anse_r, ly - int(lr * 0.30))], fill=GOLD, width=anse_w)

    # Corps du cadenas
    body_x0 = lx - int(lr * 0.60)
    body_y0 = ly - int(lr * 0.35)
    body_x1 = lx + int(lr * 0.60)
    body_y1 = ly + int(lr * 0.55)
    draw.rounded_rectangle([body_x0, body_y0, body_x1, body_y1],
                           radius=max(1, lr // 8), fill=GOLD, outline=(200, 160, 30), width=max(1, lr // 16))

    # Trou de serrure
    key_r = max(1, lr // 8)
    key_cx = lx
    key_cy = ly + int(lr * 0.05)
    draw.ellipse([key_cx - key_r, key_cy - key_r, key_cx + key_r, key_cy + key_r], fill=BG_DARK)


def make_logo(size: int) -> Image.Image:
    """Crée le logo carré à la taille demandée."""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Fond arrondi
    margin = int(size * 0.04)
    corner = int(size * 0.16)
    draw.rounded_rectangle(
        [margin, margin, size - margin, size - margin],
        radius=corner, fill=BG_DARK
    )

    cx, cy = size // 2, size // 2
    r = int(size * 0.34)
    draw_hdd_icon(img, cx, cy, r)
    draw_lock_badge(img, cx, cy, r)

    return img


def make_wizard_large() -> Image.Image:
    """Image latérale 164×314 pour l'installeur Inno Setup."""
    W, H = 164, 314
    img = Image.new('RGB', (W, H), BG_DARK)
    draw = ImageDraw.Draw(img)

    # Dégradé vertical subtil
    for y in range(H):
        t = y / H
        r = int(18 + t * 12)
        g = int(22 + t * 18)
        b = int(36 + t * 30)
        draw.line([(0, y), (W, y)], fill=(r, g, b))

    # Logo centré, petit
    logo = make_logo(120)
    img.paste(logo, ((W - 120) // 2, 40), logo)

    # Texte
    try:
        font_path = os.path.join(RESOURCES, 'Montserrat-Medium.ttf')
        font_sm = ImageFont.truetype(font_path, 11)
        font_lg = ImageFont.truetype(font_path, 13)
    except Exception:
        font_sm = ImageFont.load_default()
        font_lg = font_sm

    draw.text((W // 2, 180), "HDD Password", fill=WHITE, font=font_lg, anchor="mt")
    draw.text((W // 2, 198), "Recovery Tool", fill=BLUE_3, font=font_lg, anchor="mt")
    draw.text((W // 2, 228), "v1.0.0", fill=GRAY, font=font_sm, anchor="mt")

    # Ligne décorative
    draw.line([(20, 250), (W - 20, 250)], fill=BLUE_2, width=1)
    draw.text((W // 2, 264), "Recuperation de", fill=GRAY, font=font_sm, anchor="mt")
    draw.text((W // 2, 280), "mots de passe ATA", fill=GRAY, font=font_sm, anchor="mt")

    return img


def make_wizard_small() -> Image.Image:
    """Petite image 55×58 pour l'en-tête de l'installeur."""
    logo = make_logo(128)
    small = logo.resize((55, 55), Image.LANCZOS)
    # Convertir en RGB sur fond blanc
    bg = Image.new('RGB', (55, 58), (255, 255, 255))
    bg.paste(small, (0, 1), small)
    return bg


def main():
    os.makedirs(RESOURCES, exist_ok=True)

    # --- Logo PNG 256 ---
    logo256 = make_logo(256)
    logo256.save(os.path.join(RESOURCES, 'logo_256.png'), 'PNG')
    print('[OK] logo_256.png')

    # --- ICO multi-taille ---
    sizes = [16, 32, 48, 64, 128, 256]
    icons = [make_logo(s) for s in sizes]
    icons[0].save(
        os.path.join(RESOURCES, 'app.ico'),
        format='ICO',
        sizes=[(s, s) for s in sizes],
        append_images=icons[1:]
    )
    print('[OK] app.ico (multi-size)')

    # --- Wizard large ---
    wl = make_wizard_large()
    wl.save(os.path.join(RESOURCES, 'wizard_large.bmp'), 'BMP')
    print('[OK] wizard_large.bmp (164x314)')

    # --- Wizard small ---
    ws = make_wizard_small()
    ws.save(os.path.join(RESOURCES, 'wizard_small.bmp'), 'BMP')
    print('[OK] wizard_small.bmp (55x58)')

    print('\nTous les logos ont ete generes dans resources/')


if __name__ == '__main__':
    main()
