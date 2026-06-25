#include "pathfind.h"
#include <string.h>

#define MAX_W 64
#define MAX_H 64

typedef struct {
    int  g, f;
    int  px, py;
    bool open;
    bool closed;
} Node;

static Node nodes[MAX_H][MAX_W];

static inline int abs_i(int v) { return v < 0 ? -v : v; }

Vec2i AStarNext(int mapW, int mapH,
                bool (*canPass)(int x, int y, void *ctx), void *ctx,
                Vec2i start, Vec2i goal, int maxRange)
{
    Vec2i invalid = { -1, -1 };

    if (start.x == goal.x && start.y == goal.y) return invalid;
    if (abs_i(goal.x - start.x) + abs_i(goal.y - start.y) > maxRange) return invalid;

    memset(nodes, 0, sizeof(nodes));

    nodes[start.y][start.x].g    = 0;
    nodes[start.y][start.x].f    = abs_i(goal.x - start.x) + abs_i(goal.y - start.y);
    nodes[start.y][start.x].open = true;
    nodes[start.y][start.x].px   = -1;
    nodes[start.y][start.x].py   = -1;

    static const int dx[] = { 0, 0, 1, -1 };
    static const int dy[] = { 1, -1, 0, 0 };

    for (;;) {
        int bx = -1, by = -1, bestF = 0x7fffffff;
        for (int y = 0; y < mapH; y++)
            for (int x = 0; x < mapW; x++)
                if (nodes[y][x].open && nodes[y][x].f < bestF)
                    { bestF = nodes[y][x].f; bx = x; by = y; }

        if (bx < 0) return invalid;

        nodes[by][bx].open   = false;
        nodes[by][bx].closed = true;

        if (bx == goal.x && by == goal.y) {
            int cx = bx, cy = by;
            while (nodes[cy][cx].px != start.x || nodes[cy][cx].py != start.y) {
                int px = nodes[cy][cx].px, py = nodes[cy][cx].py;
                if (px < 0) return invalid;
                cx = px; cy = py;
            }
            return (Vec2i){ cx, cy };
        }

        for (int i = 0; i < 4; i++) {
            int nx = bx + dx[i], ny = by + dy[i];
            if (nx < 0 || nx >= mapW || ny < 0 || ny >= mapH) continue;
            if (nodes[ny][nx].closed) continue;
            bool isGoal = (nx == goal.x && ny == goal.y);
            if (!isGoal && !canPass(nx, ny, ctx)) continue;

            int ng = nodes[by][bx].g + 1;
            int nf = ng + abs_i(goal.x - nx) + abs_i(goal.y - ny);

            if (!nodes[ny][nx].open || ng < nodes[ny][nx].g) {
                nodes[ny][nx].g    = ng;
                nodes[ny][nx].f    = nf;
                nodes[ny][nx].px   = bx;
                nodes[ny][nx].py   = by;
                nodes[ny][nx].open = true;
            }
        }
    }
}
