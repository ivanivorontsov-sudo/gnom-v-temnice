#!/usr/bin/env python3
"""Original 16x32 gnome frames and clearer dungeon door tiles (Mega Drive indexed)."""
from __future__ import annotations

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SPR = ROOT / "res" / "spr"
GFX = ROOT / "res" / "gfx"

# Must match existing palPlayer in gnome.png so items/slash/bomb on PAL1 stay correct.
PAL1 = [
    (255, 0, 255),  # 0 magenta
    (36, 36, 36),   # 1 ink
    (218, 72, 72),  # 2 hat red
    (218, 182, 109),# 3 skin
    (72, 72, 72),   # 4 metal
    (255, 255, 255),# 5 white
    (182, 109, 36),  # 6 tunic brown
    (255, 218, 72),  # 7 gold
    (218, 109, 36),  # 8 nose
    (109, 218, 218), # 9 cyan steel
    (182, 145, 109), # 10 beard shade
    (36, 36, 72),    # 11 navy
    (145, 72, 36),   # 12 boots
    (218, 182, 72),  # 13 old gold
    (255, 145, 36),  # 14 hat highlight
    (182, 72, 72),   # 15 hat shadow
]

INK, RED, SKIN, METAL, WHITE = 1, 2, 3, 4, 5
TUNIC, GOLD, NOSE, STEEL, SHADE = 6, 7, 8, 9, 10
BOOT, HAT_HI, HAT_LO = 12, 14, 15


def save_indexed(im_idx: Image.Image, pal: list, path: Path):
    raw = []
    for c in pal:
        raw.extend(c)
    raw.extend([0] * (768 - len(raw)))
    out = Image.new("P", im_idx.size)
    out.putpalette(raw)
    out.putdata(list(im_idx.getdata()))
    path.parent.mkdir(parents=True, exist_ok=True)
    out.save(path)


def blank():
    return [[0] * 16 for _ in range(32)]


def p(px, x, y, c):
    if 0 <= x < 16 and 0 <= y < 32 and c:
        px[y][x] = c


def row(px, y, x0, x1, c):
    for x in range(x0, x1 + 1):
        p(px, x, y, c)


def outline_if_empty(px, x, y):
    if 0 <= x < 16 and 0 <= y < 32 and px[y][x] == 0:
        px[y][x] = INK


def add_outline(px):
    """Add a 1px ink outline around opaque pixels."""
    extra = []
    for y in range(32):
        for x in range(16):
            if px[y][x] == 0:
                continue
            if px[y][x] == INK:
                continue
            for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < 16 and 0 <= ny < 32 and px[ny][nx] == 0:
                    extra.append((nx, ny))
    for x, y in extra:
        px[y][x] = INK


def draw_hat(px, ox=0, oy=0, bob=0):
    # Tall pointy red hat, classic gnome silhouette.
    y0 = 1 + oy + bob
    # tip
    p(px, 7 + ox, y0, RED)
    row(px, y0 + 1, 6 + ox, 8 + ox, RED)
    p(px, 7 + ox, y0 + 1, HAT_HI)
    row(px, y0 + 2, 5 + ox, 8 + ox, RED)
    p(px, 6 + ox, y0 + 2, HAT_HI)
    row(px, y0 + 3, 5 + ox, 9 + ox, RED)
    p(px, 6 + ox, y0 + 3, HAT_HI)
    row(px, y0 + 4, 4 + ox, 9 + ox, RED)
    p(px, 5 + ox, y0 + 4, HAT_HI)
    row(px, y0 + 5, 4 + ox, 10 + ox, RED)
    p(px, 5 + ox, y0 + 5, HAT_HI)
    p(px, 10 + ox, y0 + 5, HAT_LO)
    row(px, y0 + 6, 3 + ox, 10 + ox, RED)
    p(px, 4 + ox, y0 + 6, HAT_HI)
    p(px, 10 + ox, y0 + 6, HAT_LO)
    row(px, y0 + 7, 3 + ox, 11 + ox, RED)
    p(px, 4 + ox, y0 + 7, HAT_HI)
    p(px, 11 + ox, y0 + 7, HAT_LO)
    row(px, y0 + 8, 2 + ox, 11 + ox, HAT_LO)
    # white fur brim
    row(px, y0 + 9, 2 + ox, 12 + ox, WHITE)
    row(px, y0 + 10, 1 + ox, 12 + ox, WHITE)
    p(px, 3 + ox, y0 + 10, SHADE)
    p(px, 8 + ox, y0 + 10, SHADE)
    # pom-pom at tip (slightly off to the right)
    p(px, 8 + ox, y0 - 1, WHITE)
    p(px, 9 + ox, y0 - 1, WHITE)
    p(px, 8 + ox, y0, WHITE)
    p(px, 9 + ox, y0, WHITE)


