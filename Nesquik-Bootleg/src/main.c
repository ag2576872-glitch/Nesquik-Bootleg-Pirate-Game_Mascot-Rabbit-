#include <genesis.h>
#include <string.h>   // для strlen

// Центрирует текст по горизонтали (экран 40 тайлов)
void drawCenteredText(const char* text, u16 y) {
    u16 len = strlen(text);
    u16 x = (40 - len) / 2;
    VDP_drawText(text, x, y);
}

int main(bool hardReset) {
    // Делаем текст красным (цвет 15 в PAL0)
    VDP_setPaletteColor(15, RGB24_TO_VDPCOLOR(0xFF0000));

    // Очищаем экран (на всякий случай)
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    // === Твои WARNING-тексты ===
    drawCenteredText("WARNING!", 9);
    drawCenteredText("A VERY SMALL PERCENTAGE", 11);
    drawCenteredText("OF PEOPLE MAY EXPERIENCE", 12);
    drawCenteredText("A SEIZURE WHEN EXPOSED", 13);
    drawCenteredText("TO FLASHING LIGHTS OR", 14);
    drawCenteredText("PATTERNS IN THIS GAME.", 15);

    drawCenteredText("QUIKY SAYS:", 18);
    drawCenteredText("DRINK NESQUIK RESPONSIBLY!", 19);
    drawCenteredText("TOO MUCH CHOCOLATE = TOO MUCH FUN", 21);

    drawCenteredText("PRESS START TO CONTINUE", 25);

    while (TRUE) {
        // Ждём нажатия Start
        if (JOY_readJoypad(JOY_1) & BUTTON_START) {
            break;   // здесь потом переход к title/игре
        }

        SYS_doVBlankProcess();   // обязательно в конце кадра
    }

    // Дальше можно очистить и рисовать следующий экран
    VDP_clearPlane(BG_A, TRUE);
    // ... твой код игры ...
    showSega();

    while (TRUE) {
        SYS_doVBlankProcess();
    }

    return 0;
}