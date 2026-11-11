#ifdef HOST_TEST
#include <string.h>
#include "dungeon.h"
#else
#include <genesis.h>
#include "dungeon.h"
#endif

static const s16 DDX[4] = {0, 0, -1, 1};
static const s16 DDY[4] = {-1, 1, 0, 0};

static s16 diabs(s16 v) { return v < 0 ? -v : v; }

static Room *find_xy(const Room *rooms, u8 room_n, s16 gx, s16 gy)
{
    u8 i;
    if (gx < 0 || gy < 0 || gx >= GRID || gy >= GRID) return NULL;
    for (i = 0; i < room_n; i++)
        if (rooms[i].used && rooms[i].gx == (u8)gx && rooms[i].gy == (u8)gy)
            return (Room *)&rooms[i];
    return NULL;
}

void dreachable(const Room *rooms, u8 room_n, u8 start, u8 block_boss, u8 *vis)
{
    u8 q[MAX_ROOMS];
    u8 qh = 0, qt = 0, d;
    memset(vis, 0, MAX_ROOMS);
    if (start >= room_n || !rooms[start].used) return;
    vis[start] = 1;
    q[qt++] = start;
    while (qh < qt) {
        u8 i = q[qh++];
        for (d = 0; d < 4; d++) {
            Room *nb;
            u8 j;
            if (!(rooms[i].doors & (u8)(1 << d))) continue;
            nb = find_xy(rooms, room_n, (s16)rooms[i].gx + DDX[d], (s16)rooms[i].gy + DDY[d]);
            if (!nb) continue;
            if (block_boss && (nb->type == RT_BOSS || rooms[i].type == RT_BOSS)) continue;
            j = (u8)(nb - rooms);
            if (!vis[j]) {
                vis[j] = 1;
                q[qt++] = j;
            }
        }
    }
}

static void set_err(char *err, int errlen, const char *s)
{
    int i = 0;
    if (!err || errlen <= 0) return;
    while (s[i] && i < errlen - 1) { err[i] = s[i]; i++; }
    err[i] = 0;
}

int dvalidate_floor(const Room *rooms, u8 room_n, char *err, int errlen)
{
    u8 vis[MAX_ROOMS];
    u8 i, nused = 0, nvis = 0, key_i = 255, boss_i = 255, item_n = 0, start_ok = 0;
    if (room_n < 4 || room_n > MAX_ROOMS) {
        set_err(err, errlen, "room count");
        return 0;
    }
    if (!rooms[0].used || rooms[0].type != RT_START) {
        set_err(err, errlen, "no start");
        return 0;
    }
    dreachable(rooms, room_n, 0, 0, vis);
    for (i = 0; i < room_n; i++) {
        u8 d;
        if (!rooms[i].used) continue;
        nused++;
        if (vis[i]) nvis++;
        if (rooms[i].type == RT_START) start_ok = 1;
        if (rooms[i].type == RT_BOSS) boss_i = i;
        if (rooms[i].type == RT_ITEM) item_n++;
        if (rooms[i].key_drop) key_i = i;
        for (d = 0; d < 4; d++) {
            if (!(rooms[i].doors & (u8)(1 << d))) continue;
            if (!find_xy(rooms, room_n, (s16)rooms[i].gx + DDX[d], (s16)rooms[i].gy + DDY[d])) {
                set_err(err, errlen, "door to nowhere");
                return 0;
            }
        }
    }
    if (!start_ok) { set_err(err, errlen, "no start"); return 0; }
    if (nvis != nused) { set_err(err, errlen, "disconnected"); return 0; }
    if (boss_i == 255) { set_err(err, errlen, "no boss"); return 0; }
    if (item_n < 1) { set_err(err, errlen, "no item"); return 0; }
    if (key_i == 255 || rooms[key_i].type != RT_NORMAL) {
        set_err(err, errlen, "no key room");
        return 0;
    }
    /* stairs live in the boss room after the fight — the room itself must exist
       and be on the unlocked graph (already checked). Key must be reachable
       without crossing the boss, otherwise the floor softlocks. */
    dreachable(rooms, room_n, 0, 1, vis);
    if (!vis[key_i]) { set_err(err, errlen, "key behind boss"); return 0; }
    return 1;
}

void dfix_floor(Room *rooms, u8 room_n)
{
    u8 vis[MAX_ROOMS];
    u8 i, key_i = 255, pick = 255;
    dreachable(rooms, room_n, 0, 1, vis);
    for (i = 0; i < room_n; i++) {
        if (rooms[i].key_drop) key_i = i;
        rooms[i].key_drop = 0;
    }
    for (i = 1; i < room_n; i++) {
        if (rooms[i].used && rooms[i].type == RT_NORMAL && vis[i]) {
            pick = i;
            break;
        }
    }
    if (pick == 255) {
        /* last resort: any reachable non-start, non-boss room */
        for (i = 1; i < room_n; i++) {
            if (rooms[i].used && rooms[i].type != RT_BOSS && vis[i]) {
                pick = i;
                break;
            }
        }
    }
    if (pick == 255) pick = (key_i != 255) ? key_i : 1;
    if (pick < room_n) rooms[pick].key_drop = 1;
}

void dgen_floor(Room *rooms, u8 *room_n, u8 grid[GRID][GRID], d_rnd_fn rnd)
{
    u8 i, ntarget, cx, cy, furthest, fur_d, item_i;
    u16 guard;
    memset(grid, 0, (size_t)GRID * GRID);
    memset(rooms, 0, sizeof(Room) * MAX_ROOMS);
    *room_n = 0;
    cx = 3; cy = 3;
    rooms[0].gx = cx; rooms[0].gy = cy; rooms[0].type = RT_START; rooms[0].used = 1;
    grid[cy][cx] = 1;
    *room_n = 1;
    ntarget = 8 + (u8)rnd(5);
    if (ntarget > MAX_ROOMS) ntarget = MAX_ROOMS;
    guard = 0;
    while (*room_n < ntarget && guard++ < 400) {
        u8 dir = (u8)rnd(4);
        s16 nx = (s16)cx + DDX[dir];
        s16 ny = (s16)cy + DDY[dir];
        if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID) continue;
        cx = (u8)nx; cy = (u8)ny;
        if (!grid[cy][cx]) {
            grid[cy][cx] = (u8)(*room_n + 1);
            rooms[*room_n].gx = cx;
            rooms[*room_n].gy = cy;
            rooms[*room_n].type = RT_NORMAL;
            rooms[*room_n].used = 1;
            (*room_n)++;
        }
    }
    furthest = 1; fur_d = 0;
    for (i = 1; i < *room_n; i++) {
        u8 d = (u8)(diabs((s16)rooms[i].gx - 3) + diabs((s16)rooms[i].gy - 3));
        if (d > fur_d) { fur_d = d; furthest = i; }
    }
    rooms[furthest].type = RT_BOSS;
    item_i = 1;
    fur_d = 0;
    for (i = 1; i < *room_n; i++) {
        u8 d;
        if (i == furthest) continue;
        d = (u8)(diabs((s16)rooms[i].gx - 3) + diabs((s16)rooms[i].gy - 3));
        if (d >= fur_d) { fur_d = d; item_i = i; }
    }
    rooms[item_i].type = RT_ITEM;
    for (i = 1; i < *room_n; i++) {
        if (rooms[i].type == RT_NORMAL) { rooms[i].key_drop = 1; break; }
    }
    for (i = 0; i < *room_n; i++) {
        u8 d;
        rooms[i].doors = 0;
        rooms[i].cleared = (rooms[i].type == RT_START || rooms[i].type == RT_ITEM) ? 1 : 0;
        rooms[i].visited = 0;
        for (d = 0; d < 4; d++) {
            s16 nx = (s16)rooms[i].gx + DDX[d];
            s16 ny = (s16)rooms[i].gy + DDY[d];
            if (nx >= 0 && ny >= 0 && nx < GRID && ny < GRID && grid[ny][nx])
                rooms[i].doors |= (u8)(1 << d);
        }
    }
    dfix_floor(rooms, *room_n);
}
