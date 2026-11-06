#include <genesis.h>
#include "resources.h"
#include "segaintro.h"
// Карнавальные цвета — яркая циклическая палитра
static const u16 carnival_pal[16] = {
    RGB24_TO_VDPCOLOR(0x000000),  // фон — чёрный
    RGB24_TO_VDPCOLOR(0xFF0000),  // красный
    RGB24_TO_VDPCOLOR(0xFF8800),  // оранжевый
    RGB24_TO_VDPCOLOR(0xFFFF00),  // жёлтый
    RGB24_TO_VDPCOLOR(0x00FF00),  // зелёный
    RGB24_TO_VDPCOLOR(0x00FFFF),  // голубой
    RGB24_TO_VDPCOLOR(0x0088FF),  // синий
    RGB24_TO_VDPCOLOR(0xFF00FF),  // пурпурный
    RGB24_TO_VDPCOLOR(0xFFFFFF),  // белый
    RGB24_TO_VDPCOLOR(0xFF4444),
    RGB24_TO_VDPCOLOR(0x44FF44),
    RGB24_TO_VDPCOLOR(0x4444FF),
    RGB24_TO_VDPCOLOR(0xFFAA00),
    RGB24_TO_VDPCOLOR(0xAA00FF),
    RGB24_TO_VDPCOLOR(0x00FFAA),
    RGB24_TO_VDPCOLOR(0xFFFFAA),
};

void showSega(void) {
    // 1. Инициализация
    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);

    // 2. Загружаем палитру (карнавальная!)
    PAL_setPalette(PAL0, carnival_pal, CPU);

    // 3. Загружаем тайлсет логотипа в VRAM
    u16 tile_index = TILE_USER_INDEX;
    VDP_loadTileSet(sega_image.tileset, tile_index, CPU);

    // 4. Рисуем логотип по центру на Plane A
    //    Логотип 128x64 = 16x8 тайлов
    //    Центр экрана: (40-16)/2 = 12 по X, (28-8)/2 = 10 по Y
    VDP_drawImageEx(
        BG_A,
        &sega_image,
        TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tile_index),
        0, 0,   // позиция X, Y в тайлах
        FALSE,
        CPU
    );

    // 5. Запускаем музыку (XGM2)
    XGM_setPCM(64, sega_sfx, sizeof(sega_sfx));
    XGM_startPlayPCM(64, 1, SOUND_PCM_CH2);

    // 6. Эффект появления: fade in с чёрного
    PAL_fadeIn(0, 15, carnival_pal, 30, FALSE);  // 30 фреймов

 bool timer3 = FALSE;
    u32 startFrame = vtimer;
    // 3 секунды: NTSC ≈ 180 кадров, PAL ≈ 150 кадров
    u16 waitFrames = IS_PAL_SYSTEM ? 150 : 180;

    u16 cycle_offset = 0;
    u16 cycled_pal[16];

    while (!timer3) {
        // Циклирование палитры
        if ((vtimer % 4) == 0) {
            cycle_offset = (cycle_offset + 1) % 15;
            cycled_pal[0] = carnival_pal[0];
            for (u16 i = 1; i < 16; i++) {
                cycled_pal[i] = carnival_pal[((i + cycle_offset - 1) % 15) + 1];
            }
            PAL_setPalette(PAL0, cycled_pal, CPU);
        }

        // Проверка таймера
        if ((vtimer - startFrame) >= waitFrames) {
            timer3 = TRUE;
        }

        SYS_doVBlankProcess();   // обязательно!
    }

    // Fade Out после 3 секунд
    PAL_fadeOut(0, 15, 30, FALSE);   // 40 кадров, блокирующий
    // или PAL_fadeOut(0, 15, 40, TRUE); + PAL_waitFadeCompletion();

    // здесь запуск игры
    showTitleScreen();
}