#define RAYLIB_ASEPRITE_IMPLEMENTATION
#include "game.h"
#include "dungeon.h"
#include "pathfind.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LIGHT_INNER 1   /* fully-lit Chebyshev radius (3x3 area) */
#define LIGHT_OUTER 4   /* explored/fading outer radius           */

static GameState game     = { 0 };
static bool s_visible[MAP_H][MAP_W];

#define MOVE_INTERVAL 0.20f   /* seconds between auto-steps */
static int   s_moveDir[2]  = {0, 0};
static float s_moveTick    = 0.0f;
static int   s_swordDirX   = 1;   /* direction the held sword faces */
static int   s_swordDirY   = 0;

static void UpdateDrawFrame(void);

/* ---- helpers ---- */

static void DrawTile(int tileId, int px, int py, Color tint)
{
    if (tileId < 0) return;
    int col = tileId % 8;
    int row = tileId / 8;
    Rectangle src = { (float)(col * TILE_SIZE), (float)(row * TILE_SIZE),
                      (float)TILE_SIZE, (float)TILE_SIZE };
    Rectangle dst = { (float)px, (float)py, (float)TILE_DRAW, (float)TILE_DRAW };
    DrawTexturePro(game.tileset, src, dst, (Vector2){0, 0}, 0.0f, tint);
}

/* draw a tile centered at world pixel (cx, cy), optionally rotated */
static void DrawTileCentered(int id, int cx, int cy, int size, float rot, Color tint)
{
    if (id < 0) return;
    int col = id % 8, row = id / 8;
    Rectangle src = { (float)(col * TILE_SIZE), (float)(row * TILE_SIZE),
                      (float)TILE_SIZE,          (float)TILE_SIZE };
    float half = size * 0.5f;
    Rectangle dst = { (float)cx, (float)cy, (float)size, (float)size };
    DrawTexturePro(game.tileset, src, dst, (Vector2){half, half}, rot, tint);
}

/* ---- fog of war ---- */

static void ComputeVisibility(void)
{
    int px = game.player.x, py = game.player.y;
    memset(s_visible, 0, sizeof(s_visible));
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            int dx = abs(x - px), dy = abs(y - py);
            int dist = dx > dy ? dx : dy;   /* Chebyshev distance */
            if (dist <= LIGHT_OUTER) {
                s_visible[y][x]       = true;
                game.map.explored[y][x] = true;
            }
        }
    }
}

/* ---- pathfinding callback ---- */

typedef struct { int skipIdx; } PfCtx;

static bool EnemyCanPass(int x, int y, void *ctx)
{
    PfCtx *c = (PfCtx *)ctx;
    int t = game.map.terrain[y][x];
    if (t != TILE_FLOOR && t != TILE_PIT) return false;
    int o = game.map.objects[y][x];
    if (o == TILE_BLOCK || o == TILE_DOOR_LOCKED) return false;
    for (int i = 0; i < game.enemyCount; i++) {
        if (i == c->skipIdx) continue;
        if (game.enemies[i].active &&
            game.enemies[i].x == x && game.enemies[i].y == y) return false;
    }
    return true;
}

/* corridor vision: enemy sees along the exact line they face, up to the
   first wall, blocked door, or the vision range limit. */
static bool CanSeePlayer(Enemy *e)
{
    int rx = game.player.x - e->x;
    int ry = game.player.y - e->y;
    int fwd = rx * e->facingX + ry * e->facingY;
    if (fwd <= 0 || fwd > e->visionRange) return false;
    /* player must be on the exact same axis (no lateral offset) */
    if (rx * e->facingY - ry * e->facingX != 0) return false;
    /* check no blocking tile between enemy and player */
    for (int s = 1; s < fwd; s++) {
        int tx = e->x + s * e->facingX;
        int ty = e->y + s * e->facingY;
        if (game.map.terrain[ty][tx] != TILE_FLOOR) return false;
        int o = game.map.objects[ty][tx];
        if (o == TILE_BLOCK || o == TILE_DOOR_LOCKED) return false;
    }
    return true;
}

