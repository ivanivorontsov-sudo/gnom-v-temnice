#include <genesis.h>
#include "fontmap.h"
#include "resources.h"
#include "dungeon.h"

#define MAP_W 20
#define MAP_H 12
#define HUD_H 32
#define TS 16
#define MAX_EN 8
#define MAX_SHOT 8
#define MAX_DROP 16
#define MAX_BOMB 2
#define MAX_FLOORS 3

#define C_EMPTY  0
#define C_WALL   1
#define C_STAIRS 7
#define C_DOOR   2
#define C_LOCK   10

#define AI_CHASE  0
#define AI_AMBUSH 1
#define AI_WANDER 2
#define AI_ARCHER 3
#define AI_SHAMAN 4
#define AI_BOSS   5


#define DROP_GOLD 0
#define DROP_HEART 1
#define DROP_KEY 2
#define DROP_GEM 3
#define DROP_FOOD 4
#define DROP_ITEM 5

#define ST_TITLE 0
#define ST_PLAY 1
#define ST_PAUSE 2
#define ST_DEAD 3
#define ST_WIN 4
#define ST_BANNER 5

#define IT_PICK 0
#define IT_BOOTS 1
#define IT_STONE 2
#define IT_GREED 3
#define IT_POWDER 4
#define IT_CLOAK 5

static const s16 ddx[4] = {0, 0, -1, 1};
static const s16 ddy[4] = {-1, 1, 0, 0};

typedef struct {
    s16 x, y;
    s16 hp, hpmax;
    u16 gold, score, power, hung_t;
    s8 fx, fy;
    u8 bombs, keys, dmg, speed;
    u8 iframes, cloak, greed, powder;
    u8 attack, atk_hit, moving, hunger;
    u8 items;
} Player;

typedef struct {
    s16 x, y;
    s16 hp, hpmax;
    Sprite *spr;
    u8 alive, stun, fear, timer;
    u8 ai, shot, anim, flag;
    s8 wdx, wdy;
} Enemy;

typedef struct {
    s16 x, y, vx, vy;
    u8 kind, alive, dmg, life;
    Sprite *spr;
} Shot;

typedef struct {
    s16 x, y;
    u8 kind, val, alive, frame;
    Sprite *spr;
} Drop;

typedef struct {
    s16 x, y;
    u8 fuse, alive, boom;
    Sprite *spr;
} Bomb;

static u16 font_base, dung_base, logo_base;
static u8 cmap[MAP_H][MAP_W];
static u8 tmap[MAP_H][MAP_W];
static Room rooms[MAX_ROOMS];
static u8 room_n, cur_ri, floor_n, boss_unlocked, key_given;
static u8 grid[GRID][GRID];
static Player P;
static Enemy ens[MAX_EN];
static Shot shots[MAX_SHOT];
static Drop drops[MAX_DROP];
static Bomb bombs[MAX_BOMB];
static Sprite *sPlayer, *sSlash, *sBoss;
static u8 state, prev_state;
static u16 joy, joy_prev, frames, banner_t, msg_t, scroll;
static u32 best_score, seed;
static char msg[32];
static u8 music_on;
static u8 grace_t, flash_t;
static u16 room_clock;

static const char *item_name[6] = {
    "ОСТРАЯ КИРКА",
    "САПОГИ ГОНЦА",
    "КАМЕННОЕ СЕРДЦЕ",
    "ЖАДНЫЙ НОС",
    "БОЧОНОК ПОРОХА",
    "ПРИЗРАЧНЫЙ ПЛАЩ"
};

static s16 iabs(s16 v) { return v < 0 ? -v : v; }
static s16 iclamp(s16 v, s16 a, s16 b) { return v < a ? a : (v > b ? b : v); }
static u16 rnd(u16 n) { return n ? (random() % n) : 0; }

static u16 utf8_next(const char **ps)
{
    const u8 *s = (const u8 *)*ps;
    if (s[0] < 0x80) { *ps += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0) {
        u16 cp = ((u16)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *ps += 2;
        return cp;
    }
    if ((s[0] & 0xF0) == 0xE0) {
        u16 cp = ((u16)(s[0] & 0x0F) << 12) | ((u16)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *ps += 3;
        return cp;
    }
    *ps += 1;
    return (u16)'?';
}

static u16 map_cp(u16 cp)
{
    u16 i;
    if (cp >= 'a' && cp <= 'z') cp -= 32;
    if (cp >= 0x0430 && cp <= 0x044F) cp -= 0x20;
    if (cp == 0x0451) cp = 0x0401;
    for (i = 0; i < FONT_COUNT; i++) if (FONT_CPS[i] == cp) return i;
    return 0;
}

static void draw_ch(u16 x, u16 y, u16 idx, u16 pal)
{
    u16 t = font_base + idx * 2;
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal, TRUE, FALSE, FALSE, t), x, y);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal, TRUE, FALSE, FALSE, t + 1), x, y + 1);
}

static void draw_str(u16 x, u16 y, const char *s)
{
    while (*s) {
        u16 cp = utf8_next(&s);
        draw_ch(x++, y, map_cp(cp), PAL3);
        if (x >= 40) break;
    }
}

static void draw_num(u16 x, u16 y, u32 n, u8 w)
{
    char buf[12];
    s16 i = (s16)w - 1;
    u8 k;
    for (k = 0; k < w; k++) buf[k] = '0';
    buf[w] = 0;
    while (i >= 0) {
        buf[i--] = (char)('0' + (n % 10));
        n /= 10;
        if (!n) break;
    }
    draw_str(x, y, buf);
}

static void hud_clear(void)
{
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, font_base), 0, 0, 40, 1);
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, font_base + 1), 0, 1, 40, 1);
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, font_base), 0, 2, 40, 1);
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, font_base + 1), 0, 3, 40, 1);
}

static void sfx_play(const u8 *s, u32 n)
{
    XGM2_playPCM(s, n, SOUND_PCM_CH2);
}

static void show_msg(const char *s)
{
    u16 i = 0;
    while (s[i] && i < 31) { msg[i] = s[i]; i++; }
    msg[i] = 0;
    msg_t = 90;
}

static void sram_load(void)
{
    u16 mag;
    SRAM_enable();
    mag = ((u16)SRAM_readByte(0) << 8) | SRAM_readByte(1);
    if (mag == 0x474E) {
        best_score = ((u32)SRAM_readByte(2) << 24) | ((u32)SRAM_readByte(3) << 16)
                   | ((u32)SRAM_readByte(4) << 8) | SRAM_readByte(5);
    } else best_score = 0;
    SRAM_disable();
}

