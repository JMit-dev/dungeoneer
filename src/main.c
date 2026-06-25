#define RAYLIB_ASEPRITE_IMPLEMENTATION
#include "game.h"
#include "dungeon.h"
#include "pathfind.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#include <stdlib.h>
#include <time.h>

static GameState game = { 0 };

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

/* ---- pathfinding callback ---- */

typedef struct { int skipIdx; } PfCtx;

static bool EnemyCanPass(int x, int y, void *ctx)
{
    PfCtx *c = (PfCtx *)ctx;
    int t = game.map.terrain[y][x];
    if (t != TILE_FLOOR && t != TILE_STAIRS_UP && t != TILE_STAIRS_DOWN) return false;
    int o = game.map.objects[y][x];
    if (o == TILE_BLOCK || o == TILE_DOOR_LOCKED) return false;
    for (int i = 0; i < game.enemyCount; i++) {
        if (i == c->skipIdx) continue;
        if (game.enemies[i].active &&
            game.enemies[i].x == x && game.enemies[i].y == y) return false;
    }
    return true;
}

/* ---- floor management ---- */

static void StartFloor(void)
{
    unsigned seed = (unsigned)time(NULL) ^ (unsigned)(game.floor * 0x9e3779b9u);
    GenerateDungeon(&game.map, game.enemies, &game.enemyCount, game.floor, seed);
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

static void MoveEnemies(void)
{
    PfCtx ctx;
    for (int i = 0; i < game.enemyCount; i++) {
        Enemy *e = &game.enemies[i];
        if (!e->active) continue;

        int dist = abs(e->x - game.player.x) + abs(e->y - game.player.y);
        if (dist > e->visionRange) continue;

        ctx.skipIdx = i;

        if (dist == 1) {
            game.player.hp--;
            if (game.player.hp <= 0) {
                if (game.score > game.highScore) game.highScore = game.score;
                game.screen = SCREEN_GAMEOVER;
            }
        } else {
            Vec2i next = AStarNext(MAP_W, MAP_H, EnemyCanPass, &ctx,
                                   (Vec2i){ e->x, e->y },
                                   (Vec2i){ game.player.x, game.player.y },
                                   e->visionRange);
            if (next.x >= 0) { e->x = next.x; e->y = next.y; }
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

    for (int i = 0; i < game.enemyCount; i++)
        if (game.enemies[i].active &&
            game.enemies[i].x == nx && game.enemies[i].y == ny) return;

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

static void DrawWorld(void)
{
    int visX   = SCREEN_WIDTH  / TILE_DRAW + 4;
    int visY   = SCREEN_HEIGHT / TILE_DRAW + 4;
    int startX = (int)(game.camera.target.x / TILE_DRAW) - visX / 2;
    int startY = (int)(game.camera.target.y / TILE_DRAW) - visY / 2;

    for (int y = startY; y < startY + visY + 2; y++) {
        for (int x = startX; x < startX + visX + 2; x++) {
            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            int px = x * TILE_DRAW;
            int py = y * TILE_DRAW;

            int terrain = game.map.terrain[y][x];
            int obj     = game.map.objects[y][x];

            DrawTile(terrain, px, py, WHITE);
            if (obj != TILE_NONE) DrawTile(obj, px, py, WHITE);
        }
    }

    for (int i = 0; i < game.enemyCount; i++) {
        Enemy *e = &game.enemies[i];
        if (!e->active) continue;
        Vector2 pos = { (float)(e->x * TILE_DRAW), (float)(e->y * TILE_DRAW) };
        DrawAsepriteEx(game.enemySprite, game.enemyFrame, pos, 0.0f, TILE_SCALE, WHITE);

        int barW = TILE_DRAW - 4;
        int barX = e->x * TILE_DRAW + 2;
        int barY = e->y * TILE_DRAW - 4;
        DrawRectangle(barX, barY, barW, 3, DARKGRAY);
        DrawRectangle(barX, barY, barW * e->hp / e->maxHp, 3, RED);
    }

    Vector2 ppos = { (float)(game.player.x * TILE_DRAW),
                     (float)(game.player.y * TILE_DRAW) };
    DrawAsepriteEx(game.playerSprite, game.playerFrame, ppos, 0.0f, TILE_SCALE, WHITE);
}

static void DrawHUD(void)
{
    DrawRectangle(0, 0, 150, 92, Fade(BLACK, 0.72f));
    int barW = 120, barH = 12;
    DrawRectangle(8, 8, barW, barH, DARKGRAY);
    DrawRectangle(8, 8, barW * game.player.hp / game.player.maxHp, barH, RED);
    DrawText(TextFormat("HP %d/%d", game.player.hp, game.player.maxHp), 10, 9, 10, WHITE);

    DrawText(TextFormat("Floor: %d", game.floor),  8, 24, 14, RAYWHITE);
    DrawText(TextFormat("Score: %d", game.score),  8, 40, 14, RAYWHITE);
    DrawText(TextFormat("Coins: %d", game.player.coins), 8, 56, 14, GOLD);

    if (game.player.hasKey)
        DrawText("[KEY]", 8, 72, 14, YELLOW);

    DrawFPS(SCREEN_WIDTH - 80, 8);
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
            if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) TryMove( 0, -1);
            if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) TryMove( 0,  1);
            if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) TryMove(-1,  0);
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) TryMove( 1,  0);

            float camX = (float)(game.player.x * TILE_DRAW + TILE_DRAW / 2);
            float camY = (float)(game.player.y * TILE_DRAW + TILE_DRAW / 2);
            float halfW = SCREEN_WIDTH  / 2.0f;
            float halfH = SCREEN_HEIGHT / 2.0f;
            float maxCX = (float)(MAP_W * TILE_DRAW) - halfW;
            float maxCY = (float)(MAP_H * TILE_DRAW) - halfH;
            if (camX < halfW)  camX = halfW;
            if (camX > maxCX)  camX = maxCX;
            if (camY < halfH)  camY = halfH;
            if (camY > maxCY)  camY = maxCY;
            game.camera.target = (Vector2){ camX, camY };
            game.camera.offset = (Vector2){ halfW, halfH };
            game.camera.zoom   = 1.0f;

            BeginMode2D(game.camera);
            DrawWorld();
            EndMode2D();
            DrawHUD();
        } break;

        case SCREEN_GAMEOVER: {
            DrawText("GAME OVER",
                     SCREEN_WIDTH/2 - MeasureText("GAME OVER", 60)/2,
                     120, 60, RED);
            DrawText(TextFormat("Score: %d", game.score),
                     SCREEN_WIDTH/2 - MeasureText(TextFormat("Score: %d", game.score), 30)/2,
                     220, 30, WHITE);
            DrawText(TextFormat("Floor reached: %d", game.floor),
                     SCREEN_WIDTH/2 - MeasureText(TextFormat("Floor reached: %d", game.floor), 22)/2,
                     260, 22, LIGHTGRAY);
            if (game.score >= game.highScore && game.score > 0)
                DrawText("New best!",
                         SCREEN_WIDTH/2 - MeasureText("New best!", 24)/2,
                         295, 24, GOLD);
            DrawText("Press ENTER to try again",
                     SCREEN_WIDTH/2 - MeasureText("Press ENTER to try again", 20)/2,
                     360, 20, WHITE);

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
