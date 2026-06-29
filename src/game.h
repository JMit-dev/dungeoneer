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
#define TILE_SWORD          16
#define TILE_POTION         17
#define TILE_IND_ALERT      18  /* ! */
#define TILE_IND_SEARCH     19  /* ? */
#define TILE_IND_CALM       20  /* ... */

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
    int  x, y;
    int  hp, maxHp;
    bool active;
    int  visionRange;
    int  facingX, facingY;  /* current heading, one of (±1,0) or (0,±1) */
    bool alerted;           /* actively chasing player                   */
    int  searchTurns;       /* >0: lost sight, walking in last direction  */
    int  patrolTimer;       /* ticks until next patrol step; -1=stationary */
} Enemy;

typedef struct {
    int  x, y;
    int  hp, maxHp;
    bool hasKey;
    bool hasSword;
    int  coins;
} Player;

typedef struct {
    int  terrain[MAP_H][MAP_W];
    int  objects[MAP_H][MAP_W];
    bool explored[MAP_H][MAP_W];
    int  spawnX, spawnY;
    int  stairsDownX, stairsDownY;
    bool leverActivated;
    int  activeW, activeH;   /* actual carved area; tiles outside are dead wall */
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
    float      shakeTimer;
    int        swordMissedFloors;
    int        potionMissedFloors;
    Color      palBg;   /* background / clear color for current floor */
    Color      palFg;   /* foreground tint for lit tiles and sprites  */
} GameState;
