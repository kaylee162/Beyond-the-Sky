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
static void enableGameplayDisplay(void);
static void enableMenuDisplay(void);
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
static void updatePlayerAnimation(void);
static void updateCamera(void);
static void drawGameplay(void);
static void drawSprites(void);
static void hideSprite(int index);

// Collision / interaction helpers
static int canMoveTo(int newX, int newY);
static int onClimbTile(void);
static int onClimbTileAt(int x, int y);
static int isStandingOnSolid(void);
static int findStandingSpawnY(int spawnX, int preferredTopY);
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

    // Clear OBJ tile memory so stale sprite graphics do not appear.
    static unsigned short zero = 0;
    DMANow(3, &zero, (volatile unsigned short*)0x6010000, (32 * 32) | DMA_SOURCE_FIXED);

    // --------------------------------------------------
    // Copy all 4 directions x 4 frames from the exported sheet.
    //
    // Each frame is 2x4 tiles (16x32 pixels).
    // Rows are stacked directly with no empty space between them.
    //
    // Right row starts at tile y = 0
    // Left  row starts at tile y = 4
    // Up    row starts at tile y = 8
    // Down  row starts at tile y = 12
    //
    // Each frame is 2 tiles wide, so frame X positions are:
    // 0, 2, 4, 6
    // --------------------------------------------------
    for (int frame = 0; frame < PLAYER_ANIM_FRAMES; frame++) {
        int srcTileX = frame * 2;

        // Right
        copySpriteBlockFromSheet(
            spriteSheetTiles, 32,
            srcTileX, 0,
            2, 4,
            OBJ_TILE_PLAYER_RIGHT + frame * PLAYER_TILES_PER_FRAME
        );

        // Left
        copySpriteBlockFromSheet(
            spriteSheetTiles, 32,
            srcTileX, 4,
            2, 4,
            OBJ_TILE_PLAYER_LEFT + frame * PLAYER_TILES_PER_FRAME
        );

        // Up / climbing up
        copySpriteBlockFromSheet(
            spriteSheetTiles, 32,
            srcTileX, 8,
            2, 4,
            OBJ_TILE_PLAYER_UP + frame * PLAYER_TILES_PER_FRAME
        );

        // Down / falling / climbing down
        copySpriteBlockFromSheet(
            spriteSheetTiles, 32,
            srcTileX, 12,
            2, 4,
            OBJ_TILE_PLAYER_DOWN + frame * PLAYER_TILES_PER_FRAME
        );
    }
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
            drawMenuScreen("BEYOND THE SKY", "PRESS START", "A: NEXT   B: BACK", "SELECT+LEFT/RIGHT/DOWN = MAP CHEATS");
            break;

        case STATE_INSTRUCTIONS:
            clearHud();
            drawMenuScreen("INSTRUCTIONS", "ARROWS MOVE", "A JUMP  START PAUSE", "SELECT+LEFT/RIGHT/DOWN TO WARP MAPS");
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
            break;

        case STATE_LOSE:
            clearHud();
            drawMenuScreen("YOU LOSE", "PRESS START TO RESPAWN", "", "");
            break;
    }
}

static void initGraphics(void) {
    initMode0();

    // Start with full gameplay layers enabled by default.
    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1) | BG_ENABLE(2)
        | SPRITE_ENABLE | SPRITE_MODE_1D;

    // BG0 = HUD / menu text layer
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

static void enableGameplayDisplay(void) {
    // Gameplay needs:
    // BG0 = HUD text
    // BG1 = background tilemap
    // BG2 = foreground tilemap
    // OBJ = sprites
    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1) | BG_ENABLE(2)
        | SPRITE_ENABLE | SPRITE_MODE_1D;
}

static void enableMenuDisplay(void) {
    // Menu/state screens only need BG0 text.
    // The blue backdrop color will fill the rest of the screen.
    REG_DISPCTL = MODE(0) | BG_ENABLE(0);

    // Keep menu text positioned correctly.
    setBackgroundOffset(0, 0, 0);

    // Hide sprites so stale OBJ art never lingers on menu screens.
    hideSprites();
}

static void goToStart(void) {
    state = STATE_START;
    enableMenuDisplay();
    clearHud();
    drawMenuScreen("BEYOND THE SKY", "PRESS START", "A: NEXT   B: BACK", "SELECT+UP TOGGLE GROW CHEAT");
}