/* ---- floor management ---- */

static void StartFloor(void)
{
    s_moveDir[0] = s_moveDir[1] = 0;
    s_moveTick   = 0.0f;
    s_swordDirX  = 1; s_swordDirY = 0;
    memset(game.map.explored, 0, sizeof(game.map.explored));
    unsigned seed = (unsigned)time(NULL) ^ (unsigned)(game.floor * 0x9e3779b9u);
    GenerateDungeon(&game.map, game.enemies, &game.enemyCount, game.floor, seed,
                    &game.swordMissedFloors, &game.potionMissedFloors);
    game.player.x = game.map.spawnX;
    game.player.y = game.map.spawnY;
}

static void NewGame(void)
{
    game.floor     = 1;
    game.score     = 0;
    game.player    = (Player){ .hp = 10, .maxHp = 10 };
    StartFloor();
}

/* ---- enemy turn ---- */

static void HurtPlayer(void)
{
    game.player.hp--;
    game.shakeTimer = 0.30f;
    if (game.player.hp <= 0) {
        if (game.score > game.highScore) game.highScore = game.score;
        game.screen = SCREEN_GAMEOVER;
    }
}

static void EnemyLandCheck(Enemy *e)
{
    if (game.map.terrain[e->y][e->x] == TILE_PIT) {
        e->active = false;
        game.score += SCORE_ENEMY;
    }
}

static void MoveEnemies(void)
{
    PfCtx ctx;
    for (int i = 0; i < game.enemyCount; i++) {
        Enemy *e = &game.enemies[i];
        if (!e->active) continue;
        ctx.skipIdx = i;

        int mdist = abs(e->x - game.player.x) + abs(e->y - game.player.y);
        bool sees  = CanSeePlayer(e);

        if (e->alerted) {
            /* ---- ALERT: chase with A* ---- */
            if (!sees && mdist > 1) {
                e->alerted     = false;
                e->searchTurns = 4;   /* lost sight → search */
            } else if (mdist == 1) {
                HurtPlayer();
            } else {
                Vec2i next = AStarNext(MAP_W, MAP_H, EnemyCanPass, &ctx,
                                       (Vec2i){e->x, e->y},
                                       (Vec2i){game.player.x, game.player.y},
                                       e->visionRange);
                if (next.x >= 0) {
                    int dx = next.x - e->x, dy = next.y - e->y;
                    if (dx || dy) { e->facingX = dx; e->facingY = dy; }
                    e->x = next.x; e->y = next.y;
                    EnemyLandCheck(e);
                }
            }
        } else if (e->searchTurns > 0) {
            /* ---- SEARCHING: walk straight in last facing direction ---- */
            if (sees) {
                e->alerted     = true;
                e->searchTurns = 0;
            } else {
                int nx = e->x + e->facingX, ny = e->y + e->facingY;
                if (nx >= 0 && nx < MAP_W && ny >= 0 && ny < MAP_H &&
                    EnemyCanPass(nx, ny, &ctx)) {
                    e->x = nx; e->y = ny;
                    EnemyLandCheck(e);
                }
                if (abs(game.player.x-e->x)+abs(game.player.y-e->y) == 1)
                    HurtPlayer();
                e->searchTurns--;
                if (e->searchTurns == 0) e->searchTurns = -8; /* → calming */
            }
        } else if (e->searchTurns < 0) {
            /* ---- CALMING: stand still briefly, show "..." ---- */
            if (sees) { e->alerted = true; e->searchTurns = 0; }
            else       { e->searchTurns++; }
        } else {
            /* ---- DORMANT: patrol corridor and watch for player ---- */
            /* patrolTimer == -1 means stationary; otherwise move every tick */
            if (e->patrolTimer >= 0) {
                /* priority: forward → turn (L/R shuffled) → backward */
                int fx = e->facingX, fy = e->facingY;
                int dirs[4][2] = {
                    { fx,  fy},   /* forward          */
                    { fy, -fx},   /* 90° left  (CCW)  */
                    {-fy,  fx},   /* 90° right (CW)   */
                    {-fx, -fy},   /* 180° back        */
                };
                if (rand()%2) {  /* shuffle the two side-turns for variety */
                    int tx = dirs[1][0], ty = dirs[1][1];
                    dirs[1][0] = dirs[2][0]; dirs[1][1] = dirs[2][1];
                    dirs[2][0] = tx;         dirs[2][1] = ty;
                }
                for (int d = 0; d < 4; d++) {
                    int dx = dirs[d][0], dy = dirs[d][1];
                    int nx = e->x + dx,  ny = e->y + dy;
                    bool ok = (nx>=0 && nx<MAP_W && ny>=0 && ny<MAP_H &&
                               game.map.terrain[ny][nx] == TILE_FLOOR &&
                               game.map.objects[ny][nx] == TILE_NONE);
                    for (int j = 0; j < game.enemyCount && ok; j++)
                        if (j!=i && game.enemies[j].active &&
                            game.enemies[j].x==nx && game.enemies[j].y==ny) ok=false;
                    if (ok) {
                        e->facingX = dx; e->facingY = dy;
                        e->x = nx;       e->y = ny;
                        EnemyLandCheck(e);
                        break;
                    }
                }
            }
            /* check vision from current position (after any patrol step) */
            if (CanSeePlayer(e)) e->alerted = true;
        }
    }
}