def draw_face(px, ox=0, oy=0, bob=0):
    y0 = 12 + oy + bob
    # head / face
    row(px, y0, 3 + ox, 10 + ox, SKIN)
    row(px, y0 + 1, 3 + ox, 11 + ox, SKIN)
    row(px, y0 + 2, 3 + ox, 11 + ox, SKIN)
    row(px, y0 + 3, 4 + ox, 10 + ox, SKIN)
    # eye
    p(px, 5 + ox, y0 + 1, INK)
    p(px, 6 + ox, y0 + 1, WHITE)
    # rosy cheek
    p(px, 7 + ox, y0 + 2, NOSE)
    # BIG nose pointing right
    p(px, 10 + ox, y0 + 1, NOSE)
    p(px, 11 + ox, y0 + 1, NOSE)
    p(px, 12 + ox, y0 + 1, NOSE)
    p(px, 10 + ox, y0 + 2, NOSE)
    p(px, 11 + ox, y0 + 2, NOSE)
    p(px, 12 + ox, y0 + 2, NOSE)
    p(px, 13 + ox, y0 + 2, NOSE)
    p(px, 11 + ox, y0 + 3, NOSE)
    p(px, 12 + ox, y0 + 3, NOSE)
    # highlight on nose
    p(px, 12 + ox, y0 + 1, HAT_HI)
    # white mustache under nose
    row(px, y0 + 3, 5 + ox, 10 + ox, WHITE)
    p(px, 4 + ox, y0 + 3, WHITE)


def draw_beard(px, ox=0, oy=0, bob=0, sway=0):
    y0 = 16 + oy + bob
    sx = ox + sway
    row(px, y0, 2 + sx, 11 + sx, WHITE)
    row(px, y0 + 1, 1 + sx, 12 + sx, WHITE)
    row(px, y0 + 2, 1 + sx, 12 + sx, WHITE)
    row(px, y0 + 3, 2 + sx, 11 + sx, WHITE)
    row(px, y0 + 4, 3 + sx, 10 + sx, WHITE)
    row(px, y0 + 5, 4 + sx, 9 + sx, WHITE)
    row(px, y0 + 6, 5 + sx, 8 + sx, WHITE)
    # shade
    p(px, 4 + sx, y0 + 1, SHADE)
    p(px, 8 + sx, y0 + 2, SHADE)
    p(px, 6 + sx, y0 + 4, SHADE)
    p(px, 7 + sx, y0 + 5, SHADE)


def draw_body(px, ox=0, oy=0, bob=0):
    y0 = 20 + oy + bob
    row(px, y0, 3 + ox, 11 + ox, TUNIC)
    row(px, y0 + 1, 3 + ox, 11 + ox, TUNIC)
    row(px, y0 + 2, 3 + ox, 11 + ox, GOLD)  # belt
    p(px, 7 + ox, y0 + 2, 7)  # buckle
    p(px, 8 + ox, y0 + 2, WHITE)
    row(px, y0 + 3, 4 + ox, 10 + ox, TUNIC)
    row(px, y0 + 4, 4 + ox, 10 + ox, TUNIC)
    p(px, 4 + ox, y0 + 1, BOOT)  # fold
    p(px, 10 + ox, y0 + 3, BOOT)