static void goToInstructions(void) {
    state = STATE_INSTRUCTIONS;
    enableMenuDisplay();
    clearHud();
    drawMenuScreen("INSTRUCTIONS", "LEFT/RIGHT MOVE  A JUMP", "UP/DOWN CLIMB  START PAUSE", "GET WATER IN SKY LEVEL, RETURN, PRESS B ON SOIL");
}

static void goToHome(int respawn) {
    state = STATE_HOME;
    enableGameplayDisplay();
    currentLevel = LEVEL_HOME;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;

    if (!respawn) {
        // Default home spawn used for fresh entry.
        player.x = HOME_SPAWN_X;
        player.y = findStandingSpawnY(player.x, levelHeight - (8 * 8));
    } else {
        player.x = respawnX;
        // If no exact Y was supplied, find a valid standing position now
        // after the home map has already loaded.
        if (respawnY < 0) {
            player.y = findStandingSpawnY(player.x, levelHeight - (8 * 8));
        } else {
            player.y = respawnY;
        }
    }

    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;
    player.direction = DIR_RIGHT;
    player.animFrame = 0;
    player.animCounter = 0;
    player.isMoving = 0;
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

static void goToLevelOne(int respawn) {
    state = STATE_LEVEL1;
    enableGameplayDisplay();
    currentLevel = LEVEL_ONE;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;

    if (!respawn) {
        // Spawn on the ground near the left side of the map.
        // Moved slightly LEFT from the previous position.
        player.x = LEVEL1_SPAWN_X;
        player.y = findStandingSpawnY(player.x, levelHeight - (8 * 8));
    } else {
        player.x = respawnX;
        player.y = respawnY;
    }

    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;
    player.direction = DIR_LEFT;
    player.animFrame = 0;
    player.animCounter = 0;
    player.isMoving = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    respawnX = player.x;
    respawnY = player.y;

    hOff = levelWidth - SCREENWIDTH;
    if (hOff < 0) hOff = 0;

    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, hOff, vOff);
}

static void goToLevelTwo(int respawn) {
    state = STATE_LEVEL2;
    enableGameplayDisplay();
    currentLevel = LEVEL_TWO;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;

    if (!respawn) {
        // Spawn on the ground near the left side of the map.
        // Moved slightly RIGHT from the previous position.
        player.x = LEVEL2_SPAWN_X;
        player.y = findStandingSpawnY(player.x, 6 * 8);
    } else {
        player.x = respawnX;
        player.y = respawnY;
    }

    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;
    player.direction = DIR_RIGHT;
    player.animFrame = 0;
    player.animCounter = 0;
    player.isMoving = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    respawnX = player.x;
    respawnY = player.y;

    hOff = 0;
    vOff = player.y + (player.height / 2) - (SCREENHEIGHT / 2);

    if (vOff < 0) vOff = 0;
    if (vOff > levelHeight - SCREENHEIGHT) {
        vOff = levelHeight - SCREENHEIGHT;
    }

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, hOff, vOff);
}

static void goToPause(void) {
    gameplayStateBeforePause = state;
    state = STATE_PAUSE;
    enableMenuDisplay();
    clearHud();
    drawPauseScreen();
}

static void goToWin(void) {
    state = STATE_WIN;
    enableMenuDisplay();
    clearHud();
    drawMenuScreen("YOU WIN", "THE BEANSTALK BLOOMED", "PRESS START TO PLAY AGAIN", instantGrowCheat ? "CHEAT MODE WAS ON" : "CHEAT MODE WAS OFF");
}

static void goToLose(int levelToReturnTo) {
    loseReturnLevel = levelToReturnTo;
    state = STATE_LOSE;
    enableMenuDisplay();
    clearHud();
    drawMenuScreen("YOU WILTED", "HAZARD HIT OR FALL RESET", "PRESS START TO RESPAWN", "THIS STILL COUNTS AS YOUR RESET/KILL OBSTACLE");
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
        enableGameplayDisplay();
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

    // Optional cheat toggle
    if (BUTTON_PRESSED(BUTTON_SELECT) && BUTTON_HELD(BUTTON_UP)) {
        instantGrowCheat = !instantGrowCheat;
    }

    // Move the player first
    updatePlayerMovement();

    // Then update the animation based on the movement result
    updatePlayerAnimation();

    // Check for transition tiles after movement
    handleLevelTransitions();

    // Update camera after movement / transitions
    updateCamera();

    // Hazard / death check
    if (touchesHazard() || player.y > levelHeight + 16) {
        goToLose(currentLevel);
    }
}

