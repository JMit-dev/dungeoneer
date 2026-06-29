#pragma once
#include "game.h"

void GenerateDungeon(DungeonMap *map, Enemy enemies[], int *enemyCount,
                     int floor, unsigned seed,
                     int *swordMissed, int *potionMissed);
