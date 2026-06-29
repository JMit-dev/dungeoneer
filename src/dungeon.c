#include "dungeon.h"
#include <stdlib.h>
#include <string.h>

#define CELL_W ((MAP_W - 1) / 2)
#define CELL_H ((MAP_H - 1) / 2)
#define CELL_CNT (CELL_W * CELL_H)
#define TILE_CNT (MAP_W * MAP_H)

typedef struct { int x, y; } IV2;

static bool s_visited[CELL_H][CELL_W];
static IV2  s_stack[CELL_CNT];
static IV2  s_bfsQ[TILE_CNT];
static int  s_dist[MAP_H][MAP_W];
static IV2  s_parent[MAP_H][MAP_W];
static IV2  s_path[TILE_CNT];
static bool s_onCrit[MAP_H][MAP_W];

static const int DX4[4] = { 0,  0, 1, -1 };
static const int DY4[4] = {-1,  1, 0,  0 };

static void shuffle4(int out[4]) {
    out[0]=0; out[1]=1; out[2]=2; out[3]=3;
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i+1);
        int t = out[i]; out[i] = out[j]; out[j] = t;
    }
}

static void carve_maze(DungeonMap *map) {
    memset(s_visited, 0, sizeof(s_visited));
    s_visited[0][0] = true;
    map->terrain[1][1] = TILE_FLOOR;
    int top = 0;
    s_stack[top++] = (IV2){0, 0};

    while (top > 0) {
        IV2 cur = s_stack[top-1];
        int dirs[4];
        shuffle4(dirs);
        bool found = false;
        for (int i = 0; i < 4 && !found; i++) {
            int d = dirs[i];
            int ncx = cur.x + DX4[d];
            int ncy = cur.y + DY4[d];
            if (ncx < 0 || ncx >= CELL_W || ncy < 0 || ncy >= CELL_H) continue;
            if (s_visited[ncy][ncx]) continue;
            map->terrain[cur.y*2+1 + DY4[d]][cur.x*2+1 + DX4[d]] = TILE_FLOOR;
            map->terrain[ncy*2+1][ncx*2+1] = TILE_FLOOR;
            s_visited[ncy][ncx] = true;
            s_stack[top++] = (IV2){ncx, ncy};
            found = true;
        }
        if (!found) top--;
    }
}

static void add_loops(DungeonMap *map) {
    /* carve ~20 extra passages to give the player alternate hiding routes */
    int added = 0;
    for (int tries = 0; tries < 600 && added < 20; tries++) {
        int cx = rand() % CELL_W, cy = rand() % CELL_H;
        int d  = rand() % 4;
        int ncx = cx + DX4[d], ncy = cy + DY4[d];
        if (ncx < 0 || ncx >= CELL_W || ncy < 0 || ncy >= CELL_H) continue;
        int wx = cx*2+1 + DX4[d], wy = cy*2+1 + DY4[d];
        if (map->terrain[wy][wx] == TILE_FLOOR) continue;
        map->terrain[wy][wx] = TILE_FLOOR;
        added++;
    }
}

static void do_bfs(DungeonMap *map, int sx, int sy, int bx, int by) {
    memset(s_dist,   -1, sizeof(s_dist));
    memset(s_parent, -1, sizeof(s_parent));
    int front = 0, back = 0;
    s_dist[sy][sx] = 0;
    s_bfsQ[back++] = (IV2){sx, sy};
    while (front < back) {
        IV2 c = s_bfsQ[front++];
        for (int d = 0; d < 4; d++) {
            int nx = c.x + DX4[d], ny = c.y + DY4[d];
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (map->terrain[ny][nx] != TILE_FLOOR) continue;
            if (nx == bx && ny == by) continue;
            if (s_dist[ny][nx] >= 0) continue;
            s_dist[ny][nx] = s_dist[c.y][c.x] + 1;
            s_parent[ny][nx] = c;
            s_bfsQ[back++] = (IV2){nx, ny};
        }
    }
}