static void updatePlayerAnimation(void) {
    // When the player is not actively walking/climbing,
    // return to the idle frame.
    if (!player.isMoving) {
        player.animFrame = 0;
        player.animCounter = 0;
        return;
    }

    // Advance animation every few game frames.
    // Lower number = faster animation.
    player.animCounter++;
    if (player.animCounter >= 6) {
        player.animCounter = 0;
        player.animFrame = (player.animFrame + 1) % PLAYER_ANIM_FRAMES;
    }
}

static void updatePlayerMovement(void) {
    int dx = 0;
    int step;
    int remaining;
    int wantsClimbUp;
    int wantsClimbDown;
    int wasActivelyMoving = 0;

    player.oldX = player.x;
    player.oldY = player.y;

    // --------------------------------------------------
    // Check whether the player is standing on solid ground
    // --------------------------------------------------
    player.grounded = isStandingOnSolid();

    // --------------------------------------------------
    // Read movement input
    // --------------------------------------------------
    if (BUTTON_HELD(BUTTON_LEFT)) {
        dx -= MOVE_SPEED;
        player.direction = DIR_LEFT;
    }

    if (BUTTON_HELD(BUTTON_RIGHT)) {
        dx += MOVE_SPEED;
        player.direction = DIR_RIGHT;
    }

    wantsClimbUp = BUTTON_HELD(BUTTON_UP);
    wantsClimbDown = BUTTON_HELD(BUTTON_DOWN);

    // --------------------------------------------------
    // Climbing logic
    // --------------------------------------------------
    if (wantsClimbUp && onClimbTile()) {
        player.climbing = 1;
        player.yVel = 0;
        player.grounded = 0;
    } else if (!onClimbTile()) {
        player.climbing = 0;
    }

    // --------------------------------------------------
    // Jump logic
    // --------------------------------------------------
    if (!player.climbing && BUTTON_PRESSED(BUTTON_UP) && player.grounded && !onClimbTile()) {
        player.yVel = JUMP_VEL;
        player.grounded = 0;
    }

    // --------------------------------------------------
    // Gravity / falling
    // --------------------------------------------------
    if (!player.climbing) {
        if (!player.grounded || player.yVel < 0) {
            player.yVel += GRAVITY;
            if (player.yVel > MAX_FALL_SPEED) {
                player.yVel = MAX_FALL_SPEED;
            }
        } else {
            player.yVel = 0;
        }
    }

    // --------------------------------------------------
    // Horizontal movement resolution, 1 pixel at a time
    // --------------------------------------------------
    if (dx != 0) {
        step = (dx > 0) ? 1 : -1;
        remaining = (dx > 0) ? dx : -dx;

        while (remaining > 0) {
            if (canMoveTo(player.x + step, player.y)) {
                player.x += step;
            } else {
                break;
            }
            remaining--;
        }
    }

    // --------------------------------------------------
    // Vertical movement while climbing
    // --------------------------------------------------
    if (player.climbing) {
        int climbDy = 0;

        if (wantsClimbUp) {
            climbDy -= CLIMB_SPEED;
        }
        if (wantsClimbDown) {
            climbDy += CLIMB_SPEED;
        }

        if (climbDy != 0) {
            step = (climbDy > 0) ? 1 : -1;
            remaining = (climbDy > 0) ? climbDy : -climbDy;

            while (remaining > 0) {
                if (canMoveTo(player.x, player.y + step) &&
                    onClimbTileAt(player.x, player.y + step)) {
                    player.y += step;
                } else {
                    break;
                }
                remaining--;
            }
        }
    } else {
        // --------------------------------------------------
        // Vertical movement from jump / gravity
        // --------------------------------------------------
        if (player.yVel != 0) {
            step = (player.yVel > 0) ? 1 : -1;
            remaining = (player.yVel > 0) ? player.yVel : -player.yVel;

            while (remaining > 0) {
                if (canMoveTo(player.x, player.y + step)) {
                    player.y += step;
                } else {
                    if (step > 0) {
                        player.grounded = 1;
                    }
                    player.yVel = 0;
                    break;
                }
                remaining--;
            }
        }
    }

    // --------------------------------------------------
    // Clamp to world bounds
    // --------------------------------------------------
    if (player.x < 0) {
        player.x = 0;
    }
    if (player.y < 0) {
        player.y = 0;
    }
    if (player.x > levelWidth - player.width) {
        player.x = levelWidth - player.width;
    }
    if (player.y > levelHeight - player.height) {
        player.y = levelHeight - player.height;
    }

    // --------------------------------------------------
    // Final grounded check
    // --------------------------------------------------
    if (!player.climbing) {
        player.grounded = isStandingOnSolid();
        if (player.grounded && player.yVel > 0) {
            player.yVel = 0;
        }
    }

    // --------------------------------------------------
    // Decide which animation row to use
    // --------------------------------------------------

    // Walking left/right
    if (dx < 0) {
        player.direction = DIR_LEFT;
    } else if (dx > 0) {
        player.direction = DIR_RIGHT;
    }

    // Climbing overrides ground walk direction
    if (player.climbing) {
        if (wantsClimbUp && player.y < player.oldY) {
            player.direction = DIR_UP;
        } else if (wantsClimbDown && player.y > player.oldY) {
            player.direction = DIR_DOWN;
        }
    }
    // Falling / airborne uses the down row
    else if (!player.grounded) {
        player.direction = DIR_DOWN;
    }

    // --------------------------------------------------
    // Decide whether to animate this frame
    //
    // Animate while:
    // - walking left/right on purpose
    // - climbing up/down on purpose
    //
    // Do not animate passive falling unless you want that later.
    // --------------------------------------------------
    if ((dx != 0 && player.x != player.oldX) ||
        (player.climbing && wantsClimbUp && player.y < player.oldY) ||
        (player.climbing && wantsClimbDown && player.y > player.oldY)) {
        wasActivelyMoving = 1;
    }

    player.isMoving = wasActivelyMoving;
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
    return !isBlockedPixel(currentLevel, newX, newY)
        && !isBlockedPixel(currentLevel, newX + player.width - 1, newY)
        && !isBlockedPixel(currentLevel, newX, newY + player.height - 1)
        && !isBlockedPixel(currentLevel, newX + player.width - 1, newY + player.height - 1);
}