static void sram_save(void)
{
    SRAM_enable();
    SRAM_writeByte(0, 0x47);
    SRAM_writeByte(1, 0x4E);
    SRAM_writeByte(2, (u8)(best_score >> 24));
    SRAM_writeByte(3, (u8)(best_score >> 16));
    SRAM_writeByte(4, (u8)(best_score >> 8));
    SRAM_writeByte(5, (u8)best_score);
    SRAM_disable();
}

static u8 cell_at(s16 px, s16 py)
{
    s16 mx, my;
    if (py < HUD_H) return C_WALL;
    mx = px / TS;
    my = (py - HUD_H) / TS;
    if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) return C_WALL;
    return cmap[my][mx];
}

static int solid_at(s16 px, s16 py)
{
    u8 c = cell_at(px, py);
    if (c == C_WALL) return 1;
    if (c >= C_LOCK && c < C_LOCK + 4) {
        if (!boss_unlocked) return 2;
        return 0;
    }
    return 0;
}

static int box_solid(s16 x, s16 y, s16 w, s16 h)
{
    if (solid_at(x, y)) return 1;
    if (solid_at(x + w - 1, y)) return 1;
    if (solid_at(x, y + h - 1)) return 1;
    if (solid_at(x + w - 1, y + h - 1)) return 1;
    return 0;
}

static int try_move_xy(s16 *x, s16 *y, s16 dx, s16 dy, s16 w, s16 h)
{
    s16 nx = *x + dx, ny = *y + dy;
    int hit = 0;
    if (dx && !box_solid(nx, *y, w, h)) *x = nx;
    else if (dx) hit = 1;
    if (dy && !box_solid(*x, ny, w, h)) *y = ny;
    else if (dy) hit = 1;
    return hit;
}

static int aabb(s16 ax, s16 ay, s16 aw, s16 ah, s16 bx, s16 by, s16 bw, s16 bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static Room *room_at(u8 gx, u8 gy)
{
    u8 i;
    for (i = 0; i < room_n; i++)
        if (rooms[i].used && rooms[i].gx == gx && rooms[i].gy == gy) return &rooms[i];
    return NULL;
}

static const SpriteDefinition *en_sprite_def(u8 ai)
{
    if (ai == AI_SHAMAN || ai == AI_ARCHER) return &sprShaman;
    if (ai == AI_WANDER || ai == AI_AMBUSH) return &sprOrc;
    return &sprGoblin;
}

static int en_is_tall(u8 ai)
{
    return (ai == AI_WANDER || ai == AI_AMBUSH || ai == AI_ARCHER || ai == AI_SHAMAN || ai == AI_BOSS);
}

static int tile_empty(s16 mx, s16 my)
{
    if (mx < 1 || my < 1 || mx >= MAP_W - 1 || my >= MAP_H - 1) return 0;
    return cmap[my][mx] == C_EMPTY;
}

static int pick_spawn_xy(s16 *ox, s16 *oy, s16 ax, s16 ay, s16 mindist)
{
    u8 t;
    for (t = 0; t < 48; t++) {
        u8 mx = 2 + rnd(MAP_W - 4);
        u8 my = 2 + rnd(MAP_H - 4);
        s16 x, y;
        if (!tile_empty(mx, my)) continue;
        x = (s16)mx * TS + 2;
        y = HUD_H + (s16)my * TS + 2;
        if (box_solid(x, y, 12, 12)) continue;
        if (iabs(x - ax) + iabs(y - ay) < mindist) continue;
        *ox = x;
        *oy = y;
        return 1;
    }
    return 0;
}

static void draw_mt(u16 mx, u16 my, u16 mt)
{
    u16 t = dung_base + mt * 4;
    u16 tx = mx * 2;
    u16 ty = my * 2 + 4;
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, t), tx, ty);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, t + 1), tx + 1, ty);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, t + 2), tx, ty + 1);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, t + 3), tx + 1, ty + 1);
}

static void fill_parallax(void)
{
    u16 x, y, t;
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 64; x++) {
            t = dung_base + (((x / 2 + y / 2) & 1) ? MT_WALL : MT_FLOOR3) * 4 + ((x + y) & 3);
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, t), x, y);
        }
    }
}

static void hide_sprite(Sprite *s)
{
    if (!s) return;
    SPR_setPosition(s, 336, 240);
}

static void show_sprite(Sprite *s)
{
    (void)s;
}

static void clear_entities(void)
{
    u8 i;
    for (i = 0; i < MAX_EN; i++) {
        ens[i].alive = 0;
        hide_sprite(ens[i].spr);
    }
    for (i = 0; i < MAX_SHOT; i++) {
        shots[i].alive = 0;
        hide_sprite(shots[i].spr);
    }
    for (i = 0; i < MAX_DROP; i++) {
        drops[i].alive = 0;
        hide_sprite(drops[i].spr);
    }
    for (i = 0; i < MAX_BOMB; i++) {
        bombs[i].alive = 0;
        hide_sprite(bombs[i].spr);
    }
    hide_sprite(sBoss);
    hide_sprite(sSlash);
}

static Drop *add_drop(s16 x, s16 y, u8 kind, u8 val)
{
    u8 i;
    for (i = 0; i < MAX_DROP; i++) if (!drops[i].alive) {
        drops[i].alive = 1;
        drops[i].x = x;
        drops[i].y = y;
        drops[i].kind = kind;
        drops[i].val = val;
        drops[i].frame = 0;
        if (drops[i].spr) {
            show_sprite(drops[i].spr);
            SPR_setPosition(drops[i].spr, x, y);
        }
        return &drops[i];
    }
    return NULL;
}

static Shot *add_shot(s16 x, s16 y, s16 vx, s16 vy, u8 kind, u8 dmg)
{
    u8 i;
    for (i = 0; i < MAX_SHOT; i++) if (!shots[i].alive) {
        shots[i].alive = 1;
        shots[i].x = x;
        shots[i].y = y;
        shots[i].vx = vx;
        shots[i].vy = vy;
        shots[i].kind = kind;
        shots[i].dmg = dmg;
        shots[i].life = 90;
        if (shots[i].spr) {
            show_sprite(shots[i].spr);
            SPR_setFrame(shots[i].spr, kind);
            SPR_setPosition(shots[i].spr, x, y);
        }
        return &shots[i];
    }
    return NULL;
}

static void spawn_gold_field(u8 n)
{
    u8 k, t;
    for (k = 0; k < n; k++) {
        for (t = 0; t < 20; t++) {
            u8 mx = 2 + rnd(MAP_W - 4);
            u8 my = 2 + rnd(MAP_H - 4);
            if (cmap[my][mx] == C_EMPTY) {
                add_drop(mx * TS + 4, HUD_H + my * TS + 4, DROP_GOLD, (u8)(1 + rnd(3)));
                break;
            }
        }
    }
}

static Enemy *spawn_en(s16 x, s16 y, u8 ai, s16 hp)
{
    u8 i;
    for (i = 0; i < MAX_EN; i++) if (!ens[i].alive) {
        ens[i].alive = 1;
        ens[i].x = x;
        ens[i].y = y;
        ens[i].ai = ai;
        ens[i].hp = hp;
        ens[i].hpmax = hp;
        ens[i].stun = 0;
        ens[i].fear = 0;
        ens[i].timer = 0;
        ens[i].shot = 40 + rnd(40);
        ens[i].anim = 0;
        ens[i].flag = 0;
        ens[i].wdx = 0;
        ens[i].wdy = 0;
        if (ai == AI_BOSS) {
            if (sBoss) { show_sprite(sBoss); SPR_setPosition(sBoss, x - 8, y - 16); }
            hide_sprite(ens[i].spr);
        } else if (ens[i].spr) {
            SPR_setDefinition(ens[i].spr, en_sprite_def(ai));
            SPR_setVisibility(ens[i].spr, VISIBLE);
            SPR_setPosition(ens[i].spr, x, y + (en_is_tall(ai) ? -16 : 0));
        }
        return &ens[i];
    }
    return NULL;
}

static void spawn_room_enemies(Room *r)
{
    u8 n, i, ai;
    u8 basehp = 2 + (floor_n / 2);
    if (r->type == RT_BOSS) {
        spawn_en(152, HUD_H + 80, AI_BOSS, 12 + floor_n * 5);
        return;
    }
    if (r->type != RT_NORMAL) return;
    n = 2 + floor_n + rnd(2);
    if (n > 6) n = 6;
    for (i = 0; i < n; i++) {
        s16 x, y;
        u8 roll;
        if (!pick_spawn_xy(&x, &y, P.x, P.y, 56)) continue;
        roll = rnd(100);
        if (floor_n == 1) {
            if (roll < 50) ai = AI_CHASE;
            else if (roll < 70) ai = AI_AMBUSH;
            else if (roll < 85) ai = AI_WANDER;
            else if (roll < 95) ai = AI_ARCHER;
            else ai = AI_SHAMAN;
        } else if (floor_n == 2) {
            if (roll < 30) ai = AI_CHASE;
            else if (roll < 50) ai = AI_AMBUSH;
            else if (roll < 65) ai = AI_WANDER;
            else if (roll < 85) ai = AI_ARCHER;
            else ai = AI_SHAMAN;
        } else {
            if (roll < 20) ai = AI_CHASE;
            else if (roll < 40) ai = AI_AMBUSH;
            else if (roll < 55) ai = AI_WANDER;
            else if (roll < 80) ai = AI_ARCHER;
            else ai = AI_SHAMAN;
        }
        spawn_en(x, y, ai, (ai == AI_SHAMAN || ai == AI_WANDER) ? basehp + 1 : basehp);
    }
}

static void gen_floor(void)
{
    u8 attempt;
    boss_unlocked = 0;
    key_given = 0;
    for (attempt = 0; attempt < 24; attempt++) {
        dgen_floor(rooms, &room_n, grid, rnd);
        if (dvalidate_floor(rooms, room_n, NULL, 0)) break;
    }
    dfix_floor(rooms, room_n);
}

static int door_locked(Room *r, u8 dir)
{
    Room *nb;
    if (!(r->doors & (1 << dir))) return 0;
    nb = room_at((u8)(r->gx + ddx[dir]), (u8)(r->gy + ddy[dir]));
    if (nb && nb->type == RT_BOSS && !boss_unlocked) return 1;
    return 0;
}

static void build_room(Room *r, int closed)
{
    u8 x, y, d;
    u8 floor_mt = MT_FLOOR1 + (floor_n % 3);
    if (r->type == RT_BOSS) floor_mt = MT_BOSS_FL;
    if (r->type == RT_ITEM) floor_mt = MT_CARPET;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) {
                cmap[y][x] = C_WALL;
                tmap[y][x] = (x == 0 || x == MAP_W - 1) ? MT_WALL_SIDE : MT_WALL;
            } else {
                cmap[y][x] = C_EMPTY;
                tmap[y][x] = floor_mt;
                if (((x * 3 + y * 7 + r->gx) & 7) == 0) tmap[y][x] = floor_mt + 1;
            }
        }
    }
    /* decorations */
    tmap[2][2] = MT_BANNER_R;
    tmap[2][MAP_W - 3] = MT_BANNER_G;
    if (r->type == RT_NORMAL && rnd(2)) tmap[MAP_H - 3][3] = MT_SKULL;
    if (r->type == RT_ITEM) {
        tmap[MAP_H / 2][MAP_W / 2] = MT_PEDESTAL;
    }
    /* doors at mid walls: (9,0)/(10,0) etc */
    for (d = 0; d < 4; d++) {
        u8 ox1, oy1, ox2, oy2, locked, open;
        if (!(r->doors & (1 << d))) continue;
        locked = door_locked(r, d);
        open = !closed && !locked;
        if (d == 0) { ox1 = 9; oy1 = 0; ox2 = 10; oy2 = 0; }
        else if (d == 1) { ox1 = 9; oy1 = MAP_H - 1; ox2 = 10; oy2 = MAP_H - 1; }
        else if (d == 2) { ox1 = 0; oy1 = 5; ox2 = 0; oy2 = 6; }
        else { ox1 = MAP_W - 1; oy1 = 5; ox2 = MAP_W - 1; oy2 = 6; }
        if (locked) {
            cmap[oy1][ox1] = C_LOCK + d; cmap[oy2][ox2] = C_LOCK + d;
            tmap[oy1][ox1] = MT_DOOR_L; tmap[oy2][ox2] = MT_DOOR_L;
        } else if (open) {
            cmap[oy1][ox1] = C_DOOR + d; cmap[oy2][ox2] = C_DOOR + d;
            tmap[oy1][ox1] = MT_DOOR_O; tmap[oy2][ox2] = MT_DOOR_O;
        } else {
            cmap[oy1][ox1] = C_WALL; cmap[oy2][ox2] = C_WALL;
            tmap[oy1][ox1] = MT_DOOR_C; tmap[oy2][ox2] = MT_DOOR_C;
        }
    }
    if (r->type == RT_BOSS && r->cleared) {
        cmap[MAP_H / 2][MAP_W / 2] = C_STAIRS;
        tmap[MAP_H / 2][MAP_W / 2] = MT_STAIRS;
    }
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++)
            draw_mt(x, y, tmap[y][x]);
}

static u8 count_alive_reachable(void)
{
    u8 i, n = 0;
    for (i = 0; i < MAX_EN; i++) if (ens[i].alive) {
        if (ens[i].x < 12 || ens[i].x > 300 || ens[i].y < HUD_H + 8 || ens[i].y > 214
            || box_solid(ens[i].x + 2, ens[i].y + 2, 8, 8)) {
            ens[i].alive = 0;
            hide_sprite(ens[i].spr);
            if (ens[i].ai == AI_BOSS) hide_sprite(sBoss);
            continue;
        }
        n++;
    }
    return n;
}

static void open_doors_now(Room *r)
{
    build_room(r, 0);
}

static void snap_player_safe(void)
{
    u8 t;
    if (!box_solid(P.x, P.y, 12, 14)) return;
    P.x = 152;
    P.y = HUD_H + 88;
    if (!box_solid(P.x, P.y, 12, 14)) return;
    for (t = 0; t < 40; t++) {
        s16 x = 32 + (s16)rnd(240);
        s16 y = HUD_H + 32 + (s16)rnd(120);
        if (!box_solid(x, y, 12, 14)) { P.x = x; P.y = y; return; }
    }
}

static void place_player_from(u8 from_dir)
{
    if (from_dir == 0) { P.x = 152; P.y = HUD_H + 20; P.fy = 1; P.fx = 0; }
    else if (from_dir == 1) { P.x = 152; P.y = HUD_H + (MAP_H - 2) * TS - 4; P.fy = -1; P.fx = 0; }
    else if (from_dir == 2) { P.x = 20; P.y = HUD_H + 88; P.fx = 1; P.fy = 0; }
    else if (from_dir == 3) { P.x = (MAP_W - 2) * TS - 4; P.y = HUD_H + 88; P.fx = -1; P.fy = 0; }
    else { P.x = 152; P.y = HUD_H + 88; P.fx = 0; P.fy = 1; }
    snap_player_safe();
}

static void enter_room(u8 gx, u8 gy, s16 from_dir)
{
    Room *r = room_at(gx, gy);
    u8 i, closed, first;
    if (!r) return;
    cur_ri = (u8)(r - rooms);
    clear_entities();
    closed = (!r->cleared && (r->type == RT_NORMAL || r->type == RT_BOSS));
    build_room(r, closed);
    first = !r->visited;
    r->visited = 1;
    if (!r->cleared && (r->type == RT_NORMAL || r->type == RT_BOSS)) {
        spawn_room_enemies(r);
        sfx_play(sfxDoor, sizeof(sfxDoor));
    }
    if (r->type == RT_START && first) spawn_gold_field(8);
    if (r->type == RT_NORMAL && r->cleared && first) spawn_gold_field(2);
    if (r->type == RT_NORMAL && !r->cleared) spawn_gold_field(5 + rnd(4));
    if (r->type == RT_ITEM && first) {
        /* item on pedestal — pick unused */
        u8 choice = 0xFF, k;
        for (k = 0; k < 6; k++) {
            u8 id = (u8)((seed + floor_n + k) % 6);
            if (!(P.items & (1 << id))) { choice = id; break; }
        }
        if (choice != 0xFF)
            add_drop(MAP_W / 2 * TS, HUD_H + MAP_H / 2 * TS - 16, DROP_ITEM, choice);
    }
    place_player_from((from_dir < 0) ? 255 : (u8)from_dir);
    grace_t = TRANSITION_GRACE;
    room_clock = 0;
    for (i = 0; i < 4; i++) { /* dummy keep compiler calm */ }
}

static void next_floor(void)
{
    floor_n++;
    if (floor_n > MAX_FLOORS) {
        state = ST_WIN;
        P.score += 500 + (u16)P.hp * 10 + (u16)P.gold;
        if (P.score > best_score) { best_score = P.score; sram_save(); }
        return;
    }
    P.score += 100;
    gen_floor();
    enter_room(3, 3, -1);
    show_msg("ЭТАЖ");
    state = ST_PLAY;
}

static void give_item(u8 id)
{
    P.items |= (1 << id);
    show_msg(item_name[id]);
    sfx_play(sfxPower, sizeof(sfxPower));
    if (id == IT_PICK) P.dmg = 2;
    if (id == IT_BOOTS) P.speed = 3;
    if (id == IT_STONE) {
        P.hpmax += 4;
        P.hp += 4;
        if (P.hp > P.hpmax) P.hp = P.hpmax;
    }
    if (id == IT_GREED) P.greed = 1;
    if (id == IT_POWDER) { P.powder = 1; if (P.bombs < 9) P.bombs += 2; }
    if (id == IT_CLOAK) P.cloak = 1;
}

static void hurt_player(u8 dmg)
{
    if (P.iframes || grace_t) return;
    if (P.hp <= dmg) P.hp = 0; else P.hp -= dmg;
    P.iframes = P.cloak ? 90 : 50;
    flash_t = 10;
    sfx_play(sfxHurt, sizeof(sfxHurt));
    if (P.hp == 0) {
        state = ST_DEAD;
        P.score += P.gold;
        if (P.score > best_score) { best_score = P.score; sram_save(); }
    }
}

static void kill_en(Enemy *e)
{
    e->alive = 0;
    hide_sprite(e->spr);
    if (e->ai == AI_BOSS) hide_sprite(sBoss);
    P.score += (e->ai == AI_BOSS) ? 500 : 10;
    if (rnd(4) == 0) add_drop(e->x, e->y, DROP_HEART, 2);
    else if (rnd(3) == 0) add_drop(e->x, e->y, DROP_FOOD, 3);
    else add_drop(e->x + 2, e->y + 2, DROP_GOLD, (u8)(2 + rnd(4)));
}

static void damage_en(Enemy *e, u8 dmg)
{
    if (!e->alive || e->stun) return;
    if (e->hp <= dmg) e->hp = 0; else e->hp -= dmg;
    e->stun = 12;
    sfx_play(sfxHit, sizeof(sfxHit));
    if (e->hp == 0) kill_en(e);
}

static void check_room_clear(void)
{
    Room *r = &rooms[cur_ri];
    if (r->cleared) return;
    if (r->type != RT_NORMAL && r->type != RT_BOSS) return;
    room_clock++;
    if (count_alive_reachable()) {
        if (room_clock < ROOM_FAILSAFE_FRAMES) return;
        show_msg("ДВЕРИ ОТКРЫТЫ");
    }
    r->cleared = 1;
    open_doors_now(r);
    sfx_play(sfxDoor, sizeof(sfxDoor));
    if (r->key_drop && !key_given) {
        add_drop(152, HUD_H + 88, DROP_KEY, 1);
        key_given = 1;
    } else {
        add_drop(152, HUD_H + 88, DROP_GOLD, 8);
    }
    if (rnd(2) == 0) add_drop(136, HUD_H + 72, DROP_FOOD, 3);
    if (r->type == RT_BOSS) {
        cmap[MAP_H / 2][MAP_W / 2] = C_STAIRS;
        tmap[MAP_H / 2][MAP_W / 2] = MT_STAIRS;
        draw_mt(MAP_W / 2, MAP_H / 2, MT_STAIRS);
        show_msg(floor_n >= MAX_FLOORS ? "ПОБЕДА?" : "СПУСК НИЖЕ");
    }
}

static void shoot_at(Enemy *e, u8 kind)
{
    s16 dx = P.x - e->x;
    s16 dy = P.y - e->y;
    s16 vx = 0, vy = 0;
    if (iabs(dx) > iabs(dy)) vx = (dx > 0) ? 2 : -2;
    else vy = (dy > 0) ? 2 : -2;
    if (kind == 1) { /* magic slight extra */
        if (vx && dy) vy = (dy > 0) ? 1 : -1;
        if (vy && dx) vx = (dx > 0) ? 1 : -1;
    }
    add_shot(e->x + 4, e->y + 4, vx, vy, kind, 1);
}

static void update_ai(Enemy *e)
{
    s16 tx = P.x, ty = P.y;
    s16 dx, dy, adx, ady, step;
    s16 w = (e->ai == AI_BOSS) ? 24 : 12;
    s16 h = 12;
    if (e->stun) { e->stun--; return; }
    if (P.power && e->fear < 8) e->fear = 8;
    if (e->fear) {
        e->fear--;
        dx = (e->x < P.x) ? -1 : 1;
        dy = (e->y < P.y) ? -1 : 1;
        try_move_xy(&e->x, &e->y, dx, dy, w, h);
        if (aabb(P.x, P.y, 12, 14, e->x, e->y, w, h)) {
            kill_en(e);
            P.score += 15;
        }
        return;
    }
    step = (e->ai == AI_BOSS && e->timer > 90 && e->timer < 130) ? 2 : 1;
    if (e->ai == AI_AMBUSH) {
        tx = P.x + P.fx * 48;
        ty = P.y + P.fy * 48;
    } else if (e->ai == AI_WANDER) {
        s16 dist = iabs(e->x - P.x) + iabs(e->y - P.y);
        if (dist < 80) {
            if (e->timer == 0) {
                e->timer = 30 + rnd(40);
                e->wdx = (s8)(rnd(3) - 1);
                e->wdy = (s8)(rnd(3) - 1);
            } else e->timer--;
            try_move_xy(&e->x, &e->y, e->wdx, e->wdy, w, h);
            goto collide;
        }
    } else if (e->ai == AI_ARCHER) {
        s16 dist = iabs(e->x - P.x) + iabs(e->y - P.y);
        if (dist < 64) { tx = e->x * 2 - P.x; ty = e->y * 2 - P.y; }
        else if (dist < 100) { tx = e->x; ty = e->y; }
        if (e->shot) e->shot--;
        else { shoot_at(e, 0); e->shot = 70; }
    } else if (e->ai == AI_SHAMAN) {
        s16 dist = iabs(e->x - P.x) + iabs(e->y - P.y);
        if (dist < 80) { tx = e->x * 2 - P.x; ty = e->y * 2 - P.y; }
        else if (dist < 130) { tx = e->x; ty = e->y; }
        if (e->shot) e->shot--;
        else { shoot_at(e, 1); e->shot = 90; }
    } else if (e->ai == AI_BOSS) {
        e->timer++;
        if (e->timer > 180) e->timer = 0;
        if (e->timer >= 130 && (e->timer % 12) == 0) {
            s16 k;
            for (k = -1; k <= 1; k++)
                add_shot(e->x + 8, e->y + 8, (P.x > e->x) ? 2 : -2, k, 2, 1);
        }
        if (e->hp < e->hpmax / 2 && !e->flag) {
            e->flag = 1;
            {
                s16 mx = e->x + 20, my = e->y;
                if (box_solid(mx, my, 12, 12))
                    pick_spawn_xy(&mx, &my, P.x, P.y, 40);
                spawn_en(mx, my, AI_CHASE, 2 + floor_n);
            }
        }
        step = (e->timer > 90 && e->timer < 130) ? 2 : 1;
    }
    dx = (tx > e->x) ? step : ((tx < e->x) ? -step : 0);
    dy = (ty > e->y) ? step : ((ty < e->y) ? -step : 0);
    /* prefer the longer axis first for maze-ish chase */
    adx = iabs(tx - e->x); ady = iabs(ty - e->y);
    if (adx > ady) {
        if (dx) try_move_xy(&e->x, &e->y, dx, 0, w, h);
        if (dy) try_move_xy(&e->x, &e->y, 0, dy, w, h);
    } else {
        if (dy) try_move_xy(&e->x, &e->y, 0, dy, w, h);
        if (dx) try_move_xy(&e->x, &e->y, dx, 0, w, h);
    }
collide:
    if (aabb(P.x, P.y, 12, 14, e->x, e->y, w, h))
        hurt_player(1);
}

static void update_shots(void)
{
    u8 i, j;
    for (i = 0; i < MAX_SHOT; i++) if (shots[i].alive) {
        shots[i].x += shots[i].vx;
        shots[i].y += shots[i].vy;
        shots[i].life--;
        if (!shots[i].life || solid_at(shots[i].x + 8, shots[i].y + 8) == 1) {
            shots[i].alive = 0;
            hide_sprite(shots[i].spr);
            continue;
        }
        if (aabb(shots[i].x + 2, shots[i].y + 2, 12, 12, P.x, P.y, 12, 14)) {
            hurt_player(shots[i].dmg);
            shots[i].alive = 0;
            hide_sprite(shots[i].spr);
            continue;
        }
        /* slash can destroy shots */
        if (P.attack && aabb(shots[i].x, shots[i].y, 12, 12,
                             P.x + P.fx * 14, P.y + P.fy * 10, 16, 16)) {
            shots[i].alive = 0;
            hide_sprite(shots[i].spr);
        }
        if (shots[i].spr) SPR_setPosition(shots[i].spr, shots[i].x, shots[i].y);
        (void)j;
    }
}

static void explode_at(s16 x, s16 y, u8 rad, u8 dmg, int hurt_self)
{
    u8 i;
    s16 r2 = (s16)rad * rad;
    for (i = 0; i < MAX_EN; i++) if (ens[i].alive) {
        s16 dx = ens[i].x + 6 - x, dy = ens[i].y + 6 - y;
        if (dx * dx + dy * dy <= r2) damage_en(&ens[i], dmg);
    }
    if (hurt_self) {
        s16 dx = P.x + 6 - x, dy = P.y + 6 - y;
        if (dx * dx + dy * dy <= r2) hurt_player(1);
    }
}

static void update_bombs(void)
{
    u8 i;
    for (i = 0; i < MAX_BOMB; i++) if (bombs[i].alive) {
        if (bombs[i].fuse) {
            bombs[i].fuse--;
            if (bombs[i].spr) {
                show_sprite(bombs[i].spr);
                SPR_setFrame(bombs[i].spr, (frames / 8) % 3);
                SPR_setPosition(bombs[i].spr, bombs[i].x, bombs[i].y);
            }
            if (!bombs[i].fuse) {
                bombs[i].boom = 16;
                sfx_play(sfxExplode, sizeof(sfxExplode));
                explode_at(bombs[i].x + 8, bombs[i].y + 8, P.powder ? 36 : 28, P.powder ? 4 : 3, 1);
            }
        } else if (bombs[i].boom) {
            bombs[i].boom--;
            if (!bombs[i].boom) {
                bombs[i].alive = 0;
                hide_sprite(bombs[i].spr);
            }
        }
    }
}

static void update_drops(void)
{
    u8 i;
    for (i = 0; i < MAX_DROP; i++) if (drops[i].alive) {
        drops[i].frame++;
        if (drops[i].spr) {
            u8 fr = 0;
            if (drops[i].kind == DROP_GOLD) fr = (drops[i].frame / 8) % 4;
            else if (drops[i].kind == DROP_HEART) fr = 4;
            else if (drops[i].kind == DROP_KEY) fr = 7;
            else if (drops[i].kind == DROP_GEM) fr = 8;
            else if (drops[i].kind == DROP_FOOD) fr = 9;
            else if (drops[i].kind == DROP_ITEM) fr = (u8)(13 + (drops[i].val % 6));
            SPR_setFrame(drops[i].spr, fr);
            SPR_setPosition(drops[i].spr, drops[i].x, drops[i].y);
            show_sprite(drops[i].spr);
        }
        if (aabb(drops[i].x, drops[i].y, 14, 14, P.x, P.y, 12, 14)) {
            if (drops[i].kind == DROP_GOLD) {
                u8 g = drops[i].val;
                if (P.greed) g += g / 2 + 1;
                P.gold += g;
                P.score += g;
                sfx_play(sfxPickup, sizeof(sfxPickup));
            } else if (drops[i].kind == DROP_HEART) {
                P.hp += 2;
                if (P.hp > P.hpmax) P.hp = P.hpmax;
                sfx_play(sfxPickup, sizeof(sfxPickup));
            } else if (drops[i].kind == DROP_KEY) {
                P.keys++;
                boss_unlocked = 1;
                show_msg("КЛЮЧ ОТ КОРОЛЯ");
                sfx_play(sfxPickup, sizeof(sfxPickup));
                open_doors_now(&rooms[cur_ri]);
            } else if (drops[i].kind == DROP_GEM) {
                P.power = 360;
                show_msg("СИЛА САМОЦВЕТА");
                sfx_play(sfxPower, sizeof(sfxPower));
            } else if (drops[i].kind == DROP_FOOD) {
                P.hunger += 3;
                if (P.hunger > 8) P.hunger = 8;
                sfx_play(sfxPickup, sizeof(sfxPickup));
            } else if (drops[i].kind == DROP_ITEM) {
                give_item(drops[i].val);
            }
            drops[i].alive = 0;
            hide_sprite(drops[i].spr);
        }
    }
}

static void place_bomb(void)
{
    u8 i;
    if (!P.bombs) return;
    for (i = 0; i < MAX_BOMB; i++) if (!bombs[i].alive) {
        bombs[i].alive = 1;
        bombs[i].x = P.x;
        bombs[i].y = P.y;
        bombs[i].fuse = 90;
        bombs[i].boom = 0;
        P.bombs--;
        sfx_play(sfxSwing, sizeof(sfxSwing));
        return;
    }
}

static void do_attack(void)
{
    if (P.attack) return;
    P.attack = 16;
    P.atk_hit = 0;
    sfx_play(sfxSwing, sizeof(sfxSwing));
}

static void update_player(void)
{
    s16 dx = 0, dy = 0;
    s16 sp = P.speed;
    u8 c, dir;
    P.moving = 0;
    if (joy & BUTTON_LEFT) { dx = -sp; P.fx = -1; if (!(joy & (BUTTON_UP | BUTTON_DOWN))) P.fy = 0; }
    if (joy & BUTTON_RIGHT) { dx = sp; P.fx = 1; if (!(joy & (BUTTON_UP | BUTTON_DOWN))) P.fy = 0; }
    if (joy & BUTTON_UP) { dy = -sp; P.fy = -1; if (!(joy & (BUTTON_LEFT | BUTTON_RIGHT))) P.fx = 0; }
    if (joy & BUTTON_DOWN) { dy = sp; P.fy = 1; if (!(joy & (BUTTON_LEFT | BUTTON_RIGHT))) P.fx = 0; }
    if (dx || dy) {
        P.moving = 1;
        try_move_xy(&P.x, &P.y, dx, dy, 12, 14);
        P.x = iclamp(P.x, 0, 308);
        P.y = iclamp(P.y, HUD_H, 208);
    }
    if ((joy & BUTTON_B) && !(joy_prev & BUTTON_B)) do_attack();
    if ((joy & BUTTON_A) && !(joy_prev & BUTTON_A)) do_attack();
    if ((joy & BUTTON_C) && !(joy_prev & BUTTON_C)) place_bomb();

    if (P.attack) {
        s16 hx = P.x + P.fx * 14;
        s16 hy = P.y + P.fy * 8;
        u8 i;
        P.attack--;
        if (sSlash) {
            show_sprite(sSlash);
            SPR_setFrame(sSlash, (u8)((16 - P.attack) / 6));
            SPR_setPosition(sSlash, hx, hy);
            SPR_setHFlip(sSlash, P.fx < 0);
        }
        if (!P.atk_hit && P.attack < 12) {
            for (i = 0; i < MAX_EN; i++) if (ens[i].alive) {
                s16 w = (ens[i].ai == AI_BOSS) ? 24 : 12;
                if (aabb(hx, hy, 18, 16, ens[i].x, ens[i].y, w, 14)) {
                    damage_en(&ens[i], P.dmg);
                    P.atk_hit = 1;
                }
            }
        }
    } else hide_sprite(sSlash);

    if (P.iframes) P.iframes--;
    if (P.power) P.power--;

    P.hung_t++;
    if (P.hunger > 0) {
        if (P.hung_t >= HUNGER_TICK_FRAMES) {
            P.hung_t = 0;
            P.hunger--;
            if (P.hunger == 2) {
                show_msg("ХОЧЕТСЯ ЕСТЬ");
                sfx_play(sfxSelect, sizeof(sfxSelect));
            }
            if (P.hunger == 0) show_msg("ГОЛОД");
        }
    } else if (!grace_t) {
        if (P.hung_t >= STARVE_FRAMES) {
            P.hung_t = 0;
            show_msg("ГОЛОД");
            hurt_player(1);
        }
    }

    /* doors / stairs under feet, or directly in front so a player
       standing on the last floor tile still triggers a door opening */
    c = cell_at(P.x + 6, P.y + 10);
    if (!((c >= C_DOOR && c < C_DOOR + 4) || (c >= C_LOCK && c < C_LOCK + 4) || c == C_STAIRS)) {
        if (P.fx || P.fy)
            c = cell_at(P.x + 6 + (s16)P.fx * 14, P.y + 10 + (s16)P.fy * 12);
    }
    if (c >= C_DOOR && c < C_DOOR + 4) {
        dir = (u8)(c - C_DOOR);
        enter_room((u8)(rooms[cur_ri].gx + ddx[dir]), (u8)(rooms[cur_ri].gy + ddy[dir]), (s16)(dir ^ 1));
    } else if (c >= C_LOCK && c < C_LOCK + 4) {
        if (boss_unlocked) {
            dir = (u8)(c - C_LOCK);
            enter_room((u8)(rooms[cur_ri].gx + ddx[dir]), (u8)(rooms[cur_ri].gy + ddy[dir]), (s16)(dir ^ 1));
        } else if (P.keys) {
            P.keys--;
            boss_unlocked = 1;
            open_doors_now(&rooms[cur_ri]);
            show_msg("ДВЕРЬ ОТКРЫТА");
        } else show_msg("НУЖЕН КЛЮЧ");
    } else if (c == C_STAIRS) {
        next_floor();
    }
}

static void update_sprites_en(void)
{
    u8 i;
    for (i = 0; i < MAX_EN; i++) if (ens[i].alive) {
        u8 fr = ((frames / 8) % 4);
        s16 sy;
        if (ens[i].ai == AI_BOSS) {
            if (sBoss) {
                show_sprite(sBoss);
                SPR_setFrame(sBoss, fr + 4);
                SPR_setPosition(sBoss, ens[i].x - 8, ens[i].y - 16);
                SPR_setHFlip(sBoss, P.x < ens[i].x);
            }
            continue;
        }
        if (!ens[i].spr) continue;
        sy = ens[i].y + (en_is_tall(ens[i].ai) ? -16 : 0);
        show_sprite(ens[i].spr);
        SPR_setFrame(ens[i].spr, fr + 4);
        SPR_setPosition(ens[i].spr, ens[i].x, sy);
        SPR_setHFlip(ens[i].spr, P.x < ens[i].x);
        SPR_setVisibility(ens[i].spr, (ens[i].fear && (frames & 2)) ? HIDDEN : VISIBLE);
    }
}

static void draw_hud(void)
{
    u8 i, hearts, hx = 1;
    hud_clear();
    hearts = (u8)((P.hpmax + 1) / 2);
    if (hearts > 8) hearts = 8;
    for (i = 0; i < hearts; i++) {
        u8 left = (P.hp > i * 2) ? (u8)(P.hp - i * 2) : 0;
        u16 ic = (left >= 2) ? ICON_HEART_F : (left == 1 ? ICON_HEART_H : ICON_HEART_E);
        draw_ch(hx++, 0, ic, PAL3);
    }
    draw_str(12, 0, "ЭТ");
    draw_num(15, 0, floor_n, 1);
    draw_ch(18, 0, ICON_COIN, PAL3);
    draw_num(19, 0, P.gold, 4);
    draw_ch(25, 0, ICON_KEY, PAL3);
    draw_num(26, 0, P.keys, 1);
    draw_ch(29, 0, ICON_BOMB, PAL3);
    draw_num(30, 0, P.bombs, 1);
    draw_str(33, 0, "С");
    draw_num(35, 0, P.score % 100000, 5);

    {
        u8 k;
        u8 warn = (P.hunger <= 2);
        if (warn && (frames & 16)) draw_str(1, 2, P.hunger ? "ЕДА" : "ГОЛОД");
        else draw_str(1, 2, "СЫТ");
        for (k = 0; k < 8; k++)
            draw_ch((u16)(6 + k), 2, (k < P.hunger) ? map_cp((u16)'*') : map_cp((u16)'-'), PAL3);
        if (P.power) draw_str(15, 2, "СИЛА");
        if (msg_t) {
            draw_str(20, 2, msg);
            msg_t--;
        } else {
            if (rooms[cur_ri].type == RT_BOSS) draw_str(22, 2, "КОРОЛЬ");
            if (rooms[cur_ri].type == RT_ITEM) draw_str(22, 2, "ДАРЫ");
        }
    }
}

static void update_player_spr(void)
{
    u8 fr;
    if (!sPlayer) return;
    SPR_setVisibility(sPlayer, VISIBLE);
    if (P.attack) fr = (u8)(8 + (16 - P.attack) / 5);
    else if (P.moving) fr = (u8)(4 + (frames / 6) % 4);
    else fr = (u8)((frames / 12) % 4);
    if (fr > 11) fr = 11;
    if (!(P.iframes && (frames & 2))) {
        SPR_setVisibility(sPlayer, VISIBLE);
        SPR_setFrame(sPlayer, fr);
        SPR_setHFlip(sPlayer, P.fx < 0);
        SPR_setPosition(sPlayer, P.x - 2, P.y - 16);
    } else {
        SPR_setPosition(sPlayer, 336, 240);
    }
}

static void create_sprites(void)
{
    u8 i;
    sPlayer = SPR_addSprite(&sprGnome, 160, 120, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    sSlash = SPR_addSprite(&sprSlash, 336, 240, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
    sBoss = SPR_addSprite(&sprBoss, 336, 240, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    hide_sprite(sSlash);
    hide_sprite(sBoss);
    for (i = 0; i < MAX_EN; i++) {
        ens[i].spr = SPR_addSprite(&sprGoblin, 336, 240, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
        hide_sprite(ens[i].spr);
    }
    for (i = 0; i < MAX_SHOT; i++) {
        shots[i].spr = SPR_addSprite(&sprShot, 336, 240, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
        hide_sprite(shots[i].spr);
    }
    for (i = 0; i < MAX_DROP; i++) {
        drops[i].spr = SPR_addSprite(&sprItems, 336, 240, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
        hide_sprite(drops[i].spr);
    }
    for (i = 0; i < MAX_BOMB; i++) {
        bombs[i].spr = SPR_addSprite(&sprBomb, 336, 240, TILE_ATTR(PAL1, TRUE, FALSE, FALSE));
        hide_sprite(bombs[i].spr);
    }
}

static void start_run(void)
{
    memset(&P, 0, sizeof(P));
    P.hp = 6; P.hpmax = 6;
    P.bombs = 3; P.dmg = 1; P.speed = 2;
    P.fx = 0; P.fy = 1;
    P.hunger = HUNGER_MAX;
    P.hung_t = 0;
    floor_n = 1;
    seed = frames | 1;
    setRandomSeed((u16)seed);
    gen_floor();
    /* place a power gem somewhere on floor 1 start adjacent later via drops */
    enter_room(3, 3, -1);
    add_drop(120, HUD_H + 60, DROP_GEM, 1);
    add_drop(184, HUD_H + 60, DROP_FOOD, 4);
    show_msg("ЭТАЖ");
    state = ST_PLAY;
    if (music_on) XGM2_play(bgmDungeon);
}

static void draw_title_static(void)
{
    u16 attr = TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, logo_base);
    VDP_clearPlane(BG_A, TRUE);
    fill_parallax();
    VDP_drawImageEx(BG_A, &imgLogo, attr, 4, 4, FALSE, TRUE);
    hud_clear();
    draw_str(4, 16, "НАЖМИ СТАРТ");
    draw_str(22, 16, "B КИРКА");
    draw_str(31, 16, "C БОМБА");
    draw_str(8, 20, "РЕКОРД");
    draw_num(16, 20, best_score, 6);
}

static void update_title(void)
{
    scroll++;
    VDP_setHorizontalScroll(BG_B, scroll >> 1);
    VDP_setVerticalScroll(BG_B, scroll >> 3);
    if (sPlayer) {
        show_sprite(sPlayer);
        SPR_setFrame(sPlayer, 4 + (frames / 6) % 4);
        SPR_setPosition(sPlayer, 136 + ((frames / 2) & 31), 176);
        SPR_setHFlip(sPlayer, ((frames / 64) & 1) == 0);
    }
    if ((joy & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C)) &&
        !(joy_prev & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C))) {
        sfx_play(sfxSelect, sizeof(sfxSelect));
        start_run();
    }
}

static void draw_center_panel(const char *title, const char *l2)
{
    hud_clear();
    draw_str(12, 10, title);
    if (l2) draw_str(10, 14, l2);
    draw_str(8, 18, "СЧЁТ");
    draw_num(14, 18, P.score, 6);
    draw_str(8, 22, "РЕКОРД");
    draw_num(16, 22, best_score, 6);
    draw_str(8, 26, "СТАРТ");
}


static void load_tiles(void)
{
    u16 idx = TILE_USER_INDEX;
    font_base = idx;
    VDP_loadTileSet(&tsFont, idx, DMA);
    idx += tsFont.numTile;
    dung_base = idx;
    VDP_loadTileSet(&tsDungeon, idx, DMA);
    idx += tsDungeon.numTile;
    logo_base = idx;
    /* logo tiles loaded on demand via drawImageEx into logo_base */
}

int main(bool hardReset)
{
    u8 i;
    (void)hardReset;
    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setBackgroundColor(1);
    SPR_init();
    JOY_init();
    load_tiles();
    PAL_setPalette(PAL0, palDungeon.data, DMA);
    PAL_setPalette(PAL1, palPlayer.data, DMA);
    PAL_setPalette(PAL2, palEnemy.data, DMA);
    PAL_setPalette(PAL3, palUI.data, DMA);
    create_sprites();
    sram_load();
    XGM2_play(bgmTitle);
    music_on = 1;
    state = ST_TITLE;
    draw_title_static();
    VDP_drawImageEx(BG_A, &imgLogo, TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, logo_base), 4, 5, FALSE, TRUE);
    SPR_update();
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();

    while (TRUE) {
        joy_prev = joy;
        joy = JOY_readJoypad(JOY_1);
        frames++;
        if (state == ST_TITLE) {
            update_title();
        } else if (state == ST_BANNER) {
            state = ST_PLAY;
        } else if (state == ST_PLAY) {
            if ((joy & BUTTON_START) && !(joy_prev & BUTTON_START)) {
                state = ST_PAUSE;
            } else {
                if (grace_t) grace_t--;
                if (flash_t) {
                    VDP_setBackgroundColor((flash_t & 1) ? 5 : 1);
                    flash_t--;
                    if (!flash_t) VDP_setBackgroundColor(1);
                }
                update_player();
                for (i = 0; i < MAX_EN; i++) if (ens[i].alive) update_ai(&ens[i]);
                update_shots();
                update_bombs();
                update_drops();
                check_room_clear();
                update_sprites_en();
                update_player_spr();
                draw_hud();
                scroll = (u16)(P.x / 4);
                VDP_setHorizontalScroll(BG_B, scroll);
                VDP_setVerticalScroll(BG_B, (u16)((P.y - HUD_H) / 8));
            }
        } else if (state == ST_PAUSE) {
            draw_str(16, 12, "ПАУЗА");
            if ((joy & BUTTON_START) && !(joy_prev & BUTTON_START)) state = ST_PLAY;
        } else if (state == ST_DEAD) {
            draw_center_panel("КОНЕЦ ИГРЫ", NULL);
            if ((joy & BUTTON_START) && !(joy_prev & BUTTON_START)) {
                SPR_reset();
                create_sprites();
                XGM2_play(bgmTitle);
                state = ST_TITLE;
                draw_title_static();
                VDP_drawImageEx(BG_A, &imgLogo, TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, logo_base), 4, 5, FALSE, TRUE);
            }
        } else if (state == ST_WIN) {
            draw_center_panel("ПОБЕДА", "ГНОМ ВЫБРАЛСЯ");
            if ((joy & BUTTON_START) && !(joy_prev & BUTTON_START)) {
                SPR_reset();
                create_sprites();
                XGM2_play(bgmTitle);
                state = ST_TITLE;
                draw_title_static();
                VDP_drawImageEx(BG_A, &imgLogo, TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, logo_base), 4, 5, FALSE, TRUE);
            }
        }
        SPR_update();
        SYS_doVBlankProcess();
    }
    return 0;
}