def draw_boots(px, ox=0, oy=0, gait=0):
    """gait 0=together, 1=left forward, 2=right forward, 3=other step."""
    y0 = 26 + oy
    if gait == 0:
        row(px, y0, 4 + ox, 7 + ox, BOOT)
        row(px, y0, 8 + ox, 11 + ox, BOOT)
        row(px, y0 + 1, 3 + ox, 7 + ox, BOOT)
        row(px, y0 + 1, 8 + ox, 12 + ox, BOOT)
        row(px, y0 + 2, 3 + ox, 6 + ox, BOOT)
        row(px, y0 + 2, 9 + ox, 12 + ox, BOOT)
        row(px, y0 + 3, 3 + ox, 6 + ox, INK)
        row(px, y0 + 3, 9 + ox, 12 + ox, INK)
        p(px, 6 + ox, y0 + 3, BOOT)
        p(px, 12 + ox, y0 + 3, BOOT)
    elif gait == 1:
        # front foot (right) steps forward
        row(px, y0, 5 + ox, 8 + ox, BOOT)
        row(px, y0 + 1, 5 + ox, 8 + ox, BOOT)
        row(px, y0 + 2, 8 + ox, 13 + ox, BOOT)
        row(px, y0 + 3, 9 + ox, 13 + ox, INK)
        p(px, 13 + ox, y0 + 2, INK)
        row(px, y0 + 1, 3 + ox, 5 + ox, BOOT)
        row(px, y0 + 2, 3 + ox, 5 + ox, BOOT)
    elif gait == 2:
        row(px, y0, 4 + ox, 10 + ox, BOOT)
        row(px, y0 + 1, 3 + ox, 7 + ox, BOOT)
        row(px, y0 + 1, 8 + ox, 11 + ox, BOOT)
        row(px, y0 + 2, 2 + ox, 6 + ox, BOOT)
        row(px, y0 + 2, 9 + ox, 11 + ox, BOOT)
        row(px, y0 + 3, 2 + ox, 6 + ox, INK)
        row(px, y0 + 3, 9 + ox, 11 + ox, INK)
    else:
        row(px, y0, 4 + ox, 11 + ox, BOOT)
        row(px, y0 + 1, 4 + ox, 7 + ox, BOOT)
        row(px, y0 + 1, 8 + ox, 12 + ox, BOOT)
        row(px, y0 + 2, 4 + ox, 6 + ox, BOOT)
        row(px, y0 + 2, 9 + ox, 13 + ox, BOOT)
        row(px, y0 + 3, 4 + ox, 6 + ox, INK)
        row(px, y0 + 3, 10 + ox, 13 + ox, INK)


def pick_head(px, cx, cy, horiz=True):
    """Chunky two-point pickaxe head around (cx, cy)."""
    if horiz:
        for x in range(cx - 4, cx + 5):
            p(px, x, cy, STEEL)
            p(px, x, cy - 1, METAL)
            p(px, x, cy + 1, METAL)
        p(px, cx - 4, cy - 2, METAL)
        p(px, cx + 4, cy - 2, WHITE)
        p(px, cx - 4, cy + 2, METAL)
        p(px, cx + 4, cy + 2, WHITE)
        p(px, cx, cy, WHITE)
    else:
        for y in range(cy - 4, cy + 5):
            p(px, cx, y, STEEL)
            p(px, cx - 1, y, METAL)
            p(px, cx + 1, y, METAL)
        p(px, cx - 2, cy - 4, METAL)
        p(px, cx + 2, cy - 4, WHITE)
        p(px, cx, cy, WHITE)