static int findStandingSpawnY(int spawnX, int preferredTopY) {
    int y;
    int clampedX;
    int clampedPreferredY;
    int leftFootX;
    int rightFootX;

    // Keep x inside the world
    clampedX = CLAMP(spawnX, 0, levelWidth - player.width);

    // Keep preferred y inside the world
    clampedPreferredY = CLAMP(preferredTopY, 0, levelHeight - player.height);

    leftFootX = clampedX + 2;
    rightFootX = clampedX + player.width - 3;

    // First search downward from the preferred position
    // until we find a valid open spot with solid ground under the feet.
    for (y = clampedPreferredY; y <= levelHeight - player.height; y++) {
        if (canMoveTo(clampedX, y) &&
            (y + player.height >= levelHeight ||
             isBlockedPixel(currentLevel, leftFootX, y + player.height) ||
             isBlockedPixel(currentLevel, rightFootX, y + player.height))) {
            return y;
        }
    }

    // If that failed, search upward.
    for (y = clampedPreferredY - 1; y >= 0; y--) {
        if (canMoveTo(clampedX, y) &&
            (y + player.height >= levelHeight ||
             isBlockedPixel(currentLevel, leftFootX, y + player.height) ||
             isBlockedPixel(currentLevel, rightFootX, y + player.height))) {
            return y;
        }
    }

    // Fallback: just clamp it.
    return clampedPreferredY;
}

static int onClimbTile(void) {
    return onClimbTileAt(player.x, player.y);
}

static int onClimbTileAt(int x, int y) {
    int centerX = x + (player.width / 2);

    // Sample multiple points vertically through the player's body.
    // This is much more reliable for ladders / beanstalks than checking
    // only one center point.
    int topY    = y + 2;
    int upperY  = y + 8;
    int midY    = y + (player.height / 2);
    int lowerY  = y + player.height - 8;
    int footY   = y + player.height - 2;

    return isClimbPixel(currentLevel, centerX, topY)
        || isClimbPixel(currentLevel, centerX, upperY)
        || isClimbPixel(currentLevel, centerX, midY)
        || isClimbPixel(currentLevel, centerX, lowerY)
        || isClimbPixel(currentLevel, centerX, footY);
}

