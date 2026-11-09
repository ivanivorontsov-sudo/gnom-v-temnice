#!/usr/bin/env python3
"""Convert CC0 tilesets into Mega Drive-ready indexed PNGs, font, logo, VGM, WAV."""
from __future__ import annotations

import math
import os
import struct
import wave
from collections import Counter
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SRC_SHEET = Path("/tmp/assets/0x72/sheet.png")
SRC_LIST = Path("/tmp/assets/0x72/tile_list_v1.7")
KENNEY = Path("/tmp/assets/kenney/Tilemap/tilemap_packed.png")
GFX = ROOT / "res" / "gfx"
SPR = ROOT / "res" / "spr"
SFX = ROOT / "res" / "sfx"
BGM = ROOT / "res" / "bgm"
ASSETS = ROOT / "assets"

MAGENTA = (255, 0, 255)


def md_q(v: int) -> int:
    return int(round(v / 255.0 * 7)) * 255 // 7


def md_rgb(rgb):
    return (md_q(rgb[0]), md_q(rgb[1]), md_q(rgb[2]))


def parse_list(path: Path) -> dict:
    frames = {}
    for line in path.read_text().splitlines():
        p = line.split()
        if len(p) < 5:
            continue
        frames[p[0]] = tuple(int(x) for x in p[1:5])
    return frames


def crop(sheet: Image.Image, frames: dict, name: str) -> Image.Image:
    x, y, w, h = frames[name]
    return sheet.crop((x, y, x + w, y + h)).convert("RGBA")