static int floor_nbrs(DungeonMap *map, int x, int y) {
    int cnt = 0;
    for (int d = 0; d < 4; d++) {
        int nx = x+DX4[d], ny = y+DY4[d];
        if (nx>=0 && nx<MAP_W && ny>=0 && ny<MAP_H &&
            map->terrain[ny][nx] == TILE_FLOOR) cnt++;
    }
    return cnt;
}

static void try_place_obstacle(DungeonMap *map, int spawnX, int spawnY,
                               int obsX, int obsY, int obsTile,
                               int unlockTile)
{
    if (map->objects[obsY][obsX] != TILE_NONE) return;
    if (obsX == spawnX && obsY == spawnY) return;

    map->objects[obsY][obsX] = obsTile;

    do_bfs(map, spawnX, spawnY, obsX, obsY);

    int bestDist = -1, ukX = -1, ukY = -1;
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (s_dist[y][x] > 0 &&
                floor_nbrs(map, x, y) == 1 &&
                map->objects[y][x] == TILE_NONE &&
                s_dist[y][x] > bestDist)
            {
                bestDist = s_dist[y][x];
                ukX = x; ukY = y;
            }

    if (ukX >= 0)
        map->objects[ukY][ukX] = unlockTile;
    else
        map->objects[obsY][obsX] = TILE_NONE;

    do_bfs(map, spawnX, spawnY, -1, -1);
}