static int isStandingOnSolid(void) {
    int leftFootX  = player.x + 2;
    int rightFootX = player.x + player.width - 3;
    int belowY     = player.y + player.height;

    if (belowY >= levelHeight) {
        return 1;
    }

    return isBlockedPixel(currentLevel, leftFootX, belowY)
        || isBlockedPixel(currentLevel, rightFootX, belowY);
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
    // Sample the player's feet area for transition tiles.
    // Checking left / center / right is more reliable than a single point.
    int leftX   = player.x + 2;
    int centerX = player.x + (player.width / 2);
    int rightX  = player.x + player.width - 3;
    int probeY  = player.y + player.height - 1;

    // --------------------------------------------------
    // HOME -> LEVEL ONE
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME &&
        (isHomeToLevel1Pixel(currentLevel, leftX, probeY) ||
         isHomeToLevel1Pixel(currentLevel, centerX, probeY) ||
         isHomeToLevel1Pixel(currentLevel, rightX, probeY))) {
        goToLevelOne(0);
        return;
    }

    // --------------------------------------------------
    // LEVEL ONE -> HOME
    // Return near the Level 1 doorway area in home.
    // --------------------------------------------------
    if (currentLevel == LEVEL_ONE &&
        (isLevel1ToHomePixel(currentLevel, leftX, probeY) ||
         isLevel1ToHomePixel(currentLevel, centerX, probeY) ||
         isLevel1ToHomePixel(currentLevel, rightX, probeY))) {
        respawnX = HOME_FROM_LEVEL1_SPAWN_X;
        respawnY = -1;   // let goToHome() compute a safe standing Y
        goToHome(1);
        return;
    }

    // --------------------------------------------------
    // HOME -> LEVEL TWO
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME &&
        (isHomeToLevel2Pixel(currentLevel, leftX, probeY) ||
         isHomeToLevel2Pixel(currentLevel, centerX, probeY) ||
         isHomeToLevel2Pixel(currentLevel, rightX, probeY))) {
        goToLevelTwo(0);
        return;
    }

    // --------------------------------------------------
    // LEVEL TWO -> HOME
    // Return near the Level 2 doorway area in home.
    // --------------------------------------------------
    if (currentLevel == LEVEL_TWO &&
        (isLevel2ToHomePixel(currentLevel, leftX, probeY) ||
        isLevel2ToHomePixel(currentLevel, centerX, probeY) ||
        isLevel2ToHomePixel(currentLevel, rightX, probeY))) {

        // Return to the Level 2 doorway area in homebase.
        // X is about 30 tiles.
        // Y is about 42 tiles up, adjusted so the player's feet land there.
        respawnX = HOME_FROM_LEVEL2_SPAWN_X;
        respawnY = HOME_FROM_LEVEL2_SPAWN_Y - player.height;
        goToHome(1);
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

        if (screenX < -player.width || screenX >= SCREENWIDTH ||
            screenY < -player.height || screenY >= SCREENHEIGHT) {
            hideSprite(0);
        } else {
            int tileBase;

            // Pick the correct directional animation row.
            if (player.direction == DIR_LEFT) {
                tileBase = OBJ_TILE_PLAYER_LEFT + player.animFrame * PLAYER_TILES_PER_FRAME;
            } else if (player.direction == DIR_UP) {
                tileBase = OBJ_TILE_PLAYER_UP + player.animFrame * PLAYER_TILES_PER_FRAME;
            } else if (player.direction == DIR_DOWN) {
                tileBase = OBJ_TILE_PLAYER_DOWN + player.animFrame * PLAYER_TILES_PER_FRAME;
            } else {
                tileBase = OBJ_TILE_PLAYER_RIGHT + player.animFrame * PLAYER_TILES_PER_FRAME;
            }

            // 16x32 sprite = TALL + MEDIUM
            shadowOAM[0].attr0 = ATTR0_Y(screenY) | ATTR0_TALL | ATTR0_4BPP;
            shadowOAM[0].attr1 = ATTR1_X(screenX) | ATTR1_MEDIUM;
            shadowOAM[0].attr2 = ATTR2_TILEID(tileBase)
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

    putText(1, 3, "UP JUMP");
    putText(1, 5, "A+VINE CLIMB");
    putText(1, 7, "START PAUSE");
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