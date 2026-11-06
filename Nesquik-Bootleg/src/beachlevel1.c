#include <genesis.h>
#include "resources.h"
#include "beachlevel1.h"
#include "level1_collision.h"

#define SFX_ATTACK 64    // ID от 64 и выше (0–63 зарезервированы под музыку)

static void doLevelComplete(void);


// ---- настройки ----
#define SCREEN_W      320
#define SPR_W         48          // кадр 6 тайлов
#define SPR_H         56          // кадр 7 тайлов
#define START_X       40
#define START_Y       161
#define RUN_SPEED     2           // обычный бег
#define DASH_RUN      4           // даш (Z + направление) — можешь поставить 3 или 5
#define MAX_STEP      8
#define DEATH_Y       260
#define GRAVITY       FIX16(0.35)
#define JUMP_SPEED    FIX16(-7.4)
#define MAX_FALL      FIX16(7.0)

#define ATTACK_DURATION  20       // сколько кадров длится атака (~0.3 сек)


// строки спрайт-листа quicky.png
#define ANIM_IDLE     0
#define ANIM_RUN      1
#define ANIM_DASH     2           // ← если у тебя даш-анимация другая — поменяй номер
#define ANIM_JUMP     4
#define ANIM_ATTACK   8           // атака как в оригинальном Nesquik-bootleg


u8 Level1_getFloorY(s16 x)
{
    if (x < 0 || x >= LEVEL1_WIDTH) return NO_GROUND;
    return level1_floor[x];
}


static Map *levelMap;
static Map *BgbMapLevel;
static Sprite *player;

static s16  px;
static s16  py;
static fix16 vy;
static bool onGround;
static bool faceLeft;

static s16  attackTimer;          // >0 = сейчас атакуем


static void respawn(void)
{
    px = START_X;
    py = START_Y;
    vy = 0;
    onGround = TRUE;
    attackTimer = 0;
}


static void updatePlayer(void)
{
    u16 j = JOY_readJoypad(JOY_1);
    s16 dx = 0;

    // --- атака (X) ---
    // запускаем только если ещё не атакуем
    if ((j & BUTTON_X) && attackTimer == 0)
    {
        attackTimer = ATTACK_DURATION;
        // Загружаем PCM в драйвер
        XGM_setPCM(SFX_ATTACK, sfx_attack, sizeof(sfx_attack));
        XGM_startPlayPCM(SFX_ATTACK, 1, SOUND_PCM_CH2);
    }

    if (attackTimer > 0)
    {
        attackTimer--;
        // во время атаки можно запретить движение, если хочешь:
        // dx = 0;
        // иначе оставляем возможность ходить/прыгать
    }

    // --- горизонталь ---
    s16 speed = RUN_SPEED;
    if (j & BUTTON_Z)               // Z зажат → даш
        speed = DASH_RUN;

    if (j & BUTTON_LEFT)  { dx = -speed; faceLeft = TRUE;  }
    if (j & BUTTON_RIGHT) { dx =  speed; faceLeft = FALSE; }

    // во время атаки можно оставить движение или заблокировать:
    // if (attackTimer > 0) dx = 0;

    if (dx != 0)
    {
        s16 nx = px + dx;
        u8 f = Level1_getFloorY(nx);
        if (nx < 0 || nx >= LEVEL1_WIDTH) { /* граница */ }
        else if (f != NO_GROUND && f < py - MAX_STEP) { /* стена */ }
        else px = nx;
    }

    // ---------- прыжок ----------
    if (onGround && (j & (BUTTON_A | BUTTON_B | BUTTON_C)))
    {
        vy = JUMP_SPEED;
        onGround = FALSE;
    }

    // ---------- вертикаль + пол ----------
    u8 floorY = Level1_getFloorY(px);

    if (onGround)
    {
        if (floorY == NO_GROUND)
            onGround = FALSE;
        else if (floorY >= py - MAX_STEP && floorY <= py + MAX_STEP)
            py = floorY;
        else
            onGround = FALSE;
    }

    if (!onGround)
    {
        vy += GRAVITY;
        if (vy > MAX_FALL) vy = MAX_FALL;
        s16 newY = py + fix16ToInt(vy);

        if (vy >= 0 && floorY != NO_GROUND && py <= floorY && newY >= floorY)
        {
            py = floorY;
            vy = 0;
            onGround = TRUE;
        }
        else
            py = newY;
    }

    if (py > DEATH_Y) respawn();
}


static void drawPlayer(s16 camX)
{
    SPR_setPosition(player, px - camX - SPR_W / 2, py - SPR_H);
    SPR_setHFlip(player, faceLeft);

    // приоритет анимаций: атака > прыжок > даш/бег > idle
    if (attackTimer > 0)
    {
        SPR_setAnim(player, ANIM_ATTACK);
    }
    else if (!onGround)
    {
        SPR_setAnim(player, ANIM_JUMP);
    }
    else if (JOY_readJoypad(JOY_1) & (BUTTON_LEFT | BUTTON_RIGHT))
    {
        // если зажат Z — даш-анимация, иначе обычный бег
        if (JOY_readJoypad(JOY_1) & BUTTON_Z)
            SPR_setAnim(player, ANIM_DASH);
        else
            SPR_setAnim(player, ANIM_RUN);
    }
    else
    {
        SPR_setAnim(player, ANIM_IDLE);
        SPR_setFrame(player, 0);
    }
}

#define MAX_ENEMIES 16

typedef struct {
    Sprite *spr;
    s16 x, y;
    s16 vx;
    fix16 vy;           // ← скорость по вертикали
    bool onGround;      // ← стоит ли на полу
    u8  health;
    u8  type;
    bool alive;
    bool faceLeft;
} Enemy;

static Enemy enemies[MAX_ENEMIES];
static u8 enemyCount = 0;

void initEnemies(void) {
    // Палитра один раз
    PAL_setPalette(PAL2, human_green.palette->data, CPU);

    // ===== Враг 1 =====
    enemies[0].x = 34;
    enemies[0].y = 80;
    enemies[0].vx = 1;
    enemies[0].vy = 0;
    enemies[0].onGround = FALSE;
    enemies[0].health = 2;
    enemies[0].type = 1;
    enemies[0].alive = TRUE;
    enemies[0].faceLeft = FALSE;
    enemies[0].spr = SPR_addSprite(&human_green, 0, 0, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    SPR_setAnim(enemies[0].spr, 0);

    // ===== Враг 2 =====
    enemies[1].x = 120;
    enemies[1].y = 80;
    enemies[1].vx = -1;          // идёт в другую сторону
    enemies[1].vy = 0;
    enemies[1].onGround = FALSE;
    enemies[1].health = 2;
    enemies[1].type = 1;
    enemies[1].alive = TRUE;
    enemies[1].faceLeft = TRUE;
    enemies[1].spr = SPR_addSprite(&hyena, 0, 0, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    SPR_setAnim(enemies[1].spr, 0);

    // ===== Враг 3 =====
    enemies[2].x = 200;
    enemies[2].y = 64;           // чуть выше
    enemies[2].vx = 1;
    enemies[2].vy = 0;
    enemies[2].onGround = FALSE;
    enemies[2].health = 2;
    enemies[2].type = 1;
    enemies[2].alive = TRUE;
    enemies[2].faceLeft = FALSE;
    enemies[2].spr = SPR_addSprite(&hyena, 0, 0, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    SPR_setAnim(enemies[2].spr, 0);

    // ===== Враг 4 =====
    enemies[3].x = 280;
    enemies[3].y = 96;
    enemies[3].vx = -1;
    enemies[3].vy = 0;
    enemies[3].onGround = FALSE;
    enemies[3].health = 2;
    enemies[3].type = 1;
    enemies[3].alive = TRUE;
    enemies[3].faceLeft = TRUE;
    enemies[3].spr = SPR_addSprite(&human_green, 0, 0, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
    SPR_setAnim(enemies[3].spr, 0);

    enemyCount = 4;
}

void updateEnemies(s16 camX) {
    for (u8 i = 0; i < enemyCount; i++) {
        if (!enemies[i].alive) continue;

        Enemy *e = &enemies[i];

        // --- Горизонтальное движение + проверка края ---
        s16 newX = e->x + e->vx;

        // Не даём уйти за карту
        if (newX < 8) {
            newX = 8;
            e->vx = -e->vx;
        } else if (newX > LEVEL1_WIDTH - 8) {
            newX = LEVEL1_WIDTH - 8;
            e->vx = -e->vx;
        }

        // Проверка: есть ли пол под новой позицией (чтобы не свалиться с края сразу)
        u8 floorAtNewX = Level1_getFloorY(newX);
        if (e->onGround && floorAtNewX == NO_GROUND) {
            // Край платформы — разворачиваемся
            e->vx = -e->vx;
            newX = e->x;          // остаёмся на месте
        } else {
            e->x = newX;
        }

        e->faceLeft = (e->vx < 0);

        // --- Вертикаль + гравитация (усиленная проверка) ---
        u8 floorY = Level1_getFloorY(e->x);

        if (!e->onGround) {
            e->vy += GRAVITY;
            if (e->vy > MAX_FALL) e->vy = MAX_FALL;

            s16 newY = e->y + fix16ToInt(e->vy);

            // Жёсткая посадка на пол (не даём проскочить)
            if (floorY != NO_GROUND && newY >= floorY && e->y <= floorY + 4) {
                e->y = floorY;
                e->vy = 0;
                e->onGround = TRUE;
            } else {
                e->y = newY;
            }
        } else {
            // Стоим на земле — постоянно «прилипаем» к полу
            if (floorY == NO_GROUND) {
                e->onGround = FALSE;          // сошли с платформы
            } else {
                // Если пол рядом — привязываемся к нему
                if (abs(floorY - e->y) <= MAX_STEP + 2) {
                    e->y = floorY;
                } else {
                    e->onGround = FALSE;      // пол слишком далеко — падаем
                }
            }
        }

        // Смерть при падении слишком низко
        if (e->y > DEATH_Y) {
            e->alive = FALSE;
            SPR_setVisibility(e->spr, HIDDEN);
            continue;
        }

        // --- Отрисовка ---
        SPR_setHFlip(e->spr, e->faceLeft);
        SPR_setPosition(e->spr, 
                        e->x - camX - 16,   // подгони под свой спрайт
                        e->y - 32);

        // Анимации
        if (!e->onGround)
            SPR_setAnim(e->spr, 1);         // падение / прыжок
        else if (e->vx != 0)
            SPR_setAnim(e->spr, 1);         // ходьба
        else
            SPR_setAnim(e->spr, 0);         // idle
    }
}

// В updatePlayer() или отдельной функции, вызывай каждый кадр
static void checkAttackHits(void) {
    if (attackTimer <= 0) return;

    // Хитбокс атаки игрока (подгони под свой спрайт Nesquik)
    s16 atkX = faceLeft ? (px - 40) : (px + 10);
    s16 atkY = py - 40;
    s16 atkW = 40;
    s16 atkH = 32;

    for (u8 i = 0; i < enemyCount; i++) {
        if (!enemies[i].alive) continue;

        // AABB проверка
        s16 ex = enemies[i].x - 12;
        s16 ey = enemies[i].y - 28;
        s16 ew = 24;
        s16 eh = 28;

        if (atkX < ex + ew && atkX + atkW > ex &&
            atkY < ey + eh && atkY + atkH > ey) {
            
            enemies[i].health--;
            // Отбрасывание
            enemies[i].x += faceLeft ? -8 : 8;

            if (enemies[i].health == 0) {
                enemies[i].alive = FALSE;
                SPR_setVisibility(enemies[i].spr, HIDDEN);
                // Можно поставить анимацию смерти: SPR_setAnim(..., DEATH_ANIM);
            }
        }
    }
}

// ---- Cocoa counter ----
static u8 cocoaCount = 0;

static void drawCocoaCounter(void)
{
    static const char *digits[] =
    {
        "0", "1", "2", "3", "4"
    };

    // Заголовок

    VDP_setTextPalette(PAL1);

    VDP_drawText("COCOA: ", 1, 1);

    // Количество собранных стаканов
    VDP_drawText(digits[cocoaCount], 8, 1);

    // Максимальное количество
    VDP_drawText("/4", 9, 1);
}

static void collectCocoa(void)
{
    if (cocoaCount < 4)
    {
        cocoaCount++;
        drawCocoaCounter();
    }
}

// ---- Cocoa collectible ----
#define COCOA_W 32
#define COCOA_H 32

#define COCOA_SPAWN_X 220

static Sprite *cocoaSprite;

static s16 cocoaX;
static s16 cocoaY;

static bool cocoaTaken = FALSE;

static void initCocoa(void)
{
    cocoaTaken = FALSE;

    cocoaX = COCOA_SPAWN_X;

    // Находим поверхность земли
    u8 floorY = Level1_getFloorY(cocoaX);

    if (floorY != NO_GROUND)
        cocoaY = floorY - COCOA_H;
    else
        cocoaY = 79;

    cocoaSprite = SPR_addSprite(
        &cocoa_sprite,
        cocoaX,
        cocoaY,
        TILE_ATTR(PAL2, TRUE, FALSE, FALSE)
    );

    SPR_setVisibility(cocoaSprite, VISIBLE);
}

static void checkCocoaCollision(void)
{
    if (cocoaTaken)
        return;

    // Границы персонажа
    s16 playerLeft = px - SPR_W / 2;
    s16 playerRight = playerLeft + SPR_W;

    s16 playerTop = py - SPR_H;
    s16 playerBottom = py;

    // Границы стакана
    s16 cocoaLeft = cocoaX;
    s16 cocoaRight = cocoaX + COCOA_W;

    s16 cocoaTop = cocoaY;
    s16 cocoaBottom = cocoaY + COCOA_H;

    // Проверка столкновения
    if (playerLeft < cocoaRight &&
        playerRight > cocoaLeft &&
        playerTop < cocoaBottom &&
        playerBottom > cocoaTop)
    {
        // Стакан подобран!
        cocoaTaken = TRUE;

        SPR_setVisibility(cocoaSprite, HIDDEN);

        collectCocoa();

        // Здесь можно добавить звук подбора
        XGM_setPCM(64, drink_cocoa, sizeof(drink_cocoa));
        XGM_startPlayPCM(64, 1, SOUND_PCM_CH2);
    }
}

static void drawCocoa(s16 camX)
{
    if (cocoaTaken)
        return;

    SPR_setPosition(
        cocoaSprite,
        cocoaX - camX,
        cocoaY
    );
}

// ---- Cocoa 2 collectible ----
static Sprite *cocoaSprite2;

static s16 cocoa2X;
static s16 cocoa2Y;

static bool cocoa2Taken = FALSE;

static void initCocoa2(void)
{
    cocoa2Taken = FALSE;

    // Позиция второго стакана
    cocoa2X = 420;

    // Определяем высоту платформы
    u8 floorY = Level1_getFloorY(cocoa2X);

    if (floorY != NO_GROUND)
        cocoa2Y = floorY - COCOA_H;
    else
        cocoa2Y = 69;

    // Создаём второй стакан
    cocoaSprite2 = SPR_addSprite(
        &cocoa_sprite,
        cocoa2X,
        cocoa2Y,
        TILE_ATTR(PAL2, TRUE, FALSE, FALSE)
    );

    SPR_setVisibility(cocoaSprite2, VISIBLE);
}

static void checkCocoa2Collision(void)
{
    if (cocoa2Taken)
        return;

    // Персонаж
    s16 playerLeft = px - SPR_W / 2;
    s16 playerRight = playerLeft + SPR_W;

    s16 playerTop = py - SPR_H;
    s16 playerBottom = py;

    // Второй стакан
    s16 cocoaLeft = cocoa2X;
    s16 cocoaRight = cocoa2X + COCOA_W;

    s16 cocoaTop = cocoa2Y;
    s16 cocoaBottom = cocoa2Y + COCOA_H;

    // AABB collision
    if (playerLeft < cocoaRight &&
        playerRight > cocoaLeft &&
        playerTop < cocoaBottom &&
        playerBottom > cocoaTop)
    {
        cocoa2Taken = TRUE;

        // Скрываем второй стакан
        SPR_setVisibility(cocoaSprite2, HIDDEN);
        collectCocoa();

        XGM_setPCM(64, drink_cocoa, sizeof(drink_cocoa));
        XGM_startPlayPCM(64, 1, SOUND_PCM_CH2);
    }
}

static void drawCocoa2(s16 camX)
{
    if (cocoa2Taken)
        return;

    SPR_setPosition(
        cocoaSprite2,
        cocoa2X - camX,
        cocoa2Y
    );
}

// ---- Cocoa 3 ----
static Sprite *cocoaSprite3;
static s16 cocoa3X, cocoa3Y;
static bool cocoa3Taken = FALSE;

// ---- Cocoa 4 ----
static Sprite *cocoaSprite4;
static s16 cocoa4X, cocoa4Y;
static bool cocoa4Taken = FALSE;

static void initCocoa3(void)
{
    cocoa3Taken = FALSE;

    cocoa3X = 620;

    u8 floorY = Level1_getFloorY(cocoa3X);

    if (floorY != NO_GROUND)
        cocoa3Y = floorY - COCOA_H;
    else
        cocoa3Y = 75;

    cocoaSprite3 = SPR_addSprite(
        &cocoa_sprite,
        cocoa3X,
        cocoa3Y,
        TILE_ATTR(PAL2, TRUE, FALSE, FALSE)
    );

    SPR_setVisibility(cocoaSprite3, VISIBLE);
}

static void initCocoa4(void)
{
    cocoa4Taken = FALSE;

    cocoa4X = 820;

    u8 floorY = Level1_getFloorY(cocoa4X);

    if (floorY != NO_GROUND)
        cocoa4Y = floorY - COCOA_H;
    else
        cocoa4Y = 90;

    cocoaSprite4 = SPR_addSprite(
        &cocoa_sprite,
        cocoa4X,
        cocoa4Y,
        TILE_ATTR(PAL2, TRUE, FALSE, FALSE)
    );

    SPR_setVisibility(cocoaSprite4, VISIBLE);
}

static void checkCocoa3Collision(void)
{
    if (cocoa3Taken) return;

    s16 pl = px - SPR_W / 2;
    s16 pr = pl + SPR_W;
    s16 pt = py - SPR_H;
    s16 pb = py;

    if (pl < cocoa3X + COCOA_W &&
        pr > cocoa3X &&
        pt < cocoa3Y + COCOA_H &&
        pb > cocoa3Y)
    {
        cocoa3Taken = TRUE;

        SPR_setVisibility(cocoaSprite3, HIDDEN);
        collectCocoa();
        XGM_setPCM(64, drink_cocoa, sizeof(drink_cocoa));
        XGM_startPlayPCM(64, 1, SOUND_PCM_CH2);
    }
}

static void checkCocoa4Collision(void)
{
    if (cocoa4Taken) return;

    s16 pl = px - SPR_W / 2;
    s16 pr = pl + SPR_W;
    s16 pt = py - SPR_H;
    s16 pb = py;

    if (pl < cocoa4X + COCOA_W &&
        pr > cocoa4X &&
        pt < cocoa4Y + COCOA_H &&
        pb > cocoa4Y)
    {
        cocoa4Taken = TRUE;

        SPR_setVisibility(cocoaSprite4, HIDDEN);
        collectCocoa();
        XGM_setPCM(64, drink_cocoa, sizeof(drink_cocoa));
        XGM_startPlayPCM(64, 1, SOUND_PCM_CH2);
    }
}

static void drawCocoa3(s16 camX)
{
    if (cocoa3Taken) return;

    SPR_setPosition(
        cocoaSprite3,
        cocoa3X - camX,
        cocoa3Y
    );
}

static void drawCocoa4(s16 camX)
{
    if (cocoa4Taken) return;

    SPR_setPosition(
        cocoaSprite4,
        cocoa4X - camX,
        cocoa4Y
    );
}

// ---- Finish flag ----
#define FINISH_W 32
#define FINISH_H 48

static Sprite *finishSprite;
static s16 finishX;
static s16 finishY;
static bool levelComplete = FALSE;

static void initFinish(void)
{
    finishX = 1550;   // поставь нужную позицию в конце уровня
    u8 floorY = Level1_getFloorY(finishX);

    if (floorY != NO_GROUND)
        finishY = floorY - FINISH_H;
    else
        finishY = 70;

    // Предполагается, что спрайт называется finish_sprite (из resources)
    finishSprite = SPR_addSprite(
        &finish_sprite,
        finishX,
        finishY,
        TILE_ATTR(PAL2, TRUE, FALSE, FALSE)
    );

    SPR_setVisibility(finishSprite, VISIBLE);
}

static void drawFinish(s16 camX)
{
    if (levelComplete) return;
    SPR_setPosition(finishSprite, finishX - camX, finishY);
}

static void checkFinishCollision(void)
{
    if (levelComplete) return;

    s16 playerLeft   = px - SPR_W / 2;
    s16 playerRight  = playerLeft + SPR_W;
    s16 playerTop    = py - SPR_H;
    s16 playerBottom = py;

    s16 flagLeft   = finishX;
    s16 flagRight  = finishX + FINISH_W;
    s16 flagTop    = finishY;
    s16 flagBottom = finishY + FINISH_H;

    if (playerLeft < flagRight &&
        playerRight > flagLeft &&
        playerTop < flagBottom &&
        playerBottom > flagTop)
    {
        // Можно добавить условие: if (cocoaCount >= 4)
        levelComplete = TRUE;
        doLevelComplete();
    }
}

static void doLevelComplete(void)
{
    // Останавливаем музыку (по желанию)
    XGM_stopPlay();

    // Fade out (весь экран в чёрный)
    PAL_fadeOut(0, 63, 40, FALSE);          // 40 кадров ≈ 0.7 сек
    while (PAL_isDoingFade())
        SYS_doVBlankProcess();

    // Скрываем спрайты
    SPR_setVisibility(player, HIDDEN);
    SPR_setVisibility(finishSprite, HIDDEN);
    // Можно также скрыть врагов и какао

    // === Важно: сбрасываем скролл и чистим плоскости ===
    VDP_setHorizontalScroll(BG_A, 0);
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    SPR_clear();

    VDP_drawImage(BG_B, &victory_image, 0, 0);

    PAL_fadeIn(0, 15, victory_image.palette->data, 30, FALSE);

    // Чистим текст
    VDP_clearText(0, 0, 40);
    VDP_clearText(0, 1, 40);

    // Рисуем сообщение
    VDP_setTextPalette(PAL2);

    // Центрированные позиции (экран 40 тайлов в ширину)
    VDP_drawText("LEVEL COMPLETE", 13, 10);
    VDP_drawText("COLLECT 4 COCOA", 12, 12);

    char buf[16];
    sprintf(buf, "COCOA: %d/4", cocoaCount);
    VDP_drawText(buf, 15, 14);

    VDP_drawText("PRESS START", 14, 18);

    // Дальше твой fade-in палитры и ожидание кнопки...
    const u16 textPal[16] = {
        0x0000, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
        0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE
    };

    PAL_fadeIn(0, 15, textPal, 30, FALSE);

    while (PAL_isDoingFade())
        SYS_doVBlankProcess();

    // Ждём нажатия кнопки
    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        if (joy & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C))
        {
            VDP_clearPlane(BG_A, TRUE);
            VDP_clearPlane(BG_B, TRUE);
            showBossLevel();
        }

        SYS_doVBlankProcess();
    }

    // Можно перезапустить уровень или выйти
    // Например: respawn(); levelComplete = FALSE; и т.д.
    // Или просто зациклиться
    while (1)
    {
        SYS_doVBlankProcess();
    }
}

void showLevel1(void)
{
    XGM_startPlay(beach_music);

    SPR_init();
    u16 ind = TILE_USER_INDEX;
    PAL_setPalette(PAL2, cocoa_sprite.palette->data, CPU);
    PAL_setPalette(PAL0, level_pal.data, CPU);
    PAL_setPalette(PAL1, bgb_pal.data, CPU);
    PAL_setPalette(PAL3, mascot_sprite.palette->data, CPU);

    VDP_loadTileSet(&level_tileset, ind, DMA);
    levelMap = MAP_create(&level_map, BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, ind));
    VDP_loadTileSet(&bgb_tileset, ind, DMA);
    BgbMapLevel = MAP_create(&bgb_map, BG_B, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, ind));

    player = SPR_addSprite(&mascot_sprite, 0, 0, TILE_ATTR(PAL3, TRUE, FALSE, FALSE));

    respawn();
    faceLeft = FALSE;

    initEnemies();

    initCocoa();

    initCocoa2();

    initCocoa3();
    initCocoa4();

    initFinish();

    while (TRUE)
    {
        updatePlayer();
        checkAttackHits();
        checkCocoaCollision();
        checkCocoa2Collision();
        checkCocoa3Collision();
        checkCocoa4Collision();
        checkFinishCollision();

        s16 camX = px - SCREEN_W / 2;
        if (camX < 0) camX = 0;
        if (camX > LEVEL1_WIDTH - SCREEN_W) camX = LEVEL1_WIDTH - SCREEN_W;

        MAP_scrollTo(levelMap, camX, 0);
        updateEnemies(camX);
        drawPlayer(camX);
        drawCocoa(camX);
        drawCocoa2(camX);
        drawCocoa3(camX);
        drawCocoa4(camX);
        drawFinish(camX);

        SPR_update();
        SYS_doVBlankProcess();
    }
}


void mainLevelGame(void)
{
    VDP_setScreenWidth320();
    JOY_init();

    // Включаем поддержку 6-кнопочного геймпада (иначе Z и X не работают)
    JOY_setSupport(PORT_1, JOY_SUPPORT_6BTN);

    showLevel1();

    while(1)
    {
        SYS_doVBlankProcess();
    }
}