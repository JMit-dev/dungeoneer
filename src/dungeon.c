#include "dungeon.h"
#include <stdlib.h>
#include <string.h>

typedef struct { int x, y, w, h; } Room;

static int rng_min(int a, int b) { return a < b ? a : b; }
static int rng_max(int a, int b) { return a > b ? a : b; }

static void fill_rect(DungeonMap *map, int x, int y, int w, int h, int tile)
{
    for (int ty = y; ty < y + h; ty++)
        for (int tx = x; tx < x + w; tx++)
            map->terrain[ty][tx] = tile;
}

static void carve_hline(DungeonMap *map, int x1, int x2, int y)
{
    int mn = rng_min(x1, x2), mx = rng_max(x1, x2);
    for (int x = mn; x <= mx; x++)
        map->terrain[y][x] = TILE_FLOOR;
}

static void carve_vline(DungeonMap *map, int x, int y1, int y2)
{
    int mn = rng_min(y1, y2), mx = rng_max(y1, y2);
    for (int y = mn; y <= mx; y++)
        map->terrain[y][x] = TILE_FLOOR;
}

static bool rooms_overlap(Room a, Room b)
{
    return a.x < b.x + b.w + 2 && a.x + a.w + 2 > b.x &&
           a.y < b.y + b.h + 2 && a.y + a.h + 2 > b.y;
}

static bool in_room(int x, int y, Room *rooms, int count)
{
    for (int i = 0; i < count; i++)
        if (x >= rooms[i].x && x < rooms[i].x + rooms[i].w &&
            y >= rooms[i].y && y < rooms[i].y + rooms[i].h)
            return true;
    return false;
}

static void place_object_safe(DungeonMap *map, int x, int y, int tile,
                              int skipX, int skipY)
{
    if (map->terrain[y][x] == TILE_FLOOR &&
        map->objects[y][x] == TILE_NONE &&
        !(x == skipX && y == skipY))
        map->objects[y][x] = tile;
}

void GenerateDungeon(DungeonMap *map, Enemy enemies[], int *enemyCount,
                     int floor, unsigned seed)
{
    srand(seed);
    *enemyCount = 0;
    map->leverActivated = false;

    int bg;
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            bg = (rand() % 2) ? TILE_WALL_A : TILE_WALL_B;
            map->terrain[y][x] = bg;
            map->objects[y][x] = TILE_NONE;
        }

    Room rooms[MAX_ROOMS];
    int  roomCount = 0;
    int  target    = rng_min(5 + floor / 2, MAX_ROOMS);

    for (int attempt = 0; attempt < 300 && roomCount < target; attempt++) {
        Room r;
        r.w = 4 + rand() % 7;
        r.h = 4 + rand() % 5;
        r.x = 1 + rand() % (MAP_W - r.w - 2);
        r.y = 1 + rand() % (MAP_H - r.h - 2);

        bool ok = true;
        for (int i = 0; i < roomCount && ok; i++)
            if (rooms_overlap(r, rooms[i])) ok = false;

        if (ok) {
            fill_rect(map, r.x, r.y, r.w, r.h, TILE_FLOOR);
            rooms[roomCount++] = r;
        }
    }

    if (roomCount < 2) return;

    for (int i = 1; i < roomCount; i++) {
        int x1 = rooms[i-1].x + rooms[i-1].w / 2;
        int y1 = rooms[i-1].y + rooms[i-1].h / 2;
        int x2 = rooms[i].x   + rooms[i].w   / 2;
        int y2 = rooms[i].y   + rooms[i].h   / 2;
        if (rand() % 2) { carve_hline(map, x1, x2, y1); carve_vline(map, x2, y1, y2); }
        else             { carve_vline(map, x1, y1, y2); carve_hline(map, x1, x2, y2); }
    }

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (map->terrain[y][x] == TILE_FLOOR) continue;
            bool adj = false;
            for (int dy = -1; dy <= 1 && !adj; dy++)
                for (int dx = -1; dx <= 1 && !adj; dx++) {
                    int nx = x+dx, ny = y+dy;
                    if (nx>=0 && nx<MAP_W && ny>=0 && ny<MAP_H &&
                        map->terrain[ny][nx] == TILE_FLOOR)
                        adj = true;
                }
            if (adj) map->terrain[y][x] = TILE_WALL;
        }
    }

    int sx = rooms[0].x + rooms[0].w / 2;
    int sy = rooms[0].y + rooms[0].h / 2;
    map->spawnX = sx;
    map->spawnY = sy;
    map->objects[sy][sx] = TILE_STAIRS_UP;

    int last = roomCount - 1;
    int dx_s = rooms[last].x + rooms[last].w / 2;
    int dy_s = rooms[last].y + rooms[last].h / 2;
    map->stairsDownX = dx_s;
    map->stairsDownY = dy_s;
    map->objects[dy_s][dx_s] = TILE_STAIRS_DOWN;

    bool addKey   = roomCount >= 3 && (rand() % 100) < 60;
    bool addLever = roomCount >= 4 && (rand() % 100) < 50;
    bool keyPlaced   = false;
    bool leverPlaced = false;

    for (int i = 1; i < roomCount - 1; i++) {
        Room *r  = &rooms[i];
        int   cx = r->x + r->w / 2;
        int   cy = r->y + r->h / 2;

        int enCount = 1 + (floor > 2 ? rand() % 2 : 0);
        for (int e = 0; e < enCount && *enemyCount < MAX_ENEMIES; e++) {
            int ex, ey;
            int tries = 0;
            do {
                ex = r->x + 1 + rand() % (r->w - 2);
                ey = r->y + 1 + rand() % (r->h - 2);
                tries++;
            } while ((ex == cx && ey == cy) && tries < 10);

            enemies[*enemyCount] = (Enemy){
                .x           = ex,
                .y           = ey,
                .hp          = 2 + floor / 2,
                .maxHp       = 2 + floor / 2,
                .active      = true,
                .visionRange = 10,
            };
            (*enemyCount)++;
        }

        int coins = rand() % 4;
        for (int c = 0; c < coins; c++) {
            int px = r->x + rand() % r->w;
            int py = r->y + rand() % r->h;
            place_object_safe(map, px, py, TILE_COIN, sx, sy);
        }

        if (rand() % 5 == 0)
            place_object_safe(map, cx, cy, TILE_CHEST_CLOSED, sx, sy);

        if (addKey && !keyPlaced && i == 1) {
            if (map->objects[cy][cx] == TILE_NONE) {
                map->objects[cy][cx] = TILE_KEY;
                keyPlaced = true;
            }
        }

        if (addLever && !leverPlaced && i == 2) {
            if (map->objects[cy][cx] == TILE_NONE) {
                map->objects[cy][cx] = TILE_LEVER_OFF;
                leverPlaced = true;
            }
        }

        if (floor >= 2) {
            int pits = rand() % 3;
            for (int p = 0; p < pits; p++) {
                int px = r->x + 1 + rand() % (r->w - 2);
                int py = r->y + 1 + rand() % (r->h - 2);
                if (map->terrain[py][px] == TILE_FLOOR &&
                    map->objects[py][px] == TILE_NONE &&
                    !(px == sx && py == sy))
                    map->terrain[py][px] = TILE_PIT;
            }
        }
    }

    if (addKey && keyPlaced) {
        for (int attempt = 0; attempt < 300; attempt++) {
            int px = 1 + rand() % (MAP_W - 2);
            int py = 1 + rand() % (MAP_H - 2);
            if (map->terrain[py][px] == TILE_FLOOR &&
                !in_room(px, py, rooms, roomCount) &&
                map->objects[py][px] == TILE_NONE)
            {
                map->objects[py][px] = TILE_DOOR_LOCKED;
                break;
            }
        }
    }

    if (addLever && leverPlaced) {
        int blocks = 1 + rand() % 3;
        for (int b = 0; b < blocks; b++) {
            for (int attempt = 0; attempt < 300; attempt++) {
                int px = 1 + rand() % (MAP_W - 2);
                int py = 1 + rand() % (MAP_H - 2);
                if (map->terrain[py][px] == TILE_FLOOR &&
                    !in_room(px, py, rooms, roomCount) &&
                    map->objects[py][px] == TILE_NONE &&
                    !(px == sx && py == sy) &&
                    !(px == dx_s && py == dy_s))
                {
                    map->objects[py][px] = TILE_BLOCK;
                    break;
                }
            }
        }
    }
}
