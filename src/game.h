#pragma once

#include "raylib.h"

#ifndef ImageDraw
#define ImageDraw(dst, src, srcRec, dstRec, tint) \
    ImageDrawImagePro((dst), (src), (srcRec), (dstRec), (Vector2){0, 0}, 0.0f, (tint))
#endif

#include "raylib-aseprite.h"

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 450
#define TARGET_FPS    60
#define GAME_TITLE    "Dungeoneer"

#define TILE_SIZE   16
#define TILE_SCALE  3
#define TILE_DRAW   (TILE_SIZE * TILE_SCALE)

#define MAP_W       60
#define MAP_H       40
#define MAX_ENEMIES 20
#define MAX_ROOMS   15

#define TILE_NONE          (-1)
#define TILE_FLOOR           0
#define TILE_WALL            1
#define TILE_BLOCK           2
#define TILE_WALL_A          3
#define TILE_WALL_B          4
#define TILE_PIT             5
#define TILE_DOOR_LOCKED     6
#define TILE_DOOR_UNLOCKED   7
#define TILE_STAIRS_UP       8
#define TILE_STAIRS_DOWN     9
#define TILE_LEVER_OFF      10
#define TILE_LEVER_ON       11
#define TILE_CHEST_CLOSED   12
#define TILE_CHEST_OPEN     13
#define TILE_KEY            14
#define TILE_COIN           15

#define SCORE_COIN    10
#define SCORE_CHEST   50
#define SCORE_ENEMY  100
#define SCORE_FLOOR  200

typedef enum {
    SCREEN_LOGO,
    SCREEN_TITLE,
    SCREEN_GAMEPLAY,
    SCREEN_GAMEOVER,
} GameScreen;

typedef struct {
    int x, y;
    int hp, maxHp;
    bool active;
    int visionRange;
} Enemy;

typedef struct {
    int x, y;
    int hp, maxHp;
    bool hasKey;
    int coins;
} Player;

typedef struct {
    int  terrain[MAP_H][MAP_W];
    int  objects[MAP_H][MAP_W];
    int  spawnX, spawnY;
    int  stairsDownX, stairsDownY;
    bool leverActivated;
} DungeonMap;

typedef struct {
    GameScreen screen;
    int        frameCount;
    int        floor;
    int        score;
    int        highScore;
    Player     player;
    Enemy      enemies[MAX_ENEMIES];
    int        enemyCount;
    DungeonMap map;
    Texture2D  tileset;
    Aseprite   playerSprite;
    Aseprite   enemySprite;
    int        playerFrame;
    float      playerAnimTimer;
    int        enemyFrame;
    float      enemyAnimTimer;
    Camera2D   camera;
    char       messageText[64];
    float      messageTimer;
} GameState;
