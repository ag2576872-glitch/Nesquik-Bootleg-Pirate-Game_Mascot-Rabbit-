#include <genesis.h>
#include "resources.h"
#include "titlescreen.h"

#define BLINK_SPEED 30

void showTitleScreen(void)
{
    u32 frame;
    u16 blinkState = 0;

    VDP_clearPlane(BG_B, TRUE);
    VDP_clearPlane(BG_A, TRUE);

    XGM_startPlay(title_xgm);
    VDP_drawImage(BG_A, &title_image, 0, 0);

    while(1)
    {
        // Меняем состояние каждые 30 кадров
        if ((frame % BLINK_SPEED) == 0)
        {
            blinkState ^= 1;

            if (blinkState)
            {
                VDP_drawText("PRESS START BUTTON", 11, 24);
            }
            else
            {
                VDP_clearTextLine(24);
            }
        }

        // Проверяем кнопку START
        if (JOY_readJoypad(JOY_1) & BUTTON_START)
        {
            PAL_fadeOutAll(10, FALSE);
            VDP_clearPlane(BG_A, TRUE);
            blinkState = 0;
            XGM_stopPlay();
            // Здесь запуск меню или игры
            showLevel1();
        }
        SYS_doVBlankProcess();
    }
}