#include "game.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

static GameState game = { 0 };

static void UpdateDrawFrame(void);

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, GAME_TITLE);
    InitAudioDevice();
    SetTargetFPS(TARGET_FPS);

    game.screen = SCREEN_LOGO;

    // --- Load assets ---
    // Aseprite sprite (all platforms):
    //   Aseprite sprite = LoadAseprite("resources/player.aseprite");
    //   AsepriteTag walkTag = LoadAsepriteTag(sprite, "Walk");

#ifdef ENABLE_TMX
    // Tiled map (desktop only):
    //   tmx_map *map = LoadTMX("resources/level1.tmx");
#endif

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, TARGET_FPS, 1);
#else
    while (!WindowShouldClose())
        UpdateDrawFrame();
#endif

    // --- Unload assets ---
    // UnloadAseprite(sprite);
#ifdef ENABLE_TMX
    // UnloadTMX(map);
#endif

    CloseAudioDevice();
    CloseWindow();
    return 0;
}

static void UpdateDrawFrame(void)
{
    game.frameCount++;

    // --- Update ---
    // UpdateAsepriteTag(&walkTag);

    // --- Draw ---
    BeginDrawing();
    ClearBackground(RAYWHITE);

    switch (game.screen)
    {
        case SCREEN_LOGO:
        {
            DrawText("LOGO SCREEN", 20, 20, 40, LIGHTGRAY);
            DrawText("(transition after 2 seconds)", 20, 70, 20, GRAY);

            if (game.frameCount > TARGET_FPS * 2)
            {
                game.screen = SCREEN_TITLE;
                game.frameCount = 0;
            }
        } break;

        case SCREEN_TITLE:
        {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GREEN);
            DrawText("TITLE SCREEN", 20, 20, 40, DARKGREEN);
            DrawText("Press ENTER to start", 20, 70, 20, DARKGREEN);

            if (IsKeyPressed(KEY_ENTER))
            {
                game.screen = SCREEN_GAMEPLAY;
                game.frameCount = 0;
            }
        } break;

        case SCREEN_GAMEPLAY:
        {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, PURPLE);
            DrawText("GAMEPLAY SCREEN", 20, 20, 40, MAROON);

            // Draw sprite:
            //   DrawAseprite(sprite, 0, 100, 100, WHITE);
            // Draw animated tag:
            //   DrawAsepriteTag(walkTag, 100, 100, WHITE);

#ifdef ENABLE_TMX
            // Draw map:
            //   DrawTMX(map, 0, 0, WHITE, 1.0f);
#endif

            if (IsKeyPressed(KEY_ENTER))
            {
                game.screen = SCREEN_ENDING;
                game.frameCount = 0;
            }
        } break;

        case SCREEN_ENDING:
        {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLUE);
            DrawText("ENDING SCREEN", 20, 20, 40, DARKBLUE);
            DrawText("Press ENTER to restart", 20, 70, 20, DARKBLUE);

            if (IsKeyPressed(KEY_ENTER))
            {
                game.screen = SCREEN_LOGO;
                game.frameCount = 0;
            }
        } break;

        default: break;
    }

    DrawFPS(SCREEN_WIDTH - 90, 10);
    EndDrawing();
}