def draw_pickaxe(px, pose="hip", ox=0, oy=0, bob=0):
    """pose: hip (idle on back/side), swing0..3 attack."""
    if pose == "hip":
        hx, hy = 1 + ox, 16 + oy + bob
        for i in range(12):
            p(px, hx, hy + i, BOOT)
            p(px, hx + 1, hy + i, 12)
        pick_head(px, hx + 1, hy + 1, True)
    elif pose == "swing0":
        hx, hy = 3 + ox, 6 + oy + bob
        for i in range(10):
            p(px, hx + i // 4, hy + i, BOOT)
            p(px, hx + 1 + i // 4, hy + i, 12)
        pick_head(px, hx + 1, hy + 1, True)
    elif pose == "swing1":
        hx, hy = 8 + ox, 3 + oy + bob
        for i in range(11):
            p(px, hx, hy + i, BOOT)
            p(px, hx + 1, hy + i, 12)
        pick_head(px, hx, hy + 1, True)
    elif pose == "swing2":
        hx, hy = 10 + ox, 10 + oy + bob
        for i in range(9):
            p(px, hx - i // 5, hy + i, BOOT)
            p(px, hx + 1 - i // 5, hy + i, 12)
        pick_head(px, hx + 2, hy, True)
    else:
        hx, hy = 11 + ox, 16 + oy + bob
        for i in range(8):
            p(px, hx - i // 4, hy + i, BOOT)
        pick_head(px, hx + 1, hy + 1, True)


def to_image(px):
    im = Image.new("P", (16, 32))
    im.putdata([px[y][x] for y in range(32) for x in range(16)])
    return im


def make_frame(kind: str, phase: int) -> Image.Image:
    px = blank()
    bob = 0
    sway = 0
    ox = 0
    gait = 0
    pose = "hip"
    if kind == "idle":
        bob = 0 if phase % 2 == 0 else 1
        sway = 0 if phase < 2 else (1 if phase == 2 else 0)
        gait = 0
        pose = "hip"
    elif kind == "run":
        bob = 0 if phase % 2 == 0 else 1
        sway = 1 if phase in (1, 2) else 0
        gait = (1, 0, 2, 3)[phase % 4]
        pose = "hip"
        ox = 0 if phase % 2 == 0 else 0
    elif kind == "atk":
        bob = 0
        gait = 0
        pose = ("swing0", "swing1", "swing2", "swing3")[phase % 4]
        if phase >= 2:
            ox = 1  # lean into the strike

    # draw back-to-front: pickaxe behind on hip/windup, in front on strike
    axe_front = pose in ("swing2", "swing3")
    if not axe_front:
        draw_pickaxe(px, pose, ox=ox, bob=bob)
    draw_hat(px, ox=ox, bob=bob)
    draw_face(px, ox=ox, bob=bob)
    draw_beard(px, ox=ox, bob=bob, sway=sway)
    draw_body(px, ox=ox, bob=bob)
    draw_boots(px, ox=ox, gait=gait)
    if axe_front:
        draw_pickaxe(px, pose, ox=ox, bob=bob)
    add_outline(px)
    return to_image(px)


def gen_gnome_frames():
    frames = [make_frame("idle", i) for i in range(4)]
    frames += [make_frame("run", i) for i in range(4)]
    frames += [make_frame("atk", i) for i in range(4)]
    sheet = Image.new("P", (16 * 12, 32))
    raw = []
    for c in PAL1:
        raw.extend(c)
    raw.extend([0] * (768 - len(raw)))
    sheet.putpalette(raw)
    for i, fr in enumerate(frames):
        sheet.paste(fr, (i * 16, 0))
    return sheet


# ---------- doors (use existing dungeon palette) ----------
# dungeon.png palette (index 0 unused magenta, tiles use 1+)
PAL0 = [
    (255, 0, 255),
    (145, 109, 109),
    (72, 72, 72),
    (0, 0, 0),
    (182, 182, 109),
    (182, 72, 72),
    (255, 109, 72),
    (145, 109, 182),
    (255, 218, 182),
    (255, 109, 109),
    (145, 72, 72),
    (145, 255, 109),
    (72, 72, 109),
    (145, 109, 72),
    (255, 218, 109),
    (36, 36, 36),
]


def door_closed():
    t = [[2] * 16 for _ in range(16)]
    # stone frame
    for y in range(16):
        t[y][0] = 1
        t[y][15] = 15
        t[y][1] = 13
        t[y][14] = 13
    for x in range(16):
        t[0][x] = 1
        t[15][x] = 15
    # wooden planks
    plank = [13, 10, 5, 13]
    for x in range(2, 14):
        col = plank[(x - 2) % 4]
        for y in range(1, 15):
            t[y][x] = col
            if x in (5, 9) and 2 <= y <= 13:
                t[y][x] = 10  # groove
    # horizontal braces
    for x in range(2, 14):
        t[4][x] = 6
        t[11][x] = 6
    # handle
    t[8][11] = 14
    t[8][12] = 7
    t[9][11] = 14
    t[9][12] = 4
    t[7][12] = 14
    return t


def door_open():
    t = [[8] * 16 for _ in range(16)]
    # warm lit interior — NOT a black hole
    for y in range(16):
        for x in range(16):
            # brighter toward center
            d = abs(x - 8) + abs(y - 8)
            if d < 5:
                t[y][x] = 14
            elif d < 8:
                t[y][x] = 8
            else:
                t[y][x] = 4
    # gold/stone arch frame, clearly an exit
    for y in range(16):
        t[y][0] = 13
        t[y][1] = 14
        t[y][14] = 14
        t[y][15] = 13
    for x in range(16):
        t[0][x] = 13
        t[1][x] = 14
        t[14][x] = 10
        t[15][x] = 13
    # extra highlight on the inner arch
    for x in range(2, 14):
        t[2][x] = 8
    t[2][2] = 14
    t[2][13] = 14
    # floor threshold
    for x in range(2, 14):
        t[13][x] = 13
        t[12][x] = 6
    return t


def door_locked():
    t = door_closed()
    # large gold lock plate
    for y in range(6, 12):
        for x in range(5, 11):
            t[y][x] = 14
    for x in range(5, 11):
        t[6][x] = 7
        t[11][x] = 13
    for y in range(6, 12):
        t[y][5] = 7
        t[y][10] = 13
    # keyhole
    t[8][7] = 3
    t[8][8] = 3
    t[9][7] = 3
    t[9][8] = 15
    t[10][7] = 3
    # bar across
    for x in range(3, 13):
        t[5][x] = 7
        t[12][x] = 7
    return t


def paste_tile(dst: Image.Image, tile, y0: int):
    for y in range(16):
        for x in range(16):
            dst.putpixel((x, y0 + y), tile[y][x])


def patch_dungeon_doors(path: Path):
    im = Image.open(path)
    if im.mode != "P":
        raise SystemExit(f"{path} is not indexed")
    # keep existing palette
    pal = im.getpalette()
    out = im.copy()
    paste_tile(out, door_closed(), 8 * 16)
    paste_tile(out, door_open(), 9 * 16)
    paste_tile(out, door_locked(), 10 * 16)
    out.putpalette(pal)
    out.save(path)
    return out


def main():
    sheet = gen_gnome_frames()
    dest = SPR / "gnome.png"
    sheet.save(dest)
    patch_dungeon_doors(GFX / "dungeon.png")
    # previews
    prev = sheet.convert("RGB").resize((16 * 12 * 4, 32 * 4), Image.NEAREST)
    prev.save("/tmp/gnome_new_preview.png")
    d = Image.open(GFX / "dungeon.png")
    doors = Image.new("RGB", (16 * 3 * 6, 16 * 6), (20, 20, 20))
    for i, name in enumerate((8, 9, 10)):
        tile = d.crop((0, name * 16, 16, name * 16 + 16)).convert("RGB")
        doors.paste(tile.resize((96, 96), Image.NEAREST), (i * 96, 0))
    doors.save("/tmp/doors_new_preview.png")
    print("wrote", dest, "and patched dungeon doors")


if __name__ == "__main__":
    main()