def pad_to(im: Image.Image, tw: int, th: int, bottom: bool = True) -> Image.Image:
    out = Image.new("RGBA", (tw, th), (0, 0, 0, 0))
    src = im
    if src.width > tw or src.height > th:
        left = max(0, (src.width - tw) // 2)
        top = max(0, src.height - th) if bottom else max(0, (src.height - th) // 2)
        src = src.crop((left, top, left + min(tw, src.width), top + min(th, src.height)))
    x = (tw - src.width) // 2
    y = th - src.height if bottom else (th - src.height) // 2
    out.paste(src, (x, max(0, y)), src)
    return out


def strip(imgs, tw, th, bottom=True) -> Image.Image:
    out = Image.new("RGBA", (tw * len(imgs), th), (0, 0, 0, 0))
    for i, im in enumerate(imgs):
        p = pad_to(im, tw, th, bottom)
        out.paste(p, (i * tw, 0), p)
    return out


def collect_colors(im: Image.Image, skip_alpha=True):
    cols = []
    for r, g, b, a in im.getdata():
        if skip_alpha and a < 128:
            continue
        cols.append(md_rgb((r, g, b)))
    return cols


def build_palette(colors, n=15, extra=None) -> list:
    cnt = Counter(colors)
    if extra:
        for c in extra:
            cnt[md_rgb(c)] += 50
    ordered = [c for c, _ in cnt.most_common(n)]
    # ensure contrast anchors
    for c in extra or []:
        c = md_rgb(c)
        if c not in ordered and len(ordered) < n:
            ordered.append(c)
    while len(ordered) < n:
        ordered.append((0, 0, 0))
    return ordered[:n]


def nearest(rgb, pal):
    r, g, b = rgb
    best, bd = 0, 10**9
    for i, (pr, pg, pb) in enumerate(pal):
        d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
        if d < bd:
            bd, best = d, i
    return best


def to_indexed(im: Image.Image, pal16: list, path: Path, transparent=True):
    """pal16[0] is transparent (magenta) if transparent else first color."""
    path.parent.mkdir(parents=True, exist_ok=True)
    w, h = im.size
    out = Image.new("P", (w, h))
    raw = []
    for c in pal16:
        raw.extend(c)
    raw.extend([0] * (768 - len(raw)))
    out.putpalette(raw)
    pix = []
    data = im.convert("RGBA").getdata()
    body = pal16[1:] if transparent else pal16
    for r, g, b, a in data:
        if transparent and a < 128:
            pix.append(0)
        else:
            idx = nearest(md_rgb((r, g, b)), body if transparent else pal16)
            pix.append((idx + 1) if transparent else idx)
    out.putdata(pix)
    # SGDK likes no max compatibility issues — save without optimization extras
    out.save(path)


def tint(im: Image.Image, rgb, amount=0.35) -> Image.Image:
    out = im.copy()
    px = out.load()
    tr, tg, tb = rgb
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            if a < 128:
                continue
            px[x, y] = (
                int(r * (1 - amount) + tr * amount),
                int(g * (1 - amount) + tg * amount),
                int(b * (1 - amount) + tb * amount),
                a,
            )
    return out


def gen_key() -> Image.Image:
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    p = im.load()
    gold, dark, shine = (232, 196, 64), (140, 92, 16), (255, 240, 160)

    def dot(x, y, c):
        if 0 <= x < 16 and 0 <= y < 16:
            p[x, y] = (*c, 255)

    for y, x0, x1 in ((3, 5, 10), (4, 4, 11), (5, 4, 11), (6, 5, 10)):
        for x in range(x0, x1):
            dot(x, y, gold)
    for y in range(6, 14):
        dot(7, y, gold)
        dot(8, y, gold)
    for x in range(8, 13):
        dot(x, 10, gold)
        dot(x, 12, gold)
    dot(5, 4, shine)
    dot(9, 4, dark)
    return im


def gen_meat() -> Image.Image:
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    p = im.load()
    pink, dark, bone = (196, 72, 64), (120, 32, 28), (232, 220, 180)
    for y in range(5, 13):
        for x in range(3, 13):
            if (x - 8) ** 2 / 25 + (y - 9) ** 2 / 16 <= 1:
                p[x, y] = (*pink, 255)
    for x in range(4, 8):
        p[x, 4] = (*bone, 255)
        p[x, 5] = (*bone, 255)
    p[6, 7] = (*dark, 255)
    return im


def gen_slash_frames():
    frames = []
    for i, (spread, thick) in enumerate(((4, 1), (7, 2), (5, 1))):
        im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        col = [(255, 240, 180, 255), (255, 220, 80, 255), (255, 255, 255, 220)][i]
        d.arc((2, 1, 14, 15), start=250, end=250 + spread * 12, fill=col, width=thick + 1)
        frames.append(im)
    return frames


def gen_orb(color) -> Image.Image:
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((4, 4, 12, 12), fill=(*color, 255))
    d.ellipse((6, 6, 9, 9), fill=(255, 255, 255, 230))
    return im


def find_font():
    candidates = [
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ]
    for p in candidates:
        if os.path.exists(p):
            return p
    raise SystemExit("no TTF font with Cyrillic")


def glyph8(ch: str, font_path: str) -> Image.Image:
    font = ImageFont.truetype(font_path, 56)
    canvas = Image.new("L", (80, 80), 0)
    dr = ImageDraw.Draw(canvas)
    dr.text((8, 0), ch, font=font, fill=255)
    bbox = canvas.getbbox()
    if not bbox:
        return Image.new("L", (8, 8), 0)
    g = canvas.crop(bbox)
    g = g.resize((7, 7), Image.Resampling.BILINEAR)
    out = Image.new("L", (8, 8), 0)
    out.paste(g, (0, 1))
    return out.point(lambda p: 255 if p > 90 else 0)


CHARSET = (
    " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"
    ".,:!?-+/()*"
)
# icons appended after charset by the converter (hearts etc.)


def build_font(pal3, font_path: str):
    icons = []  # built later
    return CHARSET, icons, font_path


def main():
    GFX.mkdir(parents=True, exist_ok=True)
    SPR.mkdir(parents=True, exist_ok=True)
    SFX.mkdir(parents=True, exist_ok=True)
    BGM.mkdir(parents=True, exist_ok=True)
    ASSETS.mkdir(parents=True, exist_ok=True)

    sheet = Image.open(SRC_SHEET).convert("RGBA")
    frames = parse_list(SRC_LIST)
    font_path = find_font()

    def fr(name):
        return crop(sheet, frames, name)

    # ---- metatiles 16x16 stacked vertically ----
    floor = [fr(f"floor_{i}") for i in range(1, 9)]
    wall = fr("wall_mid")
    wall_top = fr("wall_top_mid")
    wall_side = fr("wall_left")
    stairs = fr("floor_stairs")
    ladder = fr("floor_ladder")
    skull = fr("skull")
    banner_r = fr("wall_banner_red")
    banner_g = fr("wall_banner_green")
    spikes = fr("floor_spikes_anim_f0")
    door_c = pad_to(fr("doors_leaf_closed"), 16, 16, True)
    door_o = pad_to(fr("doors_leaf_open"), 16, 16, True)
    door_l = door_c.copy()
    # gold lock overlay
    d = ImageDraw.Draw(door_l)
    d.rectangle((5, 7, 10, 12), fill=(220, 180, 40, 255), outline=(90, 60, 10, 255))
    ped = pad_to(fr("crate"), 16, 16, True)
    boss_floor = tint(floor[0], (160, 30, 30), 0.4)
    carpet = tint(floor[2], (70, 50, 160), 0.4)
    void = Image.new("RGBA", (16, 16), (12, 8, 16, 255))
    # Kenney extra floor variation if needed
    if KENNEY.exists():
        km = Image.open(KENNEY).convert("RGBA")
        # not required — 0x72 is enough

    names = [
        ("floor1", floor[0]),
        ("floor2", floor[1]),
        ("floor3", floor[2]),
        ("floor4", floor[3]),
        ("floor5", floor[4]),
        ("wall", wall),
        ("wall_top", wall_top),
        ("wall_side", wall_side),
        ("door_c", door_c),
        ("door_o", door_o),
        ("door_l", door_l),
        ("stairs", stairs),
        ("ladder", ladder),
        ("pedestal", ped),
        ("skull", skull),
        ("banner_r", banner_r),
        ("banner_g", banner_g),
        ("boss_fl", boss_floor),
        ("carpet", carpet),
        ("void", void),
        ("spikes", spikes),
        ("floor6", floor[5]),
    ]
    dungeon_rgba = Image.new("RGBA", (16, 16 * len(names)), (0, 0, 0, 0))
    for i, (_, im) in enumerate(names):
        dungeon_rgba.paste(im.convert("RGBA"), (0, i * 16))

    pal0_body = build_palette(
        collect_colors(dungeon_rgba),
        16,
        extra=[(20, 14, 18), (48, 36, 32), (90, 72, 52), (160, 130, 90), (200, 40, 40)],
    )
    pal0 = pal0_body[:16]
    while len(pal0) < 16:
        pal0.append((0, 0, 0))
    to_indexed(dungeon_rgba, pal0, GFX / "dungeon.png", transparent=False)

    # ---- sprites ----
    gnome = [fr(f"dwarf_m_idle_anim_f{i}") for i in range(4)]
    gnome += [fr(f"dwarf_m_run_anim_f{i}") for i in range(4)]
    gnome += [fr("dwarf_m_hit_anim_f0")]
    # attack: idle + axe composite
    axe = fr("weapon_double_axe")
    for i in range(3):
        base = pad_to(gnome[i % 4], 16, 32, True)
        ax = pad_to(axe, 16, 24, True)
        comp = base.copy()
        ox = [2, 4, 1][i]
        oy = [4, 2, 6][i]
        tmp = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
        tmp.paste(ax, (ox, oy), ax)
        comp = Image.alpha_composite(comp, tmp)
        gnome.append(comp)

    gob = [fr(f"goblin_idle_anim_f{i}") for i in range(4)]
    gob += [fr(f"goblin_run_anim_f{i}") for i in range(4)]
    orc = [fr(f"masked_orc_idle_anim_f{i}") for i in range(4)]
    orc += [fr(f"masked_orc_run_anim_f{i}") for i in range(4)]
    shaman = [fr(f"orc_shaman_idle_anim_f{i}") for i in range(4)]
    shaman += [fr(f"orc_shaman_run_anim_f{i}") for i in range(4)]
    boss = [fr(f"ogre_idle_anim_f{i}") for i in range(4)]
    boss += [fr(f"ogre_run_anim_f{i}") for i in range(4)]

    # items sheet 16x16: coin0-3, heart, heart_half, key, gem, meat, bomb0-2, flasks, chest
    coins = [pad_to(fr(f"coin_anim_f{i}"), 16, 16, False) for i in range(4)]
    heart = pad_to(fr("ui_heart_full"), 16, 16, False)
    hearth = pad_to(fr("ui_heart_half"), 16, 16, False)
    heart_e = pad_to(fr("ui_heart_empty"), 16, 16, False)
    key = gen_key()
    gem = pad_to(fr("flask_yellow"), 16, 16, True)
    meat = gen_meat()
    bombs = [pad_to(fr(f"bomb_f{i}"), 16, 16, True) for i in range(3)]
    flasks = [
        pad_to(fr("flask_red"), 16, 16, True),
        pad_to(fr("flask_blue"), 16, 16, True),
        pad_to(fr("flask_green"), 16, 16, True),
        pad_to(fr("flask_big_yellow"), 16, 16, True),
        pad_to(fr("flask_big_red"), 16, 16, True),
        pad_to(fr("chest_full_open_anim_f0"), 16, 16, True),
    ]
    items = coins + [heart, hearth, heart_e, key, gem, meat] + bombs + flasks

    slash = gen_slash_frames()
    arrow = pad_to(fr("weapon_arrow"), 16, 16, True)
    magic = gen_orb((80, 140, 255))
    fire = gen_orb((255, 80, 40))
    shots = [arrow, magic, fire]

    gnome_s = strip(gnome, 16, 32, True)
    gob_s = strip(gob, 16, 16, True)
    orc_s = strip(orc, 16, 32, True)
    sha_s = strip(shaman, 16, 32, True)
    bos_s = strip(boss, 32, 32, True)
    it_s = strip(items, 16, 16, False)
    sl_s = strip(slash, 16, 16, False)
    sh_s = strip(shots, 16, 16, False)
    bm_s = strip(bombs, 16, 16, True)

    pal1_cols = []
    for im in (gnome_s, it_s, sl_s, bm_s, heart, key):
        pal1_cols += collect_colors(im)
    pal1_body = build_palette(
        pal1_cols,
        15,
        extra=[
            (40, 24, 16),
            (180, 120, 48),
            (80, 140, 72),
            (220, 48, 48),
            (240, 220, 80),
            (255, 255, 255),
        ],
    )
    pal1 = [MAGENTA] + pal1_body

    pal2_cols = []
    for im in (gob_s, orc_s, sha_s, bos_s, sh_s):
        pal2_cols += collect_colors(im)
    pal2_body = build_palette(
        pal2_cols,
        15,
        extra=[(20, 40, 16), (80, 140, 40), (160, 40, 32), (40, 80, 160), (240, 220, 80)],
    )
    pal2 = [MAGENTA] + pal2_body

    to_indexed(gnome_s, pal1, SPR / "gnome.png")
    to_indexed(gob_s, pal2, SPR / "goblin.png")
    to_indexed(orc_s, pal2, SPR / "orc.png")
    to_indexed(sha_s, pal2, SPR / "shaman.png")
    to_indexed(bos_s, pal2, SPR / "boss.png")
    to_indexed(it_s, pal1, SPR / "items.png")
    to_indexed(sl_s, pal1, SPR / "slash.png")
    to_indexed(sh_s, pal2, SPR / "shot.png")
    to_indexed(bm_s, pal1, SPR / "bomb.png")

    # ---- font + HUD icons (8x8) ----
    pal3 = [MAGENTA] + [
        (0, 0, 0),
        (255, 255, 255),
        (232, 204, 96),
        (180, 120, 32),
        (220, 48, 48),
        (80, 200, 80),
        (80, 160, 255),
        (48, 36, 32),
        (160, 160, 160),
        (255, 140, 48),
        (96, 32, 32),
        (240, 230, 200),
        (32, 80, 48),
        (64, 48, 96),
        (16, 12, 20),
    ]
    glyphs = [glyph8(ch, font_path) for ch in CHARSET]

    def icon_from(im, size=8):
        g = im.convert("RGBA").resize((size, size), Image.Resampling.NEAREST)
        return g

    extra_icons = [
        icon_from(heart),
        icon_from(hearth),
        icon_from(heart_e),
        icon_from(key),
        icon_from(fr("coin_anim_f0")),
        icon_from(fr("bomb_f0")),
    ]
    # font sheet: all 8x8 in one row
    n = len(glyphs) + len(extra_icons)
    font_rgba = Image.new("RGBA", (8 * n, 8), (0, 0, 0, 0))
    white = (255, 255, 255, 255)
    for i, g in enumerate(glyphs):
        rgba = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
        rp = rgba.load()
        gp = g.load()
        for y in range(8):
            for x in range(8):
                if gp[x, y] > 0:
                    rp[x, y] = white
        font_rgba.paste(rgba, (i * 8, 0), rgba)
    for j, ic in enumerate(extra_icons):
        font_rgba.paste(ic, ((len(glyphs) + j) * 8, 0), ic.convert("RGBA"))
    to_indexed(font_rgba, pal3, GFX / "font.png")

    # ---- logo ----
    title1 = "ГНОМ"
    title2 = "В ТЕМНИЦЕ"
    # 16x16 letters (2x glyphs)
    def blit_word(word):
        imgs = []
        for ch in word:
            g = glyph8(ch, font_path).resize((16, 16), Image.NEAREST)
            imgs.append(g)
        w = 16 * len(imgs)
        im = Image.new("L", (w, 16), 0)
        for i, g in enumerate(imgs):
            im.paste(g, (i * 16, 0))
        return im

    l1 = blit_word(title1)
    l2 = blit_word(title2)
    logo_w, logo_h = 256, 56
    logo = Image.new("RGBA", (logo_w, logo_h), (0, 0, 0, 0))

    def paste_text(mask, y, fill, outline):
        x0 = (logo_w - mask.width) // 2
        mp = mask.load()
        lp = logo.load()
        # outline
        for y0 in range(mask.height):
            for x0i in range(mask.width):
                if mp[x0i, y0] < 128:
                    continue
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        xx, yy = x0 + x0i + dx, y + y0 + dy
                        if 0 <= xx < logo_w and 0 <= yy < logo_h:
                            if dx == 0 and dy == 0:
                                continue
                            if lp[xx, yy][3] == 0:
                                lp[xx, yy] = (*outline, 255)
        for y0 in range(mask.height):
            for x0i in range(mask.width):
                if mp[x0i, y0] >= 128:
                    lp[x0 + x0i, y + y0] = (*fill, 255)

    paste_text(l1, 6, (232, 204, 96), (72, 40, 16))
    paste_text(l2, 30, (240, 230, 200), (48, 28, 16))
    to_indexed(logo, pal3, GFX / "logo.png")

    # charset header
    (ROOT / "src").mkdir(exist_ok=True)
    header = []
    header.append("#ifndef FONTMAP_H")
    header.append("#define FONTMAP_H")
    header.append(f"#define FONT_COUNT {len(glyphs)}")
    header.append(f"#define FONT_TOTAL {n}")
    header.append(f"#define ICON_HEART_F {len(glyphs) + 0}")
    header.append(f"#define ICON_HEART_H {len(glyphs) + 1}")
    header.append(f"#define ICON_HEART_E {len(glyphs) + 2}")
    header.append(f"#define ICON_KEY {len(glyphs) + 3}")
    header.append(f"#define ICON_COIN {len(glyphs) + 4}")
    header.append(f"#define ICON_BOMB {len(glyphs) + 5}")
    header.append(f"#define MT_COUNT {len(names)}")
    for i, (nm, _) in enumerate(names):
        header.append(f"#define MT_{nm.upper()} {i}")
    header.append("/* charset order stored as UTF-8 in font_chars */")
    header.append("#endif")
    (ROOT / "src" / "fontmap.h").write_text("\n".join(header) + "\n", encoding="utf-8")
    (ROOT / "src" / "font_chars.txt").write_text(CHARSET, encoding="utf-8")

    # ---- audio ----
    write_vgm(BGM / "dungeon.vgm")
    write_vgm(BGM / "title.vgm", brighter=True)
    make_sfx(SFX)

    print("OK tiles", len(names), "font", n, "gnome frames", len(gnome))
    print("wrote", GFX, SPR, BGM, SFX)


def psg_tone(ch, period):
    period = max(1, min(1023, period))
    latch = 0x80 | (ch << 5) | (period & 0x0F)
    data = (period >> 4) & 0x3F
    return bytes((0x50, latch, 0x50, data))


def psg_vol(ch, att):
    att = max(0, min(15, att))
    return bytes((0x50, 0x90 | (ch << 5) | att))


def note_period(freq):
    if freq <= 0:
        return 0
    return int(3579545 / (32 * freq) + 0.5)


# A minor dungeon / brighter title
SCALE = {
    "R": 0,
    "A2": 110.00,
    "B2": 123.47,
    "C3": 130.81,
    "D3": 146.83,
    "E3": 164.81,
    "F3": 174.61,
    "G3": 196.00,
    "A3": 220.00,
    "B3": 246.94,
    "C4": 261.63,
    "D4": 293.66,
    "E4": 329.63,
    "F4": 349.23,
    "G4": 392.00,
    "A4": 440.00,
    "B4": 493.88,
    "C5": 523.25,
    "E5": 659.25,
}


def write_vgm(path: Path, brighter=False):
    data = bytearray()
    # YM2612 dummy key-off so converters treat this as MD VGM
    for reg in (0x28,):
        data += bytes((0x52, reg, 0x00))

    if brighter:
        bass = ["A2", "C3", "E3", "A3", "G3", "E3", "C3", "E3"] * 4
        lead = [
            "A4", "C5", "B4", "A4", "G4", "A4", "E4", "R",
            "F4", "A4", "G4", "E4", "D4", "E4", "C4", "R",
            "A4", "E5", "C5", "A4", "G4", "F4", "E4", "D4",
            "C4", "E4", "A4", "G4", "E4", "C4", "A3", "R",
        ]
    else:
        bass = ["A2", "A2", "E3", "A2", "F3", "F3", "E3", "C3"] * 4
        lead = [
            "A3", "C4", "E4", "A4", "G4", "E4", "C4", "E4",
            "F4", "A4", "E4", "C4", "D4", "E4", "A3", "R",
            "A3", "E4", "G4", "A4", "F4", "D4", "E4", "C4",
            "B3", "C4", "D4", "E4", "C4", "A3", "A3", "R",
        ]

    frames_per_step = 8
    total_steps = max(len(bass), len(lead))
    samples = 0
    for i in range(total_steps):
        bn = SCALE[bass[i % len(bass)]]
        ln = SCALE[lead[i % len(lead)]]
        if bn:
            data += psg_tone(0, note_period(bn))
            data += psg_vol(0, 2)
        else:
            data += psg_vol(0, 15)
        if ln:
            data += psg_tone(1, note_period(ln))
            data += psg_vol(1, 1)
        else:
            data += psg_vol(1, 15)
        # soft hat every other step
        if i % 2 == 0:
            data += bytes((0x50, 0xE4))  # noise
            data += psg_vol(3, 8)
        else:
            data += psg_vol(3, 15)
        for _ in range(frames_per_step):
            data += b"\x62"
            samples += 735
            # decay noise quickly
            if _ == 2:
                data += psg_vol(3, 15)
    data += psg_vol(0, 15)
    data += psg_vol(1, 15)
    data += psg_vol(3, 15)
    data += b"\x66"

    header = bytearray(0x40)
    header[0:4] = b"Vgm "
    header[0x08:0x0C] = (0x00000151).to_bytes(4, "little")
    header[0x0C:0x10] = (3579545).to_bytes(4, "little")
    header[0x18:0x1C] = samples.to_bytes(4, "little")
    header[0x24:0x28] = (60).to_bytes(4, "little")
    header[0x28] = 0x09
    header[0x2A] = 16
    header[0x2C:0x30] = (7670454).to_bytes(4, "little")
    header[0x34:0x38] = (0x0C).to_bytes(4, "little")  # data at 0x40
    raw = header + data
    raw[0x04:0x08] = (len(raw) - 4).to_bytes(4, "little")
    path.write_bytes(raw)


def synth_tone(freq, ms, sr, vol=0.4, kind="sq", decay=True):
    n = int(sr * ms / 1000)
    out = []
    for i in range(n):
        t = i / sr
        env = (1.0 - i / n) if decay else 1.0
        if kind == "sq":
            s = 1.0 if math.sin(2 * math.pi * freq * t) >= 0 else -1.0
        elif kind == "tri":
            s = 2.0 * abs(2.0 * ((t * freq) % 1.0) - 1.0) - 1.0
        else:
            s = (hash((i, freq)) % 1000) / 500.0 - 1.0
        out.append(int(max(-1, min(1, s * env * vol)) * 32767))
    return out


def write_wav(path: Path, samples, sr=13300):
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(b"".join(struct.pack("<h", s) for s in samples))


def make_sfx(folder: Path):
    sr = 13300
    write_wav(folder / "hit.wav", synth_tone(180, 90, sr, 0.5, "noi") + synth_tone(90, 60, sr, 0.35, "sq"))
    write_wav(folder / "swing.wav", synth_tone(400, 40, sr, 0.25, "noi") + synth_tone(220, 50, sr, 0.2, "tri"))
    ping = []
    for f in (523, 659, 784):
        ping += synth_tone(f, 50, sr, 0.35, "sq", True)
    write_wav(folder / "pickup.wav", ping)
    expl = synth_tone(80, 180, sr, 0.55, "noi") + synth_tone(50, 160, sr, 0.4, "sq")
    write_wav(folder / "explode.wav", expl)
    hurt = []
    for f in (330, 220, 147):
        hurt += synth_tone(f, 70, sr, 0.4, "sq")
    write_wav(folder / "hurt.wav", hurt)
    write_wav(folder / "door.wav", synth_tone(140, 80, sr, 0.4, "sq") + synth_tone(90, 80, sr, 0.3, "sq"))
    write_wav(folder / "select.wav", synth_tone(660, 60, sr, 0.3, "sq"))
    shim = []
    for f in (392, 523, 659, 784, 1046):
        shim += synth_tone(f, 45, sr, 0.28, "tri")
    write_wav(folder / "power.wav", shim)


if __name__ == "__main__":
    main()
