#pragma once

#include "raylib.h"

#ifdef ENABLE_TMX
#define RAYLIB_TMX_IMPLEMENTATION
#include "raylib-tmx.h"
#endif

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// ImageDraw was renamed/removed in raylib 6.x; provide a shim for raylib-aseprite
#ifndef ImageDraw
#define ImageDraw(dst, src, srcRec, dstRec, tint) \
    ImageDrawImagePro((dst), (src), (srcRec), (dstRec), (Vector2){0, 0}, 0.0f, (tint))
#endif

#define RAYLIB_ASEPRITE_IMPLEMENTATION
#include "raylib-aseprite.h"

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 450
#define TARGET_FPS    60
#define GAME_TITLE    "My Game"

typedef enum {
    SCREEN_LOGO,
    SCREEN_TITLE,
    SCREEN_GAMEPLAY,
    SCREEN_ENDING,
} GameScreen;

typedef struct {
    GameScreen screen;
    int        frameCount;
} GameState;