/* ---- player turn ---- */

static void TryMove(int dx, int dy)
{
    if (game.screen != SCREEN_GAMEPLAY) return;

    int nx = game.player.x + dx;
    int ny = game.player.y + dy;
    if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) return;

    for (int i = 0; i < game.enemyCount; i++) {
        Enemy *e = &game.enemies[i];
        if (!e->active || e->x != nx || e->y != ny) continue;
        if (game.player.hasSword) {
            e->active             = false;
            game.score           += SCORE_ENEMY;
            game.player.hasSword  = false;
            MoveEnemies();
        }
        return;
    }

    int terrain = game.map.terrain[ny][nx];
    int obj     = game.map.objects[ny][nx];

    if (terrain == TILE_WALL || terrain == TILE_WALL_A || terrain == TILE_WALL_B) return;
    if (terrain == TILE_NONE) return;

    if (obj == TILE_BLOCK) return;

    if (obj == TILE_DOOR_LOCKED) {
        if (game.player.hasKey) {
            game.map.objects[ny][nx] = TILE_DOOR_UNLOCKED;
            game.player.hasKey = false;
            MoveEnemies();
        }
        return;
    }

    game.player.x = nx;
    game.player.y = ny;

    if (terrain == TILE_PIT) {
        if (game.score > game.highScore) game.highScore = game.score;
        game.screen = SCREEN_GAMEOVER;
        return;
    }

    switch (obj) {
        case TILE_COIN:
            game.map.objects[ny][nx] = TILE_NONE;
            game.score += SCORE_COIN;
            game.player.coins++;
            break;
        case TILE_KEY:
            game.map.objects[ny][nx] = TILE_NONE;
            game.player.hasKey = true;
            break;
        case TILE_CHEST_CLOSED:
            game.map.objects[ny][nx] = TILE_CHEST_OPEN;
            game.score += SCORE_CHEST;
            game.player.coins += 5;
            break;
        case TILE_SWORD:
            game.map.objects[ny][nx] = TILE_NONE;
            game.player.hasSword = true;
            break;
        case TILE_POTION:
            game.map.objects[ny][nx] = TILE_NONE;
            game.player.hp = game.player.maxHp;
            break;
        case TILE_LEVER_OFF:
            game.map.objects[ny][nx] = TILE_LEVER_ON;
            game.map.leverActivated = true;
            for (int y = 0; y < MAP_H; y++)
                for (int x = 0; x < MAP_W; x++)
                    if (game.map.objects[y][x] == TILE_BLOCK)
                        game.map.objects[y][x] = TILE_NONE;
            break;
        case TILE_DOOR_UNLOCKED:
            game.map.objects[ny][nx] = TILE_NONE;
            break;
        case TILE_STAIRS_DOWN:
            game.score += SCORE_FLOOR * game.floor;
            game.floor++;
            game.player.hp = (game.player.hp + 2 <= game.player.maxHp)
                              ? game.player.hp + 2 : game.player.maxHp;
            StartFloor();
            return;
        default: break;
    }

    MoveEnemies();
}

/* ---- rendering ---- */

/* small filled triangle showing which way an enemy faces */
static void DrawFacingArrow(int ex, int ey, int fdx, int fdy, Color c)
{
    float cx = ex + TILE_DRAW * 0.5f;
    float cy = ey + TILE_DRAW * 0.5f;
    float px = -(float)fdy, py = (float)fdx; /* CCW perpendicular */
    Vector2 tip = { cx + fdx*14.0f,          cy + fdy*14.0f          };
    Vector2 bl  = { cx + fdx*5.0f + px*6.0f, cy + fdy*5.0f + py*6.0f };
    Vector2 br  = { cx + fdx*5.0f - px*6.0f, cy + fdy*5.0f - py*6.0f };
    DrawTriangle(tip, br, bl, c);
}

static void DrawWorld(void)
{
    int visX   = SCREEN_WIDTH  / TILE_DRAW + 4;
    int visY   = SCREEN_HEIGHT / TILE_DRAW + 4;
    int startX = (int)(game.camera.target.x / TILE_DRAW) - visX / 2;
    int startY = (int)(game.camera.target.y / TILE_DRAW) - visY / 2;

    static const Color DIM = { 70, 70, 90, 255 };
    int plx = game.player.x, ply = game.player.y;

    for (int y = startY; y < startY + visY + 2; y++) {
        for (int x = startX; x < startX + visX + 2; x++) {
            int px = x * TILE_DRAW;
            int py = y * TILE_DRAW;

            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) {
                int dx = (x < 0) ? -x : (x >= MAP_W) ? x - MAP_W + 1 : 0;
                int dy = (y < 0) ? -y : (y >= MAP_H) ? y - MAP_H + 1 : 0;
                int b  = 48 - (dx + dy) * 16;
                if (b <= 0) continue;
                int tile = ((x ^ y) & 1) ? TILE_WALL_A : TILE_WALL_B;
                DrawTile(tile, px, py, (Color){b, b, b+8, 255});
                continue;
            }

            if (!game.map.explored[y][x]) continue;

            Color tint;
            if (s_visible[y][x]) {
                int cdx  = abs(x - plx), cdy = abs(y - ply);
                int dist = cdx > cdy ? cdx : cdy;
                if (dist <= LIGHT_INNER) {
                    tint = WHITE;
                } else {
                    float t = (float)(dist - LIGHT_INNER) /
                              (float)(LIGHT_OUTER - LIGHT_INNER);
                    tint = (Color){
                        (unsigned char)(255 + (DIM.r - 255) * t),
                        (unsigned char)(255 + (DIM.g - 255) * t),
                        (unsigned char)(255 + (DIM.b - 255) * t),
                        255
                    };
                }
            } else {
                tint = DIM;
            }

            int terrain = game.map.terrain[y][x];
            int obj     = game.map.objects[y][x];
            DrawTile(terrain, px, py, tint);
            if (obj != TILE_NONE) DrawTile(obj, px, py, tint);
        }
    }

    for (int i = 0; i < game.enemyCount; i++) {
        Enemy *e = &game.enemies[i];
        if (!e->active || !s_visible[e->y][e->x]) continue;
        Vector2 pos = { (float)(e->x * TILE_DRAW), (float)(e->y * TILE_DRAW) };
        DrawAsepriteEx(game.enemySprite, game.enemyFrame, pos, 0.0f, TILE_SCALE, WHITE);

        /* facing arrow — only shown when dormant or calming, not during chase/search */
        if (!e->alerted && e->searchTurns <= 0)
            DrawFacingArrow(e->x * TILE_DRAW, e->y * TILE_DRAW,
                            e->facingX, e->facingY, (Color){255, 255, 255, 180});

        /* state indicator above enemy */
        int indId = -1;
        if      (e->alerted)          indId = TILE_IND_ALERT;
        else if (e->searchTurns > 0)  indId = TILE_IND_SEARCH;
        else if (e->searchTurns < 0)  indId = TILE_IND_CALM;
        if (indId >= 0) {
            int sz  = TILE_DRAW * 3 / 4;
            int icx = e->x * TILE_DRAW + TILE_DRAW / 2;
            int icy = e->y * TILE_DRAW - sz / 2;
            DrawTileCentered(indId, icx, icy, sz, 0.0f, WHITE);
        }
    }

    Vector2 ppos = { (float)(game.player.x * TILE_DRAW),
                     (float)(game.player.y * TILE_DRAW) };
    DrawAsepriteEx(game.playerSprite, game.playerFrame, ppos, 0.0f, TILE_SCALE, WHITE);

    /* held sword sticks out in current movement direction */
    if (game.player.hasSword) {
        float rot = 0.0f;
        if      (s_swordDirX ==  1) rot =   0.0f;
        else if (s_swordDirX == -1) rot = 180.0f;
        else if (s_swordDirY == -1) rot = 270.0f;
        else if (s_swordDirY ==  1) rot =  90.0f;
        int scx = (game.player.x + s_swordDirX) * TILE_DRAW + TILE_DRAW / 2;
        int scy = (game.player.y + s_swordDirY) * TILE_DRAW + TILE_DRAW / 2;
        DrawTileCentered(TILE_SWORD, scx, scy, TILE_DRAW, rot, WHITE);
    }
}