void GenerateDungeon(DungeonMap *map, Enemy enemies[], int *enemyCount,
                     int floor, unsigned seed)
{
    srand(seed);
    *enemyCount = 0;
    map->leverActivated = false;

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            map->terrain[y][x] = (rand()%2) ? TILE_WALL_A : TILE_WALL_B;
            map->objects[y][x] = TILE_NONE;
        }

    carve_maze(map);
    add_loops(map);

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            if (map->terrain[y][x] == TILE_FLOOR) continue;
            for (int d = 0; d < 4; d++) {
                int nx = x+DX4[d], ny = y+DY4[d];
                if (nx>=0 && nx<MAP_W && ny>=0 && ny<MAP_H &&
                    map->terrain[ny][nx] == TILE_FLOOR) {
                    map->terrain[y][x] = TILE_WALL;
                    break;
                }
            }
        }

    int spawnX = 1, spawnY = 1;
    map->spawnX = spawnX;
    map->spawnY = spawnY;
    map->objects[spawnY][spawnX] = TILE_STAIRS_UP;

    do_bfs(map, spawnX, spawnY, -1, -1);

    int maxDist = 0, stairX = spawnX, stairY = spawnY;
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (s_dist[y][x] > maxDist) {
                maxDist = s_dist[y][x];
                stairX = x; stairY = y;
            }
    map->stairsDownX = stairX;
    map->stairsDownY = stairY;
    map->objects[stairY][stairX] = TILE_STAIRS_DOWN;

    int pathLen = 0;
    {
        int cx = stairX, cy = stairY;
        while (cx >= 0 && pathLen < TILE_CNT) {
            s_path[pathLen++] = (IV2){cx, cy};
            IV2 p = s_parent[cy][cx];
            cx = p.x; cy = p.y;
        }
    }
    /* s_path[0]=stairs, s_path[pathLen-1]=spawn */

    if (pathLen > 20) {
        bool doKey   = (rand()%100) < 60;
        bool doLever = !doKey && (rand()%100) < 50;

        if (doKey) {
            int idx = (pathLen-1) * 35 / 100;
            if (idx < 1) idx = 1;
            try_place_obstacle(map, spawnX, spawnY,
                               s_path[idx].x, s_path[idx].y,
                               TILE_DOOR_LOCKED, TILE_KEY);
        } else if (doLever) {
            int idx = (pathLen-1) * 50 / 100;
            if (idx < 1) idx = 1;
            try_place_obstacle(map, spawnX, spawnY,
                               s_path[idx].x, s_path[idx].y,
                               TILE_BLOCK, TILE_LEVER_OFF);
        }
    }

    /* mark critical path so enemies never block the only route to stairs */
    memset(s_onCrit, 0, sizeof(s_onCrit));
    for (int i = 0; i < pathLen; i++)
        s_onCrit[s_path[i].y][s_path[i].x] = true;

    int enemyTarget = 2 + floor + rand()%3;
    if (enemyTarget > MAX_ENEMIES) enemyTarget = MAX_ENEMIES;
    for (int attempt = 0; attempt < 500 && *enemyCount < enemyTarget; attempt++) {
        int x = 1 + rand()%(MAP_W-2);
        int y = 1 + rand()%(MAP_H-2);
        if (map->terrain[y][x] != TILE_FLOOR) continue;
        if (map->objects[y][x] != TILE_NONE)  continue;
        if (s_dist[y][x] < 8) continue;
        if (s_onCrit[y][x]) continue;
        bool dup = false;
        for (int i = 0; i < *enemyCount && !dup; i++)
            if (enemies[i].x == x && enemies[i].y == y) dup = true;
        if (dup) continue;
        int fd = rand() % 4;
        enemies[*enemyCount] = (Enemy){
            .x = x, .y = y,
            .hp = 2 + floor/2, .maxHp = 2 + floor/2,
            .active = true, .visionRange = 10,
            .facingX = DX4[fd], .facingY = DY4[fd],
            .alerted = false, .searchTurns = 0,
        };
        (*enemyCount)++;
    }

    int coinTarget = 3 + rand()%5;
    for (int attempt = 0; attempt < 400 && coinTarget > 0; attempt++) {
        int x = 1 + rand()%(MAP_W-2);
        int y = 1 + rand()%(MAP_H-2);
        if (map->terrain[y][x] == TILE_FLOOR && map->objects[y][x] == TILE_NONE &&
            s_dist[y][x] > 0) {
            map->objects[y][x] = TILE_COIN;
            coinTarget--;
        }
    }

    int chestTarget = 1 + rand()%3;
    for (int y = 0; y < MAP_H && chestTarget > 0; y++)
        for (int x = 0; x < MAP_W && chestTarget > 0; x++)
            if (s_dist[y][x] > 5 && floor_nbrs(map, x, y) == 1 &&
                map->objects[y][x] == TILE_NONE) {
                map->objects[y][x] = TILE_CHEST_CLOSED;
                chestTarget--;
            }

    /* pits on every floor; prefer junction tiles so enemies can fall in tactically */
    int pitTarget = 2 + floor/2 + rand()%3;
    /* first pass: junctions (3+ floor neighbours) — good ambush spots */
    for (int y = 1; y < MAP_H-1 && pitTarget > 0; y++)
        for (int x = 1; x < MAP_W-1 && pitTarget > 0; x++)
            if (map->terrain[y][x] == TILE_FLOOR &&
                map->objects[y][x] == TILE_NONE &&
                floor_nbrs(map, x, y) >= 3 &&
                s_dist[y][x] > 6 &&
                !s_onCrit[y][x] &&
                !(x == stairX && y == stairY)) {
                map->terrain[y][x] = TILE_PIT;
                pitTarget--;
            }
    /* second pass: random floor tiles for the rest */
    for (int attempt = 0; attempt < 400 && pitTarget > 0; attempt++) {
        int x = 1 + rand()%(MAP_W-2);
        int y = 1 + rand()%(MAP_H-2);
        if (map->terrain[y][x] == TILE_FLOOR &&
            map->objects[y][x] == TILE_NONE &&
            s_dist[y][x] > 4 &&
            !(x == stairX && y == stairY)) {
            map->terrain[y][x] = TILE_PIT;
            pitTarget--;
        }
    }
}
