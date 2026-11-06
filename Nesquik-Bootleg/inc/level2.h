#ifndef LEVEL2_H
#define LEVEL2_H

#include <genesis.h>

// Function to show the boss level for level 2: Grizzly Bear vs Quicky (Nesquik Mascot)
void showBossLevel(void);

// Boss states
typedef enum {
    BOSS_IDLE = 0,
    BOSS_WALK = 1,
    BOSS_ATTACK = 2,
    BOSS_HURT = 3,
    BOSS_DEAD
} BossState;

// Player (Quicky) states
typedef enum {
    PLAYER_IDLE = 0,
    PLAYER_WALK = 1,
    PLAYER_JUMP = 4,
    PLAYER_ATTACK = 8
} PlayerState;

#endif // LEVEL2_H