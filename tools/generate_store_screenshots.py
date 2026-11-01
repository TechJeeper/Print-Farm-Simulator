#!/usr/bin/env python3
"""Render Rebble App Store screenshots at exact platform resolutions.

Mirrors Print Farm Simulator watch UI (no watch bezel — store requirement).
"""
from __future__ import annotations

import os
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "store" / "screenshots"
RES = ROOT / "resources" / "images"

PLATFORMS = {
    "basalt": (144, 168, True),   # Pebble Time — color
    "diorite": (144, 168, False),  # Pebble 2 — B&W
    "flint": (144, 168, False),    # Pebble 2 Duo — B&W
    "emery": (200, 228, True),    # Pebble Time 2 — color
}

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
LIGHT_GRAY = (170, 170, 170)
DARK_GRAY = (85, 85, 85)
CYAN = (0, 255, 255)
BRIGHT_GREEN = (0, 255, 0)
RED = (255, 0, 0)
YELLOW = (255, 170, 0)
CERULEAN = (0, 170, 255)


def font(size: int) -> ImageFont.ImageFont:
    candidates = [
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ]
    for path in candidates:
        if os.path.isfile(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def font_bold(size: int) -> ImageFont.ImageFont:
    candidates = [
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    ]
    for path in candidates:
        if os.path.isfile(path):
            return ImageFont.truetype(path, size)
    return font(size)


def text_center(draw, text, y, w, fnt, fill):
    bbox = draw.textbbox((0, 0), text, font=fnt)
    tw = bbox[2] - bbox[0]
    draw.text(((w - tw) // 2, y), text, font=fnt, fill=fill)


def bw_sprite(spr: Image.Image) -> Image.Image:
    spr = spr.copy()
    px = spr.load()
    for y in range(spr.height):
        for x in range(spr.width):
            r, g, b, a = px[x, y]
            if a < 32:
                continue
            lum = (r + g + b) // 3
            if lum < 20:
                px[x, y] = (0, 0, 0, 0)
            else:
                px[x, y] = (255, 255, 255, a)
    return spr


def paste_sprite(base: Image.Image, path: Path, origin: tuple[int, int], color: bool,
                 centered: bool = False):
    if not path.is_file():
        return
    spr = Image.open(path).convert("RGBA")
    if not color:
        spr = bw_sprite(spr)
    x, y = origin
    if centered:
        x -= spr.width // 2
        y -= spr.height // 2
    base.paste(spr, (x, y), spr)


def draw_bar(draw, x, y, w, h, pct, color: bool):
    bg = DARK_GRAY if color else BLACK
    fg = BRIGHT_GREEN if color else WHITE
    outline = LIGHT_GRAY if color else WHITE
    draw.rectangle((x, y, x + w, y + h), fill=bg)
    fill_w = max(0, min(w, int(w * pct / 100)))
    if fill_w > 0:
        draw.rectangle((x, y, x + fill_w, y + h), fill=fg)
    draw.rectangle((x, y, x + w, y + h), outline=outline)


def render_title(w: int, h: int, color: bool) -> Image.Image:
    img = Image.new("RGB", (w, h), BLACK)
    draw = ImageDraw.Draw(img)
    f18 = font_bold(max(14, w // 10))
    f14 = font(max(11, w // 13))
    dim = LIGHT_GRAY if color else WHITE

    title = RES / "title.png"
    if title.is_file():
        spr = Image.open(title).convert("RGBA")
        tw = min(spr.width, w - 4)
        th = spr.height
        if spr.width > tw:
            ratio = tw / spr.width
            th = max(1, int(spr.height * ratio))
            spr = spr.resize((tw, th), Image.NEAREST)
        if not color:
            spr = bw_sprite(spr)
        x = (w - spr.width) // 2
        img.paste(spr, (x, 10), spr)
    else:
        text_center(draw, "PRINT FARM", 24, w, font_bold(max(18, w // 8)), WHITE)

    text_center(draw, "Manage printers", h // 2, w, f14, dim)
    text_center(draw, "from your wrist", h // 2 + 16, w, f14, dim)
    text_center(draw, "SELECT to open", h - 36, w, f18, WHITE)
    return img


def render_farm(w: int, h: int, color: bool) -> Image.Image:
    img = Image.new("RGB", (w, h), BLACK)
    draw = ImageDraw.Draw(img)
    f14b = font_bold(max(11, w // 13))
    f14 = font(max(11, w // 13))
    f18 = font_bold(max(14, w // 10))
    dim = LIGHT_GRAY if color else WHITE
    accent = CYAN if color else WHITE
    ok = BRIGHT_GREEN if color else WHITE
    bad = RED if color else WHITE
    warn = YELLOW if color else WHITE
    panel = DARK_GRAY if color else BLACK

    draw.text((2, 0), "PRINT FARM", font=f14b, fill=WHITE)
    money = "$1.2k"
    bbox = draw.textbbox((0, 0), money, font=f14b)
    draw.text((w - 2 - (bbox[2] - bbox[0]), 0), money, font=f14b, fill=WHITE)
    draw.text((2, 16), "Rep 240 · Staff 1", font=f14, fill=dim)

    # Match watch UI: 38px two-line rows (sprite + status / bar)
    rows = [
        ("printer_basic.png", "P1  run", 62, ok, True, None),
        ("printer_corexy.png", "P2  JAM", 38, bad, True, "status_jam.png"),
        ("printer_basic.png", "P3  idle", 0, dim, False, "status_ok.png"),
    ]
    row_h = 38
    list_top = 34
    visible = max(1, (h - 20 - list_top) // row_h)
    for i, (sprite, label, pct, col, show_bar, status) in enumerate(rows[:visible]):
        y = list_top + i * row_h
        focused = i == 1
        if focused:
            if color:
                draw.rectangle((0, y, w, y + row_h - 2), fill=panel)
            else:
                draw.rectangle((0, y, w - 1, y + row_h - 3), outline=WHITE)
        paste_sprite(img, RES / sprite, (2, y + 2), color)
        draw.text((38, y + 1), label, font=f18, fill=col)
        if status:
            paste_sprite(img, RES / status, (w - 18, y + 2), color)
        if show_bar:
            draw_bar(draw, 38, y + 22, w - 42, 10, pct, color)
        else:
            draw.text((38, y + 20), "PLA", font=f14, fill=dim)
            paste_sprite(img, RES / "spool_pla.png", (w - 18, y + 18), color)

    if len(rows) < visible:
        shop_y = list_top + len(rows) * row_h
        draw.text((8, shop_y + 8), "  SHOP", font=f18, fill=accent)
    text_center(draw, "UP/DN  SEL open", h - 18, w, f14, dim)
    return img


def render_jam(w: int, h: int, color: bool) -> Image.Image:
    img = Image.new("RGB", (w, h), BLACK)
    draw = ImageDraw.Draw(img)
    f14b = font_bold(max(11, w // 13))
    f14 = font(max(11, w // 13))
    bad = RED if color else WHITE
    dim = LIGHT_GRAY if color else WHITE

    draw.text((2, 0), "PRINTER 2  CoreXY", font=f14b, fill=WHITE)
    paste_sprite(img, RES / "printer_corexy.png", (w // 2, 40), color, centered=True)
    paste_sprite(img, RES / "fail_spaghetti.png", (w // 2 + 18, 48), color, centered=True)
    text_center(draw, "Dragon  need PETG", 60, w, f14, bad)
    draw_bar(draw, 10, 78, w - 20, 10, 38, color)
    text_center(draw, "SEL mash cut  DN scrap", 94, w, f14, WHITE)
    text_center(draw, "Cut 3/5", h - 18, w, f14, dim)
    return img


def render_filament(w: int, h: int, color: bool) -> Image.Image:
    img = Image.new("RGB", (w, h), BLACK)
    draw = ImageDraw.Draw(img)
    f14b = font_bold(max(11, w // 13))
    f14 = font(max(11, w // 13))
    warn = YELLOW if color else WHITE
    dim = LIGHT_GRAY if color else WHITE

    draw.text((2, 0), "PRINTER 1  Ender", font=f14b, fill=WHITE)
    paste_sprite(img, RES / "spool_petg.png", (w - 20, 2), color)
    paste_sprite(img, RES / "printer_basic.png", (w // 2, 40), color, centered=True)
    paste_sprite(img, RES / "fail_empty.png", (w // 2 + 18, 48), color, centered=True)
    text_center(draw, "Benchy  need PETG", 60, w, f14, warn)
    draw_bar(draw, 10, 78, w - 20, 10, 55, color)
    text_center(draw, "Pick PETG  SEL load", 94, w, f14, WHITE)
    text_center(draw, "Filament out!", h - 18, w, f14, dim)
    return img


def render_shop(w: int, h: int, color: bool) -> Image.Image:
    img = Image.new("RGB", (w, h), BLACK)
    draw = ImageDraw.Draw(img)
    f18 = font_bold(max(14, w // 10))
    f14b = font_bold(max(11, w // 13))
    f14 = font(max(11, w // 13))
    dim = LIGHT_GRAY if color else WHITE
    accent = CYAN if color else WHITE
    panel = DARK_GRAY if color else BLACK

    draw.text((2, 0), "SHOP", font=f18, fill=WHITE)
    money = "$820"
    bbox = draw.textbbox((0, 0), money, font=f14b)
    draw.text((w - 2 - (bbox[2] - bbox[0]), 2), money, font=f14b, fill=WHITE)

    items = [
        ("Buy printer", "3/6  $1450", True),
        ("Upgrade tier", "+speed  $600", False),
        ("Hire staff", "Lv1  $1200", False),
        ("Unlock exotic", "glow  $1200", False),
    ]
    for i, (label, detail, focused) in enumerate(items):
        y = 24 + i * 28
        if focused:
            draw.rectangle((0, y, w, y + 26), fill=panel)
        draw.text((4, y), label, font=f14b, fill=accent if focused else WHITE)
        draw.text((4, y + 12), detail, font=f14, fill=dim)

    text_center(draw, "SEL buy  BACK", h - 18, w, f14, dim)
    return img


SCENES = [
    ("01-title.png", render_title),
    ("02-farm.png", render_farm),
    ("03-jam.png", render_jam),
    ("04-filament.png", render_filament),
    ("05-shop.png", render_shop),
]


def main() -> None:
    for plat, (w, h, color) in PLATFORMS.items():
        dest = OUT / plat
        dest.mkdir(parents=True, exist_ok=True)
        for filename, fn in SCENES:
            img = fn(w, h, color)
            path = dest / filename
            img.save(path, "PNG", optimize=True)
            print(f"wrote {path.relative_to(ROOT)} ({w}x{h})")
    print("Done:", OUT)


if __name__ == "__main__":
    main()
