#include "game.h"
#include "tileset.h"
#include "spriteSheet.h"

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


// --------------------------------------------------
// FORWARD DECLARATIONS
// --------------------------------------------------
static void initGraphics(void);
static void initBgAssets(void);
static void initObjAssets(void);
static void copySpriteBlockFromSheet(const unsigned short* sheetTiles, int sheetWidthTiles,
    int srcTileX, int srcTileY, int blockWidthTiles, int blockHeightTiles, int dstTileIndex);
static void drawHudText(void);
static void drawMenuScreen(const char* title, const char* line1, const char* line2, const char* line3);
static void drawPauseScreen(void);
static void goToStart(void);
static void goToInstructions(void);
static void goToHome(int respawn);
static void goToLevelOne(int respawn);
static void goToLevelTwo(int respawn);
static void goToPause(void);
static void goToWin(void);
static void goToLose(int levelToReturnTo);
static void respawnIntoCurrentLevel(void);
static void updateStart(void);
static void updateInstructions(void);
static void updateHome(void);
static void updateLevelOne(void);
static void updateLevelTwo(void);
static void updatePause(void);
static void updateLose(void);
static void updateWin(void);

static void updateGameplayCommon(void);
static void updatePlayerMovement(void);
static void updateCamera(void);
static void drawGameplay(void);
static void drawSprites(void);
static void hideSprite(int index);

// Collision / interaction helpers
static int canMoveTo(int newX, int newY);
static int onClimbTile(void);
static int touchesHazard(void);
static void handleLevelTransitions(void);

static void putText(int col, int row, const char* str);
static void clearHud(void);

// --------------------------------------------------
// Copy a rectangular block of 8x8 OBJ tiles out of the full exported
// sprite sheet and repack it into a contiguous chunk in OBJ VRAM.
//
// Why this matters:
// In 1D OBJ mapping, every hardware sprite expects its tiles to be laid
// out contiguously in OBJ memory. A large exported sheet does not store
// animation frames that way, so we manually repack each player piece.
// --------------------------------------------------
static void copySpriteBlockFromSheet(const unsigned short* sheetTiles, int sheetWidthTiles,
    int srcTileX, int srcTileY, int blockWidthTiles, int blockHeightTiles, int dstTileIndex) {

    volatile unsigned short* objTiles = (volatile unsigned short*)0x6010000;

    for (int tileRow = 0; tileRow < blockHeightTiles; tileRow++) {
        for (int tileCol = 0; tileCol < blockWidthTiles; tileCol++) {
            int srcIndex = ((srcTileY + tileRow) * sheetWidthTiles) + (srcTileX + tileCol);
            int dstIndex = dstTileIndex + (tileRow * blockWidthTiles) + tileCol;

            // Each 4bpp tile is 32 bytes = 16 unsigned shorts.
            for (int i = 0; i < 16; i++) {
                objTiles[dstIndex * 16 + i] = sheetTiles[srcIndex * 16 + i];
            }
        }
    }
}

static void initBgAssets(void) {
    // Load the tileset palette and tiles for the gameplay backgrounds.
    DMANow(3, (void*)tilesetPal, BG_PALETTE, tilesetPalLen / 2);
    DMANow(3, (void*)tilesetTiles, CHARBLOCK[1].tileimg, tilesetTilesLen / 2);

    // Palette index 0 is the global BG backdrop color.
    // Because your tilemaps now use index 0 as transparent, this is the sky
    // color that will show through anywhere index 0 appears.
    BG_PALETTE[0] = SKY_COLOR;

    // Give the HUD/menu font its own dedicated palette row so the text does
    // not inherit random colors from the tileset palette.
    BG_PALETTE[FONT_PALROW * 16 + 0] = SKY_COLOR;
    BG_PALETTE[FONT_PALROW * 16 + 1] = FONT_COLOR;
}

static void initObjAssets(void) {
    DMANow(3, (void*)spriteSheetPal, SPRITE_PAL, spriteSheetPalLen / 2);

    // Clear enough OBJ tile memory that stale sprite art does not show up.
    static unsigned short zero = 0;
    DMANow(3, &zero, (volatile unsigned short*)0x6010000, (32 * 32) | DMA_SOURCE_FIXED);

    // The player now occupies one clean 2x4 tile block at the top-left of the
    // sprite sheet, so repack that 16x32 block directly into OBJ tile slot 0.
    copySpriteBlockFromSheet(spriteSheetTiles, 32, 0, 0, 2, 4, OBJ_TILE_PLAYER);
}

void initGame(void) {
    initGraphics();

    frameCounter = 0;
    carryingResource = 0;
    beanstalkGrown = 0;
    instantGrowCheat = 0;

    currentLevel = LEVEL_HOME;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    loadLevelMaps(currentLevel);
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);
    setBackgroundOffset(2, 0, 0);

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
        case STATE_LEVEL1:
            updateLevelOne();
            break;
        case STATE_LEVEL2:
            updateLevelTwo();
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
    switch (state) {
        case STATE_START:
            clearHud();
            configureMapBackgroundsForLevel(LEVEL_HOME);
            loadLevelMaps(LEVEL_HOME);
            setBackgroundOffset(0, 0, 0);
            setBackgroundOffset(1, 0, 0);
            setBackgroundOffset(2, 0, 0);
            drawMenuScreen("BEYOND THE SKY", "PRESS START", "A: NEXT   B: BACK", "SELECT+LEFT/RIGHT/DOWN = MAP CHEATS");
            hideSprites();
            break;

        case STATE_INSTRUCTIONS:
            clearHud();
            configureMapBackgroundsForLevel(LEVEL_HOME);
            loadLevelMaps(LEVEL_HOME);
            setBackgroundOffset(0, 0, 0);
            setBackgroundOffset(1, 0, 0);
            setBackgroundOffset(2, 0, 0);
            drawMenuScreen("INSTRUCTIONS", "ARROWS MOVE", "A JUMP  START PAUSE", "SELECT+LEFT/RIGHT/DOWN TO WARP MAPS");
            hideSprites();
            break;

        case STATE_HOME:
        case STATE_LEVEL1:
        case STATE_LEVEL2:
            drawGameplay();
            break;

        case STATE_PAUSE:
            drawPauseScreen();
            break;

        case STATE_WIN:
            clearHud();
            drawMenuScreen("YOU WIN", "PRESS START TO PLAY AGAIN", "", "");
            hideSprites();
            break;

        case STATE_LOSE:
            clearHud();
            drawMenuScreen("YOU LOSE", "PRESS START TO RESPAWN", "", "");
            hideSprites();
            break;
    }
}

static void initGraphics(void) {
    initMode0();

    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1) | BG_ENABLE(2)
        | SPRITE_ENABLE | SPRITE_MODE_1D;

    // BG0 = HUD text layer
    setupBackground(0, BG_PRIORITY(0) | BG_CHARBLOCK(0) | BG_SCREENBLOCK(HUD_SCREENBLOCK) | BG_4BPP | BG_SIZE_SMALL);

    // BG1/BG2 shape gets configured per-level in levels.c
    configureMapBackgroundsForLevel(LEVEL_HOME);

    hideSprites();
    clearCharBlock(0);
    clearScreenblock(HUD_SCREENBLOCK);
    clearScreenblock(BG_BACK_SB_A);
    clearScreenblock(BG_BACK_SB_B);
    clearScreenblock(BG_FRONT_SB_A);
    clearScreenblock(BG_FRONT_SB_B);

    fontInit(0, 0);
    initBgAssets();
    initObjAssets();
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
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();

    if (!respawn) {
        // Spawn near the bottom-left of the map.
        // Ground is 3 tiles tall = 24 pixels.
        player.x = 0;
        player.y = levelHeight - 24 - PLAYER_HEIGHT;
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
    player.facingLeft = 0;
    player.animFrame = 0;
    player.animCounter = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    respawnX = player.x;
    respawnY = player.y;

    // Start camera at bottom-left of the world.
    hOff = 0;
    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, hOff, vOff);
}

static void goToLevelOne(int respawn) {
    state = STATE_LEVEL1;
    currentLevel = LEVEL_ONE;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();

    if (!respawn) {
        player.x = 0;
        player.y = levelHeight - 24 - PLAYER_HEIGHT;
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
    player.facingLeft = 0;
    player.animFrame = 0;
    player.animCounter = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    respawnX = player.x;
    respawnY = player.y;

    hOff = 0;
    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, hOff, vOff);
}

static void goToLevelTwo(int respawn) {
    state = STATE_LEVEL2;
    currentLevel = LEVEL_TWO;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();

    if (!respawn) {
        player.x = 0;
        player.y = levelHeight - 24 - PLAYER_HEIGHT;
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
    player.facingLeft = 0;
    player.animFrame = 0;
    player.animCounter = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    respawnX = player.x;
    respawnY = player.y;

    hOff = 0;
    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, hOff, vOff);
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
    if (loseReturnLevel == LEVEL_HOME) {
        goToHome(1);
    } else if (loseReturnLevel == LEVEL_ONE) {
        goToLevelOne(1);
    } else {
        goToLevelTwo(1);
    }
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
}

static void updateLevelOne(void) {
    updateGameplayCommon();
}

static void updateLevelTwo(void) {
    updateGameplayCommon();
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
        goToStart();
    }
}

static void updateGameplayCommon(void) {
    // START pauses the game.
    if (BUTTON_PRESSED(BUTTON_START)) {
        goToPause();
        return;
    }

    // Optional cheat toggle if you still want it.
    if (BUTTON_PRESSED(BUTTON_SELECT) && BUTTON_HELD(BUTTON_UP)) {
        instantGrowCheat = !instantGrowCheat;
    }

    // Move the player using collision-map rules.
    updatePlayerMovement();

    // Check transition tiles after movement.
    handleLevelTransitions();

    // Update camera after movement / transitions.
    updateCamera();

    // Hazard check.
    if (touchesHazard() || player.y > levelHeight + 16) {
        goToLose(currentLevel);
    }
}

static void updatePlayerMovement(void) {
    int nextX;
    int nextY;
    int allowVerticalMovement;

    player.oldX = player.x;
    player.oldY = player.y;

    nextX = player.x;
    nextY = player.y;

    // --------------------------------------------------
    // Horizontal movement
    // --------------------------------------------------
    if (BUTTON_HELD(BUTTON_LEFT)) {
        nextX -= MOVE_SPEED;
        player.facingLeft = 1;
    }

    if (BUTTON_HELD(BUTTON_RIGHT)) {
        nextX += MOVE_SPEED;
        player.facingLeft = 0;
    }

    // --------------------------------------------------
    // Vertical movement only on climb tiles
    // --------------------------------------------------
    allowVerticalMovement = onClimbTile();

    if (BUTTON_HELD(BUTTON_UP)) {
        int testCenterX = player.x + (player.width / 2);
        int testY = player.y - MOVE_SPEED + (player.height / 2);

        if (allowVerticalMovement || isClimbPixel(currentLevel, testCenterX, testY)) {
            nextY -= MOVE_SPEED;
            player.climbing = 1;
        }
    }

    if (BUTTON_HELD(BUTTON_DOWN)) {
        int testCenterX = player.x + (player.width / 2);
        int testY = player.y + MOVE_SPEED + (player.height / 2);

        if (allowVerticalMovement || isClimbPixel(currentLevel, testCenterX, testY)) {
            nextY += MOVE_SPEED;
            player.climbing = 1;
        }
    }

    if (!BUTTON_HELD(BUTTON_UP) && !BUTTON_HELD(BUTTON_DOWN)) {
        player.climbing = 0;
    }

    // --------------------------------------------------
    // Clamp to map bounds
    // --------------------------------------------------
    if (nextX < 0) nextX = 0;
    if (nextY < 0) nextY = 0;
    if (nextX > levelWidth - player.width) nextX = levelWidth - player.width;
    if (nextY > levelHeight - player.height) nextY = levelHeight - player.height;

    // --------------------------------------------------
    // Resolve movement axis-by-axis
    // --------------------------------------------------
    if (canMoveTo(nextX, player.y)) {
        player.x = nextX;
    }

    if (canMoveTo(player.x, nextY)) {
        player.y = nextY;
    }
}

static void updateCamera(void) {
    hOff = player.x + (player.width / 2) - (SCREENWIDTH / 2);
    vOff = player.y + (player.height / 2) - (SCREENHEIGHT / 2);

    if (hOff < 0) {
        hOff = 0;
    }
    if (vOff < 0) {
        vOff = 0;
    }
    if (hOff > levelWidth - SCREENWIDTH) {
        hOff = levelWidth - SCREENWIDTH;
    }
    if (vOff > levelHeight - SCREENHEIGHT) {
        vOff = levelHeight - SCREENHEIGHT;
    }

    if (levelWidth < SCREENWIDTH) {
        hOff = 0;
    }
    if (levelHeight < SCREENHEIGHT) {
        vOff = 0;
    }

    cloudHOff = hOff / 3;
}

static int canMoveTo(int newX, int newY) {
    // Check all four corners of the player's hitbox.
    return !isBlockedPixel(currentLevel, newX, newY)
        && !isBlockedPixel(currentLevel, newX + player.width - 1, newY)
        && !isBlockedPixel(currentLevel, newX, newY + player.height - 1)
        && !isBlockedPixel(currentLevel, newX + player.width - 1, newY + player.height - 1);
}

static int onClimbTile(void) {
    int centerX = player.x + (player.width / 2);
    int centerY = player.y + (player.height / 2);

    return isClimbPixel(currentLevel, centerX, centerY);
}

static int touchesHazard(void) {
    int leftFootX  = player.x + 2;
    int rightFootX = player.x + player.width - 3;
    int footY      = player.y + player.height - 1;
    int midX       = player.x + (player.width / 2);
    int midY       = player.y + player.height - 4;

    return isHazardPixel(currentLevel, leftFootX, footY)
        || isHazardPixel(currentLevel, rightFootX, footY)
        || isHazardPixel(currentLevel, midX, midY);
}

static void handleLevelTransitions(void) {
    // Sample the center-bottom of the player.
    // This works well for doorway / portal / transition tiles.
    int probeX = player.x + (player.width / 2);
    int probeY = player.y + player.height - 1;

    // --------------------------------------------------
    // HOME -> LEVEL ONE
    // Only trigger if we are currently in HOME and we
    // touch collision index 4.
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME && isHomeToLevel1Pixel(currentLevel, probeX, probeY)) {
        goToLevelOne(0);
        return;
    }

    // --------------------------------------------------
    // LEVEL ONE -> HOME
    // Only trigger if we are currently in LEVEL ONE and
    // we touch collision index 5.
    // --------------------------------------------------
    if (currentLevel == LEVEL_ONE && isLevel1ToHomePixel(currentLevel, probeX, probeY)) {
        goToHome(0);
        return;
    }

    // --------------------------------------------------
    // HOME -> LEVEL TWO
    // Only trigger if we are currently in HOME and we
    // touch collision index 6.
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME && isHomeToLevel2Pixel(currentLevel, probeX, probeY)) {
        goToLevelTwo(0);
        return;
    }

    // --------------------------------------------------
    // LEVEL TWO -> HOME
    // Only trigger if we are currently in LEVEL TWO and
    // we touch collision index 7.
    // --------------------------------------------------
    if (currentLevel == LEVEL_TWO && isLevel2ToHomePixel(currentLevel, probeX, probeY)) {
        goToHome(0);
        return;
    }
}

static void drawGameplay(void) {
    clearHud();
    drawHudText();

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, hOff, vOff);

    drawSprites();
}

static void drawSprites(void) {
    int screenX = player.x - hOff;
    int screenY = player.y - vOff;

    // The new player is a single 16x32 OBJ. Hide it cleanly if it is fully
    // offscreen. Also hide the three old helper sprites so stale art from the
    // previous 24x48 setup never shows up.
    if (screenX < -player.width || screenX >= SCREENWIDTH ||
        screenY < -player.height || screenY >= SCREENHEIGHT) {
        hideSprite(0);
    } else {
        shadowOAM[0].attr0 = ATTR0_Y(screenY) | ATTR0_TALL | ATTR0_4BPP;
        shadowOAM[0].attr1 = ATTR1_X(screenX) | ATTR1_MEDIUM;

        if (player.facingLeft) {
            shadowOAM[0].attr1 |= ATTR1_HFLIP;
        }

        shadowOAM[0].attr2 = ATTR2_TILEID(OBJ_TILE_PLAYER)
            | ATTR2_PRIORITY(0)
            | ATTR2_PALROW(PLAYER_PALROW);
    }

    // Hide leftover sprite slots from the old multi-piece player.
    hideSprite(1);
    hideSprite(2);
    hideSprite(3);

    // Hide everything else unless you later add more gameplay sprites.
    for (int i = 4; i < 128; i++) {
        hideSprite(i);
    }
}

static void hideSprite(int index) {
    shadowOAM[index].attr0 = ATTR0_HIDE;
    shadowOAM[index].attr1 = 0;
    shadowOAM[index].attr2 = 0;
}

static void drawHudText(void) {
    if (currentLevel == LEVEL_HOME) {
        putText(1, 1, "HOMEBASE");
    } else if (currentLevel == LEVEL_ONE) {
        putText(1, 1, "LEVEL 1");
    } else {
        putText(1, 1, "LEVEL 2");
    }

    putText(1, 3, "SEL+DOWN  HOME");
    putText(1, 5, "SEL+LEFT  LV1");
    putText(1, 7, "SEL+RIGHT LV2");
}

static void drawMenuScreen(const char* title, const char* line1, const char* line2, const char* line3) {
    // This function should only DRAW the menu screen.
    // It must never change game state, otherwise the title / instructions
    // screens constantly stomp each other and input appears broken.
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