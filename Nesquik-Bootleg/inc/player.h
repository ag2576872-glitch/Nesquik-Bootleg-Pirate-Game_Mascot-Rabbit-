#ifndef PLAYER_H
#define PLAYER_H

#include <genesis.h>

#define PLAYER_W   32          // 4 tiles * 8
#define PLAYER_H   48          // 6 tiles * 8 (подгони под свой спрайт)
#define GRAVITY    FIX16(0.4)
#define MAX_FALL   FIX16(6)
#define JUMP_FORCE FIX16(-7.5)
#define MOVE_SPEED 3

typedef struct {
    Sprite *spr;
    fix16  x, y;
    fix16  vx, vy;
    s16    screenX, screenY;
    bool   onGround;
    s16    facing;             // 1 = right, -1 = left
} Player;

void playerInit(s16 startX, s16 startY);
void playerUpdate(void);
void playerHandleInput(u16 joy, u16 changed, u16 state);

extern Player player;

#endif