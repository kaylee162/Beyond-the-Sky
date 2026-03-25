#include "game.h"

static GameState state;
static GameState gameplayStateBeforePause;
static int currentLevel;
static int levelWidth;
static int levelHeight;

static Player player;
static ResourceItem resource;
static Bee bee;

static int hOff;
static int vOff;
static int cloudHOff;
static int frameCounter;

static int carryingResource;
static int beanstalkGrown;
static int instantGrowCheat;

static int respawnX;
static int respawnY;
static int loseReturnLevel;

static void initGraphics(void);
static void initBgTiles(void);
static void initObjTiles(void);
static void drawHudText(void);
static void drawMenuScreen(const char* title, const char* line1, const char* line2, const char* line3);
static void drawPauseScreen(void);
static void goToStart(void);
static void goToInstructions(void);
static void goToHome(int respawn);
static void goToSky(int respawn);
static void goToWin(void);
static void goToLose(int levelToReturnTo);
static void respawnIntoCurrentLevel(void);
static void updateStart(void);
static void updateInstructions(void);
static void updateHome(void);
static void updateSky(void);
static void updatePause(void);
static void updateLose(void);
static void updateWin(void);
static void updateGameplayCommon(void);
static void updatePlayerMovement(void);
static void updateCamera(void);
static void updateBee(void);
static void tryDeposit(void);
static void drawGameplay(void);
static void drawSprites(void);
static void hideSprite(int index);
static int canMoveTo(int newX, int newY);
static int onLadder(void);
static int touchesHazard(void);
static void applySkyPalettePulse(void);
static void putText(int col, int row, const char* str);
static void clearHud(void);

static void writeTile4bpp(volatile u32* dst, const u8 pixels[64]) {
    int i;
    for (i = 0; i < 8; i++) {
        dst[i] =
            (pixels[i * 8 + 0] << 0) |
            (pixels[i * 8 + 1] << 4) |
            (pixels[i * 8 + 2] << 8) |
            (pixels[i * 8 + 3] << 12) |
            (pixels[i * 8 + 4] << 16) |
            (pixels[i * 8 + 5] << 20) |
            (pixels[i * 8 + 6] << 24) |
            (pixels[i * 8 + 7] << 28);
    }
}

static void makeSolidTile(int charblock, int tileIndex, u8 color) {
    u8 pixels[64];
    int i;
    for (i = 0; i < 64; i++) pixels[i] = color;
    writeTile4bpp((volatile u32*)&CHARBLOCK[charblock].tileimg[tileIndex * 16], pixels);
}

static void makeGrassTile(void) {
    u8 pixels[64];
    int x;
    int y;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            if (y < 2) pixels[y * 8 + x] = 4;
            else pixels[y * 8 + x] = 5;
        }
    }

    writeTile4bpp((volatile u32*)&CHARBLOCK[GAME_CHARBLOCK].tileimg[TILE_GRASS * 16], pixels);
}

static void makePlatformTile(void) {
    u8 pixels[64];
    int x;
    int y;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            pixels[y * 8 + x] = (y < 2) ? 3 : 4;
        }
    }

    writeTile4bpp((volatile u32*)&CHARBLOCK[GAME_CHARBLOCK].tileimg[TILE_PLATFORM * 16], pixels);
}

static void makeLadderTile(void) {
    u8 pixels[64];
    int x;
    int y;

    for (i = 0; i < 64; i++) pixels[i] = 2;
    for (y = 0; y < 8; y++) {
        pixels[y * 8 + 2] = 9;
        pixels[y * 8 + 5] = 9;
    }
    for (x = 1; x <= 6; x++) {
        pixels[2 * 8 + x] = 9;
        pixels[5 * 8 + x] = 9;
    }

    writeTile4bpp((volatile u32*)&CHARBLOCK[GAME_CHARBLOCK].tileimg[TILE_LADDER * 16], pixels);
}

static void makeHazardTile(void) {
    u8 pixels[64];
    int x;
    int y;

    for (i = 0; i < 64; i++) pixels[i] = 2;
    for (x = 1; x < 7; x++) {
        pixels[6 * 8 + x] = 6;
        if (x > 1 && x < 6) pixels[4 * 8 + x] = 6;
    }
    for (y = 3; y < 7; y++) {
        pixels[y * 8 + 2] = 6;
        pixels[y * 8 + 5] = 6;
    }

    writeTile4bpp((volatile u32*)&CHARBLOCK[GAME_CHARBLOCK].tileimg[TILE_HAZARD * 16], pixels);
}

static void makeSoilTile(void) {
    u8 pixels[64];
    int x;
    int y;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            pixels[y * 8 + x] = (y < 2) ? 8 : 5;
        }
    }

    writeTile4bpp((volatile u32*)&CHARBLOCK[GAME_CHARBLOCK].tileimg[TILE_SOIL * 16], pixels);
}

static void makeBeanstalkTile(void) {
    u8 pixels[64];
    int x;
    int y;

    for (i = 0; i < 64; i++) pixels[i] = 2;
    for (y = 0; y < 8; y++) {
        pixels[y * 8 + 3] = 7;
        pixels[y * 8 + 4] = 7;
    }
    pixels[1 * 8 + 5] = 4;
    pixels[3 * 8 + 2] = 4;
    pixels[5 * 8 + 5] = 4;
    pixels[7 * 8 + 2] = 4;

    writeTile4bpp((volatile u32*)&CHARBLOCK[GAME_CHARBLOCK].tileimg[TILE_BEANSTALK * 16], pixels);
}

static void makePortalTile(void) {
    u8 pixels[64];
    int x;
    int y;

    for (i = 0; i < 64; i++) pixels[i] = 2;
    for (y = 1; y < 7; y++) {
        for (x = 2; x < 6; x++) {
            pixels[y * 8 + x] = (x == 2 || x == 5 || y == 1 || y == 6) ? 9 : 3;
        }
    }

    writeTile4bpp((volatile u32*)&CHARBLOCK[GAME_CHARBLOCK].tileimg[TILE_PORTAL * 16], pixels);
}

static void makeSignTile(void) {
    u8 pixels[64];
    int x;
    int y;

    for (i = 0; i < 64; i++) pixels[i] = 2;
    for (y = 1; y < 4; y++) {
        for (x = 1; x < 7; x++) pixels[y * 8 + x] = 8;
    }
    for (y = 4; y < 8; y++) pixels[y * 8 + 3] = 9;

    writeTile4bpp((volatile u32*)&CHARBLOCK[GAME_CHARBLOCK].tileimg[TILE_SIGN * 16], pixels);
}

static void buildPlayerFrame(int baseTile, int accentColor) {
    u8 pixels[64];
    int x;
    int y;
    int tile;
    int tx;
    int ty;

    for (tile = 0; tile < 4; tile++) {
        for (i = 0; i < 64; i++) pixels[i] = 0;

        tx = (tile % 2) * 8;
        ty = (tile / 2) * 8;

        for (y = 0; y < 8; y++) {
            for (x = 0; x < 8; x++) {
                int worldX = tx + x;
                int worldY = ty + y;
                if (worldX >= 4 && worldX <= 11 && worldY >= 2 && worldY <= 13) pixels[y * 8 + x] = 8;
                if (worldX >= 5 && worldX <= 10 && worldY >= 5 && worldY <= 13) pixels[y * 8 + x] = accentColor;
                if (worldX >= 6 && worldX <= 9 && worldY <= 3) pixels[y * 8 + x] = 3;
            }
        }

        writeTile4bpp((volatile u32*)((u16*)0x6010000 + (baseTile + tile) * 16), pixels);
    }
}

static void buildSmallSpriteTile(int tileIndex, u8 mainColor, u8 accentColor) {
    u8 pixels[64];
    int x;
    int y;

    for (i = 0; i < 64; i++) pixels[i] = 0;
    for (y = 2; y < 6; y++) {
        for (x = 2; x < 6; x++) pixels[y * 8 + x] = mainColor;
    }
    pixels[1 * 8 + 3] = accentColor;
    pixels[1 * 8 + 4] = accentColor;
    pixels[6 * 8 + 3] = accentColor;
    pixels[6 * 8 + 4] = accentColor;

    writeTile4bpp((volatile u32*)((u16*)0x6010000 + tileIndex * 16), pixels);
}

void initGame(void) {
    initGraphics();
    initLevelData();

    frameCounter = 0;
    carryingResource = 0;
    beanstalkGrown = 0;
    instantGrowCheat = 0;

    goToStart();
}

void updateGame(void) {
    frameCounter++;

    switch (state) {
        case STATE_START:
            updateStart();
            break;
        case STATE_INSTRUCTIONS:
            updateInstructions();
            break;
        case STATE_HOME:
            updateHome();
            break;
        case STATE_SKY:
            updateSky();
            break;
        case STATE_PAUSE:
            updatePause();
            break;
        case STATE_WIN:
            updateWin();
            break;
        case STATE_LOSE:
            updateLose();
            break;
    }
}

void drawGame(void) {
    if (state == STATE_HOME || state == STATE_SKY) {
        drawGameplay();
    }

    DMANow(3, shadowOAM, OAM, 128 * 4);
}

static void initGraphics(void) {
    initMode0();

    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1) | BG_ENABLE(2) | SPRITE_ENABLE | SPRITE_MODE_1D;

    setupBackground(0, BG_PRIORITY(0) | BG_CHARBLOCK(HUD_CHARBLOCK) | BG_SCREENBLOCK(HUD_SCREENBLOCK) | BG_4BPP | BG_SIZE_SMALL);
    setupBackground(1, BG_PRIORITY(1) | BG_CHARBLOCK(GAME_CHARBLOCK) | BG_SCREENBLOCK(GAME_SCREENBLOCK) | BG_4BPP | BG_SIZE_WIDE);
    setupBackground(2, BG_PRIORITY(2) | BG_CHARBLOCK(GAME_CHARBLOCK) | BG_SCREENBLOCK(CLOUD_SCREENBLOCK) | BG_4BPP | BG_SIZE_WIDE);

    hideSprites();
    clearCharBlock(HUD_CHARBLOCK);
    clearCharBlock(GAME_CHARBLOCK);
    clearScreenblock(HUD_SCREENBLOCK);
    clearScreenblock(GAME_SCREENBLOCK);
    clearScreenblock(GAME_SCREENBLOCK + 1);
    clearScreenblock(CLOUD_SCREENBLOCK);
    clearScreenblock(CLOUD_SCREENBLOCK + 1);

    fontInit(HUD_CHARBLOCK, 0);
    initBgTiles();
    initObjTiles();
}

static void initBgTiles(void) {
    BG_PALETTE[0] = RGB(0, 0, 0);
    BG_PALETTE[1] = RGB(0, 0, 0);
    BG_PALETTE[2] = RGB(15, 24, 31);
    BG_PALETTE[3] = RGB(31, 31, 31);
    BG_PALETTE[4] = RGB(15, 28, 8);
    BG_PALETTE[5] = RGB(19, 11, 5);
    BG_PALETTE[6] = RGB(31, 7, 7);
    BG_PALETTE[7] = RGB(7, 24, 7);
    BG_PALETTE[8] = RGB(28, 23, 14);
    BG_PALETTE[9] = RGB(31, 25, 6);

    makeSolidTile(GAME_CHARBLOCK, TILE_SKY, 2);
    makeSolidTile(GAME_CHARBLOCK, TILE_CLOUD, 3);
    makeGrassTile();
    makeSolidTile(GAME_CHARBLOCK, TILE_DIRT, 5);
    makePlatformTile();
    makeLadderTile();
    makeHazardTile();
    makeSoilTile();
    makeBeanstalkTile();
    makePortalTile();
    makeSignTile();
}

static void initObjTiles(void) {
    SPRITE_PAL[0] = RGB(0, 0, 0);
    SPRITE_PAL[1] = RGB(31, 31, 31);
    SPRITE_PAL[2] = RGB(15, 24, 31);
    SPRITE_PAL[3] = RGB(28, 23, 14);
    SPRITE_PAL[4] = RGB(15, 28, 8);
    SPRITE_PAL[5] = RGB(31, 7, 7);
    SPRITE_PAL[6] = RGB(29, 14, 29);
    SPRITE_PAL[7] = RGB(5, 18, 6);
    SPRITE_PAL[8] = RGB(22, 13, 6);
    SPRITE_PAL[9] = RGB(31, 25, 6);

    buildPlayerFrame(OBJ_TILE_PLAYER0, 4);
    buildPlayerFrame(OBJ_TILE_PLAYER1, 9);
    buildSmallSpriteTile(OBJ_TILE_ITEM, 9, 3);
    buildSmallSpriteTile(OBJ_TILE_BEE0, 9, 1);
    buildSmallSpriteTile(OBJ_TILE_BEE1, 5, 1);
}

static void goToStart(void) {
    state = STATE_START;
    clearHud();
    drawMenuScreen("BEYOND THE SKY", "PRESS START", "A: NEXT   B: BACK", "SELECT+UP TOGGLE GROW CHEAT");
    hideSprites();
}

static void goToInstructions(void) {
    state = STATE_INSTRUCTIONS;
    clearHud();
    drawMenuScreen("INSTRUCTIONS", "LEFT/RIGHT MOVE  A JUMP", "UP/DOWN CLIMB  START PAUSE", "GET WATER IN SKY LEVEL, RETURN, PRESS B ON SOIL");
    hideSprites();
}

static void goToHome(int respawn) {
    state = STATE_HOME;
    currentLevel = LEVEL_HOME;
    levelWidth = HOME_PIXEL_W;
    levelHeight = HOME_PIXEL_H;
    cloudHOff = 0;

    loadHomeLevelMap();
    clearHud();

    if (!respawn) {
        player.x = 24;
        player.y = 120;
    } else {
        player.x = respawnX;
        player.y = respawnY;
    }

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;
    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;

    respawnX = 24;
    respawnY = 120;

    resource.active = 0;
    bee.active = 0;

    hOff = 0;
    vOff = 0;
}

static void goToSky(int respawn) {
    state = STATE_SKY;
    currentLevel = LEVEL_SKY;
    levelWidth = SKY_PIXEL_W;
    levelHeight = SKY_PIXEL_H;
    cloudHOff = 0;

    loadSkyLevelMap();
    clearHud();

    if (!respawn) {
        player.x = 20;
        player.y = 200;
    } else {
        player.x = respawnX;
        player.y = respawnY;
    }

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;
    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;

    respawnX = 20;
    respawnY = 200;

    resource.x = 470;
    resource.y = 52;
    resource.active = carryingResource ? 0 : 1;
    resource.bob = 0;

    bee.x = 360;
    bee.y = 80;
    bee.active = 1;
    bee.animFrame = 0;
    bee.animCounter = 0;
    bee.dir = 1;

    hOff = 0;
    vOff = 96;
}

static void goToPause(void) {
    gameplayStateBeforePause = state;
    state = STATE_PAUSE;
    drawPauseScreen();
}

static void goToWin(void) {
    state = STATE_WIN;
    clearHud();
    drawMenuScreen("YOU WIN", "THE BEANSTALK BLOOMED", "PRESS START TO PLAY AGAIN", instantGrowCheat ? "CHEAT MODE WAS ON" : "CHEAT MODE WAS OFF");
    hideSprites();
}

static void goToLose(int levelToReturnTo) {
    loseReturnLevel = levelToReturnTo;
    state = STATE_LOSE;
    clearHud();
    drawMenuScreen("YOU WILTED", "HAZARD HIT OR FALL RESET", "PRESS START TO RESPAWN", "THIS STILL COUNTS AS YOUR RESET/KILL OBSTACLE");
    hideSprites();
}

static void respawnIntoCurrentLevel(void) {
    if (loseReturnLevel == LEVEL_HOME) goToHome(1);
    else goToSky(1);
}

static void updateStart(void) {
    if (BUTTON_PRESSED(BUTTON_START) || BUTTON_PRESSED(BUTTON_A)) {
        goToInstructions();
    }
}

static void updateInstructions(void) {
    if (BUTTON_PRESSED(BUTTON_B)) {
        goToStart();
    }
    if (BUTTON_PRESSED(BUTTON_START) || BUTTON_PRESSED(BUTTON_A)) {
        goToHome(0);
    }
}

static void updateHome(void) {
    updateGameplayCommon();

    if (BUTTON_PRESSED(BUTTON_UP) && player.x > 456 && player.y > 180) {
        goToSky(0);
    }

    if (carryingResource) {
        tryDeposit();
    }

    if (beanstalkGrown && player.y < 24 && player.x >= 48 && player.x <= 104) {
        goToWin();
    }
}

static void updateSky(void) {
    updateGameplayCommon();
    updateBee();

    if (resource.active && collision(player.x, player.y, player.width, player.height,
        resource.x, resource.y + ((resource.bob >> 3) & 1), ITEM_SIZE, ITEM_SIZE)) {
        resource.active = 0;
        carryingResource = 1;
    }

    resource.bob++;

    if (BUTTON_PRESSED(BUTTON_UP) && player.x < 16) {
        goToHome(0);
    }
}

static void updatePause(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        state = gameplayStateBeforePause;
        clearHud();
    }
    if (BUTTON_PRESSED(BUTTON_SELECT)) {
        goToStart();
    }
}

static void updateLose(void) {
    if (BUTTON_PRESSED(BUTTON_START) || BUTTON_PRESSED(BUTTON_A)) {
        respawnIntoCurrentLevel();
    }
}

static void updateWin(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        carryingResource = 0;
        beanstalkGrown = 0;
        initLevelData();
        goToStart();
    }
}

static void updateGameplayCommon(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        goToPause();
        return;
    }

    if (BUTTON_PRESSED(BUTTON_SELECT) && BUTTON_HELD(BUTTON_UP)) {
        instantGrowCheat = !instantGrowCheat;
    }

    updatePlayerMovement();
    updateCamera();
    applySkyPalettePulse();

    if (touchesHazard() || player.y > levelHeight + 16) {
        goToLose(currentLevel);
    }
}

static void updatePlayerMovement(void) {
    int nextX;
    int nextY;

    player.oldX = player.x;
    player.oldY = player.y;

    player.climbing = onLadder();

    if (BUTTON_HELD(BUTTON_LEFT)) {
        nextX = player.x - MOVE_SPEED;
        if (canMoveTo(nextX, player.y)) player.x = nextX;
        player.facingLeft = 1;
        player.animCounter++;
    }

    if (BUTTON_HELD(BUTTON_RIGHT)) {
        nextX = player.x + MOVE_SPEED;
        if (canMoveTo(nextX, player.y)) player.x = nextX;
        player.facingLeft = 0;
        player.animCounter++;
    }

    if (player.climbing) {
        player.yVel = 0;
        if (BUTTON_HELD(BUTTON_UP)) player.y -= CLIMB_SPEED;
        if (BUTTON_HELD(BUTTON_DOWN)) player.y += CLIMB_SPEED;
    } else {
        if (BUTTON_PRESSED(BUTTON_A) && player.grounded) {
            player.yVel = JUMP_VEL;
            player.grounded = 0;
        }

        player.yVel += GRAVITY;
        if (player.yVel > MAX_FALL_SPEED) player.yVel = MAX_FALL_SPEED;

        nextY = player.y + player.yVel;
        if (canMoveTo(player.x, nextY)) {
            player.y = nextY;
            player.grounded = 0;
        } else {
            if (player.yVel > 0) {
                while (!canMoveTo(player.x, player.y + 1)) player.y--;
                player.grounded = 1;
            }
            player.yVel = 0;
        }
    }

    if (frameCounter % 12 == 0) {
        player.animFrame ^= 1;
    }
}

static void updateCamera(void) {
    hOff = player.x - (SCREENWIDTH / 2) + (player.width / 2);
    vOff = player.y - (SCREENHEIGHT / 2) + (player.height / 2);

    if (hOff < 0) hOff = 0;
    if (vOff < 0) vOff = 0;
    if (hOff > levelWidth - SCREENWIDTH) hOff = levelWidth - SCREENWIDTH;
    if (vOff > levelHeight - SCREENHEIGHT) vOff = levelHeight - SCREENHEIGHT;

    cloudHOff = hOff / 3 + frameCounter / 6;
}

static void updateBee(void) {
    if (!bee.active) return;

    bee.animCounter++;
    if (bee.animCounter >= 12) {
        bee.animCounter = 0;
        bee.animFrame ^= 1;
    }

    bee.x += bee.dir;
    if (bee.x < 340) bee.dir = 1;
    if (bee.x > 410) bee.dir = -1;
}

static void tryDeposit(void) {
    int footX = player.x + player.width / 2;
    int footY = player.y + player.height + 1;

    if (isSoilPixel(currentLevel, footX, footY) && BUTTON_PRESSED(BUTTON_B)) {
        carryingResource = 0;
        beanstalkGrown = 1;
        if (instantGrowCheat || beanstalkGrown) {
            growHomeBeanstalk();
            loadHomeLevelMap();
        }
    }
}

static int canMoveTo(int newX, int newY) {
    return !isSolidPixel(currentLevel, newX, newY)
        && !isSolidPixel(currentLevel, newX + player.width - 1, newY)
        && !isSolidPixel(currentLevel, newX, newY + player.height - 1)
        && !isSolidPixel(currentLevel, newX + player.width - 1, newY + player.height - 1);
}

static int onLadder(void) {
    int centerX = player.x + player.width / 2;
    int centerY = player.y + player.height / 2;
    return isLadderPixel(currentLevel, centerX, centerY);
}

static int touchesHazard(void) {
    return isHazardPixel(currentLevel, player.x + 2, player.y + player.height)
        || isHazardPixel(currentLevel, player.x + player.width - 3, player.y + player.height);
}

static void applySkyPalettePulse(void) {
    if ((frameCounter / 30) & 1) {
        BG_PALETTE[2] = RGB(15, 24, 31);
    } else {
        BG_PALETTE[2] = RGB(18, 26, 31);
    }
}

static void drawGameplay(void) {
    clearHud();
    drawHudText();

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, cloudHOff, vOff / 4);

    drawSprites();
}

static void drawSprites(void) {
    int screenX = player.x - hOff;
    int screenY = player.y - vOff;

    shadowOAM[0].attr0 = ATTR0_Y(screenY) | ATTR0_SQUARE | ATTR0_4BPP;
    shadowOAM[0].attr1 = ATTR1_X(screenX) | ATTR1_SMALL | (player.facingLeft ? ATTR1_HFLIP : 0);
    shadowOAM[0].attr2 = ATTR2_TILEID(player.animFrame ? OBJ_TILE_PLAYER1 : OBJ_TILE_PLAYER0) | ATTR2_PRIORITY(0);

    if (resource.active && state == STATE_SKY) {
        shadowOAM[1].attr0 = ATTR0_Y(resource.y - vOff + ((resource.bob >> 3) & 1)) | ATTR0_SQUARE | ATTR0_4BPP;
        shadowOAM[1].attr1 = ATTR1_X(resource.x - hOff) | ATTR1_TINY;
        shadowOAM[1].attr2 = ATTR2_TILEID(OBJ_TILE_ITEM) | ATTR2_PRIORITY(0);
    } else {
        hideSprite(1);
    }

    if (bee.active && state == STATE_SKY) {
        shadowOAM[2].attr0 = ATTR0_Y(bee.y - vOff) | ATTR0_SQUARE | ATTR0_4BPP;
        shadowOAM[2].attr1 = ATTR1_X(bee.x - hOff) | ATTR1_TINY;
        shadowOAM[2].attr2 = ATTR2_TILEID(bee.animFrame ? OBJ_TILE_BEE1 : OBJ_TILE_BEE0) | ATTR2_PRIORITY(0);
    } else {
        hideSprite(2);
    }

    hideSprite(3);
    hideSprite(4);
    hideSprite(5);
}

static void hideSprite(int index) {
    shadowOAM[index].attr0 = ATTR0_HIDE;
    shadowOAM[index].attr1 = 0;
    shadowOAM[index].attr2 = 0;
}

static void drawHudText(void) {
    if (state == STATE_HOME) {
        putText(1, 1, "HOME");
        putText(1, 3, carryingResource ? "CARRYING: WATER" : "CARRYING: NONE");
        putText(1, 5, beanstalkGrown ? "BEANSTALK: GROWN" : "BEANSTALK: SMALL");
        putText(1, 7, "UP AT PORTAL -> SKY");
        putText(1, 8, "PRESS B ON SOIL TO DEPOSIT");
    } else {
        putText(1, 1, "SKY LEVEL");
        putText(1, 3, carryingResource ? "RESOURCE COLLECTED" : "GET THE WATER DROP");
        putText(1, 5, "UP AT LEFT PORTAL -> HOME");
        putText(1, 7, "LADDER + HAZARDS ACTIVE");
    }

    putText(1, 10, instantGrowCheat ? "CHEAT: INSTANT GROW ON" : "CHEAT: OFF");
}

static void drawMenuScreen(const char* title, const char* line1, const char* line2, const char* line3) {
    loadHomeLevelMap();
    setBackgroundOffset(1, 0, 0);
    setBackgroundOffset(2, 0, 0);
    putText(6, 5, title);
    putText(3, 10, line1);
    putText(3, 13, line2);
    putText(3, 16, line3);
}

static void drawPauseScreen(void) {
    clearHud();
    putText(11, 7, "PAUSED");
    putText(6, 11, "START TO RESUME");
    putText(4, 14, "SELECT TO RETURN TO TITLE");
    hideSprites();
}

static void clearHud(void) {
    fontClearMap(HUD_SCREENBLOCK, 0);
}

static void putText(int col, int row, const char* str) {
    fontDrawString(HUD_SCREENBLOCK, col, row, str);
}