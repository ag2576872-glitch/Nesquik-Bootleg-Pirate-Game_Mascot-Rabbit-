#ifndef LEVEL1_COLLISION_H
#define LEVEL1_COLLISION_H
#include <genesis.h>

#define LEVEL1_WIDTH   1600   // ширина уровня в пикселях
#define LEVEL1_HEIGHT  224
#define NO_GROUND      255    // яма: в этой колонке пола нет

// level1_floor[x] = Y верхней точки земли в колонке x (в пикселях)
extern const u8 level1_floor[LEVEL1_WIDTH];

// Y пола под колонкой x (NO_GROUND если яма или за границей уровня)
u8 Level1_getFloorY(s16 x);

#endif
