#ifndef GNOM_DUNGEON_H
#define GNOM_DUNGEON_H

#ifdef HOST_TEST
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
#endif

#define GRID 7
#define MAX_ROOMS 12

#define RT_START 0
#define RT_NORMAL 1
#define RT_ITEM 2
#define RT_BOSS 3

#define HUNGER_MAX 8
#define HUNGER_TICK_FRAMES (40 * 60)
#define STARVE_FRAMES (20 * 60)
#define ROOM_FAILSAFE_FRAMES (90 * 60)
#define TRANSITION_GRACE 48

typedef struct {
    u8 gx, gy, type, doors, cleared, visited, key_drop, used;
} Room;

typedef u16 (*d_rnd_fn)(u16 n);

void dgen_floor(Room *rooms, u8 *room_n, u8 grid[GRID][GRID], d_rnd_fn rnd);
void dfix_floor(Room *rooms, u8 room_n);
int dvalidate_floor(const Room *rooms, u8 room_n, char *err, int errlen);
void dreachable(const Room *rooms, u8 room_n, u8 start, u8 block_boss, u8 *vis);

#endif
