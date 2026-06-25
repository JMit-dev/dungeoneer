#pragma once
#include <stdbool.h>

typedef struct { int x, y; } Vec2i;

/* Returns the next grid position to step toward goal, or {-1,-1} if unreachable. */
Vec2i AStarNext(int mapW, int mapH,
                bool (*canPass)(int x, int y, void *ctx), void *ctx,
                Vec2i start, Vec2i goal, int maxRange);
