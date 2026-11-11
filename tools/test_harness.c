#define HOST_TEST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/dungeon.h"
#include "../src/dungeon.c"

static uint32_t rng_state = 1;

static u16 hrnd(u16 n)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return n ? (u16)((rng_state >> 16) % n) : 0;
}

static int test_floors(void)
{
    Room rooms[MAX_ROOMS];
    u8 grid[GRID][GRID];
    u8 room_n;
    int s, ok = 0, fail = 0;
    char err[80];

    for (s = 1; s <= 2500; s++) {
        rng_state = (uint32_t)s * 2654435761u + 97u;
        dgen_floor(rooms, &room_n, grid, hrnd);
        if (!dvalidate_floor(rooms, room_n, err, (int)sizeof(err))) {
            fail++;
            if (fail <= 12)
                printf("FAIL seed %d rooms=%u: %s\n", s, (unsigned)room_n, err);
        } else {
            ok++;
        }
    }
    printf("floors: ok=%d fail=%d / 2500\n", ok, fail);
    return fail == 0;
}

static int test_hunger(void)
{
    int hunger = HUNGER_MAX;
    u16 hung_t = 0;
    int hp = 6;
    int grace = 0;
    int f, first_dmg = -1;
    int limit = HUNGER_TICK_FRAMES * HUNGER_MAX + STARVE_FRAMES + 200;

    for (f = 1; f <= limit; f++) {
        hung_t++;
        if (hunger > 0) {
            if (hung_t >= HUNGER_TICK_FRAMES) {
                hung_t = 0;
                hunger--;
            }
        } else if (!grace) {
            if (hung_t >= STARVE_FRAMES) {
                hung_t = 0;
                hp--;
                if (first_dmg < 0) first_dmg = f;
            }
        }
        if (f < HUNGER_TICK_FRAMES * HUNGER_MAX && hp != 6) {
            printf("hunger: damage too early at frame %d (hunger=%d)\n", f, hunger);
            return 0;
        }
    }
    if (first_dmg != HUNGER_TICK_FRAMES * HUNGER_MAX + STARVE_FRAMES) {
        printf("hunger: first damage at %d, expected %d\n",
               first_dmg, HUNGER_TICK_FRAMES * HUNGER_MAX + STARVE_FRAMES);
        return 0;
    }
    printf("hunger: first starve damage at frame %d (%.1f s), hp after tick=%d\n",
           first_dmg, first_dmg / 60.0, hp);
    return hp == 5;
}

static int test_old_hunger_would_have_killed(void)
{
    /* Document that the old 18s/pip + 10s damage would already be lethal
       by the time the new curve first hurts the player. */
    int old_empty = 18 * 60 * 8;
    int old_dead = old_empty + 600 * 6;
    int new_first = HUNGER_TICK_FRAMES * HUNGER_MAX + STARVE_FRAMES;
    printf("timing: old starve-to-death ~%d frames (%.1f s), new first hit %d (%.1f s)\n",
           old_dead, old_dead / 60.0, new_first, new_first / 60.0);
    return new_first > old_dead;
}

int main(void)
{
    int pass = 1;
    if (!test_floors()) pass = 0;
    if (!test_hunger()) pass = 0;
    if (!test_old_hunger_would_have_killed()) pass = 0;
    if (!pass) {
        printf("HARNESS FAILED\n");
        return 1;
    }
    printf("HARNESS OK\n");
    return 0;
}