static void DrawHUD(void)
{
    DrawRectangle(0, 0, 144, 68, Fade(BLACK, 0.72f));

    /* HP bar */
    DrawRectangle(8, 6, 128, 10, DARKGRAY);
    DrawRectangle(8, 6, 128 * game.player.hp / game.player.maxHp, 10, RED);
    const char *hpStr = TextFormat("%d/%d", game.player.hp, game.player.maxHp);
    DrawText(hpStr, 72 - MeasureText(hpStr, 9)/2, 7, 9, WHITE);

    /* score */
    DrawText(TextFormat("SCORE  %d", game.score), 8, 21, 14, RAYWHITE);

    /* inventory icons — key left, sword right */
    if (game.player.hasKey)   DrawTileCentered(TILE_KEY,    20, 50, 24, 0.0f, YELLOW);
    if (game.player.hasSword) DrawTileCentered(TILE_SWORD, 124, 50, 24, 0.0f, WHITE);
}

static void DrawMinimap(void)
{
    enum { MS = 2 };                               /* pixels per tile        */
    const int MW = MAP_W * MS;                     /* 120                    */
    const int MH = MAP_H * MS;                     /* 80                     */
    const int MX = SCREEN_WIDTH  - MW - 8;
    const int MY = 8;

    DrawRectangle(MX - 2, MY - 2, MW + 4, MH + 4, Fade(BLACK, 0.75f));

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (!game.map.explored[y][x]) continue;

            bool vis     = s_visible[y][x];
            int  terrain = game.map.terrain[y][x];
            int  obj     = game.map.objects[y][x];
            int  px      = MX + x * MS;
            int  py      = MY + y * MS;

            Color tc;
            if (terrain == TILE_FLOOR || terrain == TILE_PIT)
                tc = vis ? (Color){160,160,185,255} : (Color){75,75,90,255};
            else
                tc = vis ? (Color){80,80,95,255}    : (Color){45,45,55,255};
            DrawRectangle(px, py, MS, MS, tc);

            if (!vis || obj == TILE_NONE) continue;
            Color oc;
            switch (obj) {
                case TILE_STAIRS_DOWN:  oc = SKYBLUE;                    break;
                case TILE_STAIRS_UP:    oc = (Color){100,210,100,255};   break;
                case TILE_DOOR_LOCKED:  oc = ORANGE;                     break;
                case TILE_KEY:          oc = YELLOW;                     break;
                case TILE_CHEST_CLOSED: oc = GOLD;                       break;
                default:                continue;
            }
            DrawRectangle(px, py, MS, MS, oc);
        }
    }

    for (int i = 0; i < game.enemyCount; i++) {
        Enemy *e = &game.enemies[i];
        if (!e->active || !s_visible[e->y][e->x]) continue;
        DrawRectangle(MX + e->x * MS, MY + e->y * MS, MS, MS, RED);
    }

    DrawRectangle(MX + game.player.x * MS, MY + game.player.y * MS, MS, MS, WHITE);
}

/* ---- main loop ---- */

static int AsepriteFrameCount(Aseprite s)
{
    if (!IsAsepriteValid(s)) return 1;
    int w = GetAsepriteWidth(s);
    return w > 0 ? GetAsepriteTexture(s).width / w : 1;
}

static void UpdateDrawFrame(void)
{
    game.frameCount++;
    float dt = GetFrameTime();

    game.playerAnimTimer += dt;
    if (game.playerAnimTimer > 0.15f) {
        game.playerAnimTimer = 0;
        int fc = AsepriteFrameCount(game.playerSprite);
        game.playerFrame = (game.playerFrame + 1) % fc;
    }
    game.enemyAnimTimer += dt;
    if (game.enemyAnimTimer > 0.20f) {
        game.enemyAnimTimer = 0;
        int fc = AsepriteFrameCount(game.enemySprite);
        game.enemyFrame = (game.enemyFrame + 1) % fc;
    }
    if (game.shakeTimer > 0.0f) game.shakeTimer -= dt;

    BeginDrawing();
    ClearBackground(BLACK);

    switch (game.screen) {
        case SCREEN_LOGO: {
            int tw = MeasureText("DUNGEONEER", 70);
            DrawText("DUNGEONEER", SCREEN_WIDTH/2 - tw/2, SCREEN_HEIGHT/2 - 35, 70, WHITE);
            if (game.frameCount > TARGET_FPS * 2) {
                game.screen = SCREEN_TITLE;
                game.frameCount = 0;
            }
        } break;

        case SCREEN_TITLE: {
            int tw = MeasureText("DUNGEONEER", 70);
            DrawText("DUNGEONEER", SCREEN_WIDTH/2 - tw/2, SCREEN_HEIGHT/2 - 60, 70, WHITE);
            DrawText("PRESS ENTER",
                     SCREEN_WIDTH/2 - MeasureText("PRESS ENTER", 18)/2,
                     SCREEN_HEIGHT/2 + 30, 18, GRAY);
            if (game.highScore > 0)
                DrawText(TextFormat("BEST  %d", game.highScore),
                         SCREEN_WIDTH/2 - MeasureText(TextFormat("BEST  %d", game.highScore), 14)/2,
                         SCREEN_HEIGHT/2 + 60, 14, DARKGRAY);

            if (IsKeyPressed(KEY_ENTER)) {
                NewGame();
                game.screen = SCREEN_GAMEPLAY;
                game.frameCount = 0;
            }
        } break;

        case SCREEN_GAMEPLAY: {
            /* direction input — pressing a key sets direction and moves immediately */
            {
                int ndx = 0, ndy = 0;
                if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) { ndx= 0; ndy=-1; }
                if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) { ndx= 0; ndy= 1; }
                if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) { ndx=-1; ndy= 0; }
                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { ndx= 1; ndy= 0; }
                if (ndx || ndy) {
                    s_moveDir[0] = ndx;
                    s_moveDir[1] = ndy;
                    s_swordDirX  = ndx;
                    s_swordDirY  = ndy;
                    TryMove(ndx, ndy);
                    s_moveTick = MOVE_INTERVAL;
                }
            }
            /* auto-move tick — keep walking in current direction */
            if ((s_moveDir[0] || s_moveDir[1]) && game.screen == SCREEN_GAMEPLAY) {
                s_moveTick -= dt;
                if (s_moveTick <= 0.0f) {
                    TryMove(s_moveDir[0], s_moveDir[1]);
                    s_moveTick += MOVE_INTERVAL;
                }
            }

            float halfW = SCREEN_WIDTH  / 2.0f;
            float halfH = SCREEN_HEIGHT / 2.0f;
            game.camera.target = (Vector2){
                (float)(game.player.x * TILE_DRAW + TILE_DRAW / 2),
                (float)(game.player.y * TILE_DRAW + TILE_DRAW / 2)
            };
            game.camera.zoom = 1.0f;
            float shakeX = 0.0f, shakeY = 0.0f;
            if (game.shakeTimer > 0.0f) {
                float mag = game.shakeTimer * 20.0f;
                shakeX = (float)GetRandomValue(-100, 100) / 100.0f * mag;
                shakeY = (float)GetRandomValue(-100, 100) / 100.0f * mag;
            }
            game.camera.offset = (Vector2){ halfW + shakeX, halfH + shakeY };

            ComputeVisibility();
            BeginMode2D(game.camera);
            DrawWorld();
            EndMode2D();
            DrawHUD();
            DrawMinimap();
        } break;

        case SCREEN_GAMEOVER: {
            ClearBackground(BLACK);
            DrawText("GAME OVER",
                     SCREEN_WIDTH/2 - MeasureText("GAME OVER", 60)/2,
                     80, 60, RED);

            int cx = SCREEN_WIDTH/2 - 80, sy = 180, gap = 30;
            DrawText(TextFormat("Floor    %d",  game.floor),         cx, sy,         22, LIGHTGRAY);
            DrawText(TextFormat("Score    %d",  game.score),         cx, sy + gap,   22, WHITE);
            DrawText(TextFormat("Coins    %d",  game.player.coins),  cx, sy + gap*2, 22, GOLD);

            if (game.score > 0 && game.score >= game.highScore)
                DrawText("NEW BEST!",
                         SCREEN_WIDTH/2 - MeasureText("NEW BEST!", 28)/2,
                         sy + gap*3 + 12, 28, GOLD);
            else if (game.highScore > 0)
                DrawText(TextFormat("Best  %d", game.highScore),
                         SCREEN_WIDTH/2 - MeasureText(TextFormat("Best  %d", game.highScore), 18)/2,
                         sy + gap*3 + 16, 18, DARKGRAY);

            DrawText("PRESS ENTER",
                     SCREEN_WIDTH/2 - MeasureText("PRESS ENTER", 20)/2,
                     370, 20, GRAY);

            if (IsKeyPressed(KEY_ENTER)) {
                game.screen = SCREEN_TITLE;
                game.frameCount = 0;
            }
        } break;

        default: break;
    }

    EndDrawing();
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, GAME_TITLE);
    InitAudioDevice();
    SetTargetFPS(TARGET_FPS);

    game.tileset     = LoadTexture("resources/dungeoneer-tileset.png");
    game.playerSprite = LoadAseprite("resources/player.aseprite");
    game.enemySprite  = LoadAseprite("resources/enemy.aseprite");
    game.screen      = SCREEN_LOGO;
    game.frameCount  = 0;

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, TARGET_FPS, 1);
#else
    while (!WindowShouldClose())
        UpdateDrawFrame();
#endif

    UnloadAseprite(game.playerSprite);
    UnloadAseprite(game.enemySprite);
    UnloadTexture(game.tileset);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
