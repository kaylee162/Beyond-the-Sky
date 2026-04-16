#include <string.h>
#include "game.h"
#include "tileset.h"
#include "spriteSheet.h"
#include "homebase_foreground.h"
#include "title.h"

// ======================================================
//                FILE-SCOPE GAME STATE
// ======================================================
// This file keeps nearly all gameplay state at file scope so the update and
// draw pipeline can share the same data each frame without passing large
// structs around between helpers 

static GameState state;
static GameState gameplayStateBeforePause;
static int currentLevel;
static int levelWidth;
static int levelHeight;

static Player player;

// World collectibles
static ResourceItem beanSprout;
static ResourceItem bonemeal;
static ResourceItem waterDroplet;

// Enemy bees
static Bee bees[3];
static int beeCount;

static int hOff;
static int vOff;
static int cloudScrollX;
static int frameCounter;

// Inventory bitmask.
static int inventoryFlags;

// Beanstalk growth progression:
//   0 = empty farmland
//   1 = bean sprout deposited
//   2 = bonemeal deposited (partial growth)
//   3 = water deposited (full growth)
static int beanstalkGrowthStage;

static int respawnX;
static int respawnY;
static int respawnPreferredY;
static int loseReturnLevel;

// Cheats
int cheatModeEnabled;
int invincibilityCheat;

// --------------------------------------------------
// FORWARD DECLARATIONS
// --------------------------------------------------
static void initGraphics(void);
static void initBgAssets(void);
static void initObjAssets(void);
static void copySpriteBlockFromSheet(const unsigned short* sheetTiles, int sheetWidthTiles,
    int srcTileX, int srcTileY, int blockWidthTiles, int blockHeightTiles, int dstTileIndex);
static void drawHudText(void);
static void drawPauseScreen(void);
static void drawTitleScreen(void);
static void drawInstructionsPage(void);
static void enableGameplayDisplay(void);
static void enableMenuDisplay(void);
static void loadMenuBackground(void);
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

static void initBeanSprout(void);
static void initBonemeal(void);
static void initWaterDroplet(void);
static void initBee(Bee* bee, int x, int y, int patrolRange, int startDir);
static void initLevelBees(void);
static void updateBees(void);
static void updateCollectibles(void);

static void updateCollectibleAnimations(void);
static int rectsOverlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh);
static const char* getInventoryText(void);
static void giveNextResourceCheat(void);
static unsigned short getHomeForegroundSourceEntry(int tileX, int tileY);
static void setHomeForegroundTileEntry(int tileX, int tileY, unsigned short entry);
static void drawHomeTileBlockFromTileset(int mapTileX, int mapTileY, int srcTileX, int srcTileY, int widthTiles, int heightTiles);
static void refreshHomeBeanstalkVisuals(void);
static int isPlayerAtFarmland(void);
static void tryDepositResource(void);
static int canUseCurrentClimbPixel(int x, int y);
static int beeBodyHitsSolidAt(int x, int y);
static int canBeeMoveTo(int newX, int newY);
static int getCloudMapWidth(void);
static void applyBackgroundOffsets(void);

// sky palette/colors 
static unsigned short lerpSkyColor(unsigned short startColor, unsigned short endColor, int step, int totalSteps);
static void updateDaytimePalette(void);
static void setMenuPalette(void);

// Collision / interaction helpers
static int bodyHitsSolidAt(int x, int y);
static int canMoveTo(int newX, int newY);
static int onClimbTile(void);
static int onClimbTileAt(int x, int y);
static int isAtTopOfClimb(int x, int y);
static int tryStartLadderTopExit(void);
static int isStandingOnSolid(void);
static int findStandingSpawnY(int spawnX, int preferredTopY);
static int touchesHazard(void);
static int playerTouchesBee(void);
static int fellOutOfLevel(void);
static void recoverFromInvincibleFall(void);
static int touchesWinTile(void);
static void handleLevelTransitions(void);
static void putText(int col, int row, const char* str);
static void clearHud(void);

// Tracks which instructions page is currently being shown.
static int instructionsPage = 0;

// --------------------------------------------------
// Copy a rectangular block of 8x8 OBJ tiles out of the full exported
// sprite sheet and repack it into a contiguous chunk in OBJ VRAM.
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

// ======================================================
//                RUNTIME DAYTIME PALETTE CYCLE
// ======================================================
// Instead of swapping out whole background graphics for each time of day,
// just change the sky at runtime by rewriting palette memory
static unsigned short lerpSkyColor(unsigned short startColor, unsigned short endColor, int step, int totalSteps) {
    // Linearly interpolate between two GBA 15-bit colors.
    // Each channel is 5 bits: RRRRRGGGGGBBBBB in the RGB() helper layout.
    int startR = startColor & 31;
    int startG = (startColor >> 5) & 31;
    int startB = (startColor >> 10) & 31;

    int endR = endColor & 31;
    int endG = (endColor >> 5) & 31;
    int endB = (endColor >> 10) & 31;

    int r = startR + ((endR - startR) * step) / totalSteps;
    int g = startG + ((endG - startG) * step) / totalSteps;
    int b = startB + ((endB - startB) * step) / totalSteps;

    return RGB(r, g, b);
}

static void updateDaytimePalette(void) {
    // Animate the backdrop color at runtime by rewriting BG palette index 0.
    // This creates a looping:
    // EARLY MORNING -> DAY -> SUNSET -> NIGHT -> EARLY MORNING

    const int segmentLength = DAYTIME_CYCLE_SEGMENT_FRAMES;
    const int totalCycleLength = segmentLength * 4;

    // Offset lets you choose which part of the cycle the game starts on.
    int cycleFrame = (frameCounter + DAYTIME_CYCLE_START_OFFSET_FRAMES) % totalCycleLength;
    int segmentFrame = cycleFrame % segmentLength;

    unsigned short startColor;
    unsigned short endColor;

    if (cycleFrame < segmentLength) {
        // Early morning -> day
        startColor = SKY_COLOR_EARLY_MORNING;
        endColor = SKY_COLOR_DAY;
    } else if (cycleFrame < segmentLength * 2) {
        // Day -> sunset
        startColor = SKY_COLOR_DAY;
        endColor = SKY_COLOR_SUNSET;
    } else if (cycleFrame < segmentLength * 3) {
        // Sunset -> night
        startColor = SKY_COLOR_SUNSET;
        endColor = SKY_COLOR_NIGHT;
    } else {
        // Night -> early morning
        startColor = SKY_COLOR_NIGHT;
        endColor = SKY_COLOR_EARLY_MORNING;
    }

    unsigned short currentSkyColor = lerpSkyColor(startColor, endColor, segmentFrame, segmentLength);

    // BG palette index 0 is the visible sky/backdrop color.
    BG_PALETTE[0] = currentSkyColor;

    // Keep the font row's transparent/background entry synced to the sky color
    // so HUD/menu text still blends into the background cleanly.
    BG_PALETTE[FONT_PALROW * 16 + 0] = currentSkyColor;
}

static void setMenuPalette(void) {
    // Restore the tileset's original palette color at BG palette index 0
    // so the menu background tilemap uses the intended artwork colors.
    BG_PALETTE[0] = tilesetPal[0];

    // Match the font background entry to the same palette color so text
    // still blends correctly with the tilemap behind it.
    BG_PALETTE[FONT_PALROW * 16 + 0] = tilesetPal[0];
}

// ======================================================
//                ASSET LOADING AND SETUP
// ======================================================
static void initBgAssets(void) {
    // Load shared background art and reserve a palette row for font text.
    // Load the tileset palette and tiles for the gameplay backgrounds.
    DMANow(3, (void*)tilesetPal, BG_PALETTE, tilesetPalLen / 2);
    DMANow(3, (void*)tilesetTiles, CHARBLOCK[1].tileimg, tilesetTilesLen / 2);

    // Give the HUD/menu font its own dedicated palette row so the text does
    // not inherit random colors from the tileset palette.
    BG_PALETTE[FONT_PALROW * 16 + 1] = FONT_COLOR;

    // Initialize the runtime sky color immediately so the first frame starts
    // with the correct palette-modified backdrop.
    updateDaytimePalette();
}

static void initObjAssets(void) {
    // Load sprite palettes/tiles into OBJ memory and repack animation frames.
    // --------------------------------------------------
    // Load OBJ palette rows explicitly:
    //   palette row 0 = player
    //   palette row 1 = all other sprites
    // --------------------------------------------------
    DMANow(3, (void*)spriteSheetPal,      &SPRITE_PAL[16 * PLAYER_PALROW], 16);
    DMANow(3, (void*)(spriteSheetPal + 16), &SPRITE_PAL[16 * WORLD_SPRITE_PALROW], 16);

    // Clear OBJ tile memory so stale sprite graphics do not appear.
    static unsigned short zero = 0;
    DMANow(3, &zero, (volatile unsigned short*)0x6010000, (32 * 32) | DMA_SOURCE_FIXED);
    // --------------------------------------------------
    // Copy all 4 directions x 4 frames from the exported sheet.
    //
    // Each frame is 2x4 tiles (16x32 pixels).
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

    // --------------------------------------------------
    // Copy bean sprout sprite art
    //
    // Sheet position:
    //   top-left tile = (0, 21)
    // Size:
    //   2 tiles wide x 3 tiles tall
    //
    // This copies the 6 bean sprout tiles contiguously into OBJ VRAM.
    // draw them as:
    //   top    16x16 using tiles 0..3 of this block
    //   bottom 16x8  using tiles 4..5 of this block
    // --------------------------------------------------
    copySpriteBlockFromSheet(
        spriteSheetTiles, 32,
        0, 21,
        2, 3,
        OBJ_TILE_BEAN_SPROUT
    );

    // --------------------------------------------------
    // Copy bonemeal sprite art
    //
    // Frame 0 top-left = (0, 19)
    // Frame 1 top-left = (2, 19)
    // Each frame is 2x2 tiles = 16x16
    // --------------------------------------------------
    copySpriteBlockFromSheet(
        spriteSheetTiles, 32,
        0, 19,
        2, 2,
        OBJ_TILE_BONEMEAL
    );

    copySpriteBlockFromSheet(
        spriteSheetTiles, 32,
        2, 19,
        2, 2,
        OBJ_TILE_BONEMEAL + RESOURCE_TILES_PER_FRAME
    );

    // --------------------------------------------------
    // Copy water droplet sprite art
    //
    // Frame 0 top-left = (3, 19)
    // Frame 1 top-left = (5, 19)
    // Each frame is 2x2 tiles = 16x16
    // --------------------------------------------------
    copySpriteBlockFromSheet(
        spriteSheetTiles, 32,
        4, 19,
        2, 2,
        OBJ_TILE_WATER
    );

    copySpriteBlockFromSheet(
        spriteSheetTiles, 32,
        6, 19,
        2, 2,
        OBJ_TILE_WATER + RESOURCE_TILES_PER_FRAME
    );

    // --------------------------------------------------
    // Copy bee enemy sprite art
    //
    // Frame 0 top-left = (0, 16)
    // Frame 1 top-left = (2, 16)
    // Frame 2 top-left = (4, 16)
    // Each frame is 2x3 tiles = 16x24
    // --------------------------------------------------
    for (int frame = 0; frame < BEE_ANIM_FRAMES; frame++) {
        copySpriteBlockFromSheet(
            spriteSheetTiles, 32,
            frame * 2, 16,
            2, 3,
            OBJ_TILE_BEE + frame * BEE_TILES_PER_FRAME
        );
    }
}

// ======================================================
//                GAME LIFECYCLE ENTRY POINTS
// ======================================================
void initGame(void) {
    // Reset game-wide variables, load the initial level data, and enter the title screen.
    initGraphics();

    frameCounter = 0;
    inventoryFlags = 0;
    beanstalkGrowthStage = 0;
    invincibilityCheat = 0;
    cheatModeEnabled = 0;
    beeCount = 0;

    currentLevel = LEVEL_HOME;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    loadLevelMaps(currentLevel);
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);
    setBackgroundOffset(2, 0, 0);

    cloudScrollX = 0;

    goToStart();
}

void updateGame(void) {
    // Run exactly one update function based on the active game state.
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
    // Draw the current screen without changing any gameplay state.
    switch (state) {
        case STATE_START:
            drawTitleScreen();
            break;

        case STATE_INSTRUCTIONS:
            drawInstructionsPage();
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
            putText(12, 8, "YOU WIN"); 
            putText(2, 12, "PRESS START TO PLAY AGAIN");
            break;

        case STATE_LOSE:
            clearHud();
            putText(10, 8, "YOU LOSE"); 
            putText(5, 12, "PRESS START TO RESPAWN");
            break;
    }
}

// ======================================================
//                DISPLAY MODE AND BACKGROUND SETUP
// ======================================================
static void initGraphics(void) {
    // Initialize Mode 0, background layers, font data, and sprite assets.
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

static void loadMenuBackground(void) {
    // Use BG1 as a static decorative background for all menu/state screens.
    // This map was built with the same tileset already loaded into charblock 1,
    // so we only need to configure BG1 and copy the tilemap into its screenblock.

    // Clear the screenblock first so no old map data lingers underneath.
    clearScreenblock(BG_BACK_SB_A);

    // BG1 will show a simple 32x32 tile map for the menu background.
    setupBackground(
        1,
        BG_PRIORITY(1) | BG_CHARBLOCK(1) | BG_SCREENBLOCK(BG_BACK_SB_A) | BG_4BPP | BG_SIZE_SMALL
    );

    // Copy the full 32x32 title/menu map into BG1's screenblock.
    DMANow(3, (void*)titleMap, SCREENBLOCK[BG_BACK_SB_A].tilemap, 1024);

    // Keep the menu background anchored to the screen.
    setBackgroundOffset(1, 0, 0);
}

static void enableGameplayDisplay(void) {
    // Turn on the background and sprite layers used during active gameplay.
    // BG0 = HUD text
    // BG1 = background tilemap
    // BG2 = foreground tilemap
    // OBJ = sprites
    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1) | BG_ENABLE(2)
        | SPRITE_ENABLE | SPRITE_MODE_1D;
}

static void enableMenuDisplay(void) {
    // Menu/state screens use:
    // BG0 = text
    // BG1 = decorative title/menu background tilemap
    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1);

    // Load the shared menu background each time we enter a menu-style state.
    loadMenuBackground();

    // Keep both visible menu layers fixed on screen.
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);

    // Hide sprites so stale OBJ art never lingers on menu screens.
    hideSprites();
}

// ======================================================
//                STATE / SCREEN TRANSITIONS
// ======================================================
static void goToStart(void) {
    // Switch to the title screen and reset the visible menu text.
    state = STATE_START;
    enableMenuDisplay();
    clearHud();
    drawTitleScreen();
    setMenuPalette();
}

static void goToInstructions(void) {
    // Switch to the instructions screen and start on page 1.
    state = STATE_INSTRUCTIONS;
    instructionsPage = 0;
    enableMenuDisplay();
    clearHud();
    drawInstructionsPage();
    setMenuPalette();
}

static void goToHome(int respawn) {
    // Load the home map, position the player, and reset moment-to-moment movement state.
    state = STATE_HOME;
    enableGameplayDisplay();
    currentLevel = LEVEL_HOME;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();
    updateDaytimePalette();

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;

    if (!respawn) {
        // Default home spawn used for fresh entry.
        player.x = HOME_SPAWN_X;
        player.y = findStandingSpawnY(player.x, levelHeight - (8 * 8));
    } else {
        player.x = respawnX;

        // If no exact Y was supplied, find a valid standing position now
        // using the preferred search height chosen by the transition.
        if (respawnY < 0) {
            player.y = findStandingSpawnY(player.x, respawnPreferredY);
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

    // Rebuild the bean sprout each time home is loaded.
    // If the player already collected it, initBeanSprout() will keep it hidden.
    initBeanSprout();
    beeCount = 0;

    // Reapply the dynamic farmland / beanstalk visuals every time home loads.
    refreshHomeBeanstalkVisuals();

    hOff = 0;
    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    // Keep the cloud layer vertically fixed and horizontally animated only.
    applyBackgroundOffsets();
}

static void goToLevelOne(int respawn) {
    // Load Level 1 and place the player at either a default or transition-based spawn.
    state = STATE_LEVEL1;
    enableGameplayDisplay();
    currentLevel = LEVEL_ONE;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();
    // Restore the animated sky palette for gameplay.
    updateDaytimePalette();

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;

    if (!respawn) {
        // Default/fallback Level 1 spawn.
        player.x = LEVEL1_FROM_HOME_BOTTOM_SPAWN_X;
        player.y = findStandingSpawnY(player.x, LEVEL1_FROM_HOME_BOTTOM_PREF_Y);
    } else {
        player.x = respawnX;

        // If no exact Y was supplied, compute a safe standing Y from the
        // preferred search height selected by the transition tile.
        if (respawnY < 0) {
            player.y = findStandingSpawnY(player.x, respawnPreferredY);
        } else {
            player.y = respawnY;
        }
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

    // Rebuild the Level 1 collectible each time the level is loaded.
    // If it was already collected, initBonemeal() keeps it hidden.
    initBonemeal();
    initLevelBees(); // initializes bees

    hOff = levelWidth - SCREENWIDTH;
    if (hOff < 0) hOff = 0;

    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    // Keep the cloud layer vertically fixed and horizontally animated only.
    applyBackgroundOffsets();
}

static void goToLevelTwo(int respawn) {
    // Load Level 2 and place the player near its intended entry point.
    state = STATE_LEVEL2;
    enableGameplayDisplay();
    currentLevel = LEVEL_TWO;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();
    // Restore the animated sky palette for gameplay.
    updateDaytimePalette();

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

    // Rebuild the Level 2 collectible each time the level is loaded.
    // If it was already collected, initWaterDroplet() keeps it hidden.
    initWaterDroplet();
    initLevelBees(); // initalizes bees

    hOff = 0;
    vOff = player.y + (player.height / 2) - (SCREENHEIGHT / 2);

    if (vOff < 0) vOff = 0;
    if (vOff > levelHeight - SCREENHEIGHT) {
        vOff = levelHeight - SCREENHEIGHT;
    }

    // Keep the cloud layer vertically fixed and horizontally animated only.
    applyBackgroundOffsets();
}

static void goToPause(void) {
    // Remember the gameplay state we came from, then open the pause screen.
    gameplayStateBeforePause = state;
    state = STATE_PAUSE;
    enableMenuDisplay();
    clearHud();
    drawPauseScreen();
    setMenuPalette();
}

static void goToWin(void) {
    // Switch to the win screen
    state = STATE_WIN;
    enableMenuDisplay();
    clearHud();
    setMenuPalette();
}

static void goToLose(int levelToReturnTo) {
    // Switch to the lose screen and remember which level should be reloaded on respawn.
    loseReturnLevel = levelToReturnTo;
    state = STATE_LOSE;
    enableMenuDisplay();
    clearHud();
    setMenuPalette();
}

static void respawnIntoCurrentLevel(void) {
    // Reload the level stored by the lose screen and respawn the player there.
    if (loseReturnLevel == LEVEL_HOME) {
        goToHome(1);
    } else if (loseReturnLevel == LEVEL_ONE) {
        goToLevelOne(1);
    } else {
        goToLevelTwo(1);
    }
}

// ======================================================
//                PER-STATE UPDATE FUNCTIONS
// ======================================================
static void updateStart(void) {
    // Title screen input: START opens the instructions screen.
    if (BUTTON_PRESSED(BUTTON_START)) {
        goToInstructions();
    }
}

static void updateInstructions(void) {
    // Instructions flow:
    // A = next
    // B = back
    // START = begin the game

    if (BUTTON_PRESSED(BUTTON_B)) {
        if (instructionsPage == 0) {
            // From page 1, go back to the title screen.
            goToStart();
        } else {
            // From page 2, go back to page 1.
            instructionsPage = 0;
            drawInstructionsPage();
            setMenuPalette();
        }
        return;
    }

    if (BUTTON_PRESSED(BUTTON_A)) {
        if (instructionsPage == 0) {
            // From page 1, go to page 2.
            instructionsPage = 1;
            drawInstructionsPage();
            setMenuPalette();
        }
        return;
    }

    if (BUTTON_PRESSED(BUTTON_START)) {
        // Start a completely fresh run from the home level.
        inventoryFlags = 0;
        beanstalkGrowthStage = 0;
        invincibilityCheat = 0;
        cheatModeEnabled = 0;

        // Restart the daytime cycle so a brand new game always begins at
        // early morning. This only happens on fresh game start, not on
        // normal level transitions.
        frameCounter = 0;

        goToHome(0);
    }
}

static void updateHome(void) {
    // Home currently uses the shared gameplay update path.
    updateGameplayCommon();
}

static void updateLevelOne(void) {
    // Level 1 currently uses the shared gameplay update path.
    updateGameplayCommon();
}

static void updateLevelTwo(void) {
    // Level 2 currently uses the shared gameplay update path.
    updateGameplayCommon();
}

static void updatePause(void) {
    // Pause input: resume gameplay or return to the title screen.
    if (BUTTON_PRESSED(BUTTON_START)) {
        // Go back to the gameplay state we were in before pausing.
        state = gameplayStateBeforePause;

        // Rebuild BG1/BG2 for the active level.
        // This is important because the pause screen reused BG1 for the
        // title/menu tilemap, so we must restore the real gameplay maps.
        configureMapBackgroundsForLevel(currentLevel);
        loadLevelMaps(currentLevel);

        // Turn gameplay layers back on.
        enableGameplayDisplay();

        // Restore the gameplay palette now that we are leaving the menu.
        updateDaytimePalette();

        // Reapply the current scroll offsets so the camera resumes exactly
        // where it was before pausing.
        applyBackgroundOffsets();

        // Redraw HUD text cleanly on BG0.
        clearHud();
    }

    if (BUTTON_PRESSED(BUTTON_SELECT)) {
        goToStart();
    }
}

static void updateLose(void) {
    // Lose screen input: respawn into the stored level.
    if (BUTTON_PRESSED(BUTTON_START) || BUTTON_PRESSED(BUTTON_A)) {
        respawnIntoCurrentLevel();
    }
}

static void updateWin(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        // Fresh run after winning.
        inventoryFlags = 0;
        beanstalkGrowthStage = 0;
        invincibilityCheat = 0;
        cheatModeEnabled = 0;

        // Reset the daytime cycle so the next new run starts at early morning.
        frameCounter = 0;

        goToStart();
    }
}

// ======================================================
//                CORE GAMEPLAY UPDATE HELPERS
// ======================================================
static void updateGameplayCommon(void) {
    // Handle the shared gameplay loop used by all playable levels.

    if (BUTTON_PRESSED(BUTTON_START)) {
        goToPause();
        return;
    }

    // SELECT + A gives the next resource needed for progression.
    // Order:
    //   1) bonemeal
    //   2) water droplet
    //
    // If bonemeal has already been obtained or deposited, this gives water.
    // Both resources can exist in the inventory at the same time.
    if (BUTTON_HELD(BUTTON_SELECT) && BUTTON_PRESSED(BUTTON_A)) {
        giveNextResourceCheat();
    }

    // Toggle cheat mode on/off with SELECT + B.
    // This is a latched toggle, so the player only has to press it once.
    if (BUTTON_HELD(BUTTON_SELECT) && BUTTON_PRESSED(BUTTON_B)) {
        cheatModeEnabled = !cheatModeEnabled;

        // Sync individual cheats to the master cheat mode toggle.
        invincibilityCheat = cheatModeEnabled;
    }

    // Move the player first
    updatePlayerMovement();

    // Update enemy movement after the player moves
    updateBees();

    // Check item pickups after movement.
    updateCollectibles();

    // Allow B to deposit the next required resource while standing on the farmland.
    tryDepositResource();

    // Animate any active world collectibles.
    updateCollectibleAnimations();

    // Then update the player animation based on the movement result.
    updatePlayerAnimation();

    // Check for transition tiles after movement
    handleLevelTransitions();

    // Update camera after movement / transitions
    updateCamera();

    // Win check
    if (touchesWinTile()) {
        goToWin();
        return;
    }

    // Hazard / death check
    //
    // Normal play:
    // - hazard tiles kill the player
    // - falling out of the level kills the player
    //
    // Invincibility cheat:
    // - hazard tiles are ignored
    // - in level two, the player cannot "fall out of the world" and die, so they are able
    // to just walk along the bottom
    if (invincibilityCheat) {
        if (fellOutOfLevel()) {
            recoverFromInvincibleFall();
            return;
        }
    } else if (touchesHazard() || fellOutOfLevel() || playerTouchesBee()) {        goToLose(currentLevel);
        return;
    }
}

static void updatePlayerAnimation(void) {
    // Advance or reset the player animation based on actual movement this frame.
    // When the player is not actively walking/climbing,
    // return to the idle frame.
    if (!player.isMoving) {
        player.animFrame = 0;
        player.animCounter = 0;
        return;
    }

    // Use a slower delay for walking, but keep climbing at the current speed.
    player.animCounter++;

    int animDelay = player.climbing ? 6 : 12;

    if (player.animCounter >= animDelay) {
        player.animCounter = 0;
        player.animFrame = (player.animFrame + 1) % PLAYER_ANIM_FRAMES;
    }
}

static void updatePlayerMovement(void) {
    // Resolve player input, climbing, jumping, gravity, and collision one pixel at a time.
    //
    // Movement is not applied in one big jump. Horizontal and vertical motion
    // are both resolved one pixel at a time, which makes collision much more
    // reliable around tile edges, short platforms, ladders, and ledges.
    int dx = 0;
    int step;
    int remaining;
    int wantsClimbUp;
    int wantsClimbDown;
    int wasActivelyMoving = 0;
    int onClimbNow;

    player.oldX = player.x;
    player.oldY = player.y;

    // Check whether the player is standing on solid ground
    player.grounded = isStandingOnSolid();

    // Read movement input
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
    onClimbNow = onClimbTile();

    // Climbing logic
    // The player enters climb mode only when their body overlaps a climb tile
    // and the player is actively pressing up or down. This avoids 'sticky'
    // ladder behavior where merely touching a ladder would steal control away
    // from normal platforming.
    if (onClimbNow && (wantsClimbUp || wantsClimbDown)) {
        player.climbing = 1;
        player.yVel = 0;
        player.grounded = 0;
    } else if (!onClimbNow) {
        player.climbing = 0;
    }

    // Jump logic
    // Normal jump only when not climbing.
    // Ladder-top exit is handled separately below.
    //
    // This separation is important: a standard jump and a ladder-top exit are
    // different situations. A normal jump starts from grounded platforming,
    // while the ladder-top exit is a special anti-stuck case triggered when
    // the lower body is still on the climb tile but the upper body has already
    // cleared it.
    if (!player.climbing && BUTTON_PRESSED(BUTTON_UP) && player.grounded) {
        player.yVel = JUMP_VEL;
        player.grounded = 0;
    }

    // Gravity / falling
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

    // Horizontal movement resolution, 1 pixel at a time
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

    // Vertical movement while climbing
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
                int nextY = player.y + step;

                // Normal climb movement: keep moving if the next position is
                // open and still overlaps climb tiles.
                if (canMoveTo(player.x, nextY) &&
                    onClimbTileAt(player.x, nextY)) {
                    player.y = nextY;
                }
                // Special case: climbing upward at the top of the ladder / vine.
                // Leave climb mode and give the player a mini upward launch.
                //
                // This is what lets the player get on top of the ladder instead
                // of getting trapped. Once the code detects that only the lower
                // body is still overlapping climb pixels, it pops the player up
                // to a clean non-climb position and gives a small upward boost
                // so the sprite lands naturally on the platform above.
                else if (step < 0 && wantsClimbUp && isAtTopOfClimb(player.x, player.y)) {
                    tryStartLadderTopExit();
                    wasActivelyMoving = 1;
                    break;
                } else {
                    break;
                }

                remaining--;
            }
        }

        // If the body no longer overlaps the climb tiles, leave climb mode
        if (!onClimbTile()) {
            player.climbing = 0;
            player.grounded = isStandingOnSolid();
        }
    } else {
        // Vertical movement from jump / gravity
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

    // Clamp to world bounds
    if (player.x < 0) {
        player.x = 0;
    }
    if (player.y < 0) {
        player.y = 0;
    }
    if (player.x > levelWidth - player.width) {
        player.x = levelWidth - player.width;
    }

    // Final grounded check
    if (!player.climbing) {
        player.grounded = isStandingOnSolid();
        if (player.grounded && player.yVel > 0) {
            player.yVel = 0;
        }
    }

    // Decide which animation row to use
    if (dx < 0) {
        player.direction = DIR_LEFT;
    } else if (dx > 0) {
        player.direction = DIR_RIGHT;
    }

    if (player.climbing) {
        if (wantsClimbUp && player.y < player.oldY) {
            player.direction = DIR_UP;
        } else if (wantsClimbDown && player.y > player.oldY) {
            player.direction = DIR_DOWN;
        }
    } else if (!player.grounded) {
        player.direction = DIR_DOWN;
    }

    // Decide whether to animate this frame
    if ((dx != 0 && player.x != player.oldX) ||
        (player.climbing && wantsClimbUp && player.y < player.oldY) ||
        (player.climbing && wantsClimbDown && player.y > player.oldY) ||
        (!player.climbing && player.y < player.oldY)) {
        wasActivelyMoving = 1;
    }

    player.isMoving = wasActivelyMoving;
}

static void updateCamera(void) {
    // Center the camera on the player, then clamp it to the map bounds.
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
}

// Return the horizontal size of the active cloud background in pixels.
// Home uses its own 32x64 cloud map, while both sky levels use the shared 64x32 cloud map.
static int getCloudMapWidth(void) {
    if (currentLevel == LEVEL_HOME) {
        return HOME_PIXEL_W;
    }

    // Level 1 and Level 2 both use the shared level background map.
    return LEVEL1_PIXEL_W;
}

// Apply the scrolling offsets for both visible gameplay layers.
//
// BG1 = cloud/background layer
//   - follows the camera more slowly for parallax
//   - also auto-scrolls on a timer so the clouds keep drifting
//
// BG2 = foreground / main gameplay visuals
static void applyBackgroundOffsets(void) {
    // This is where the camera and parallax meet. BG2 follows the player as
    // the main gameplay layer, while BG1 behaves like a drifting cloud layer.
    // The cloud layer scrolls on a timer, so the world feels more alive even
    // if the player stands still.
    int cloudMapWidth = getCloudMapWidth();

    // Animate the cloud layer using ONLY a horizontal timer-based scroll.
    // This keeps the clouds at a fixed vertical position on screen.
    cloudScrollX = (frameCounter / CLOUD_SCROLL_DELAY) % cloudMapWidth;

    // BG1 = clouds
    // Horizontal drift only, no vertical camera follow.
    setBackgroundOffset(1, cloudScrollX, vOff);

    // BG2 = main gameplay layer
    // Foreground follows the camera normally.
    setBackgroundOffset(2, hOff, vOff);
}

// ======================================================
//        HOME BEANSTALK / FARMLAND DYNAMIC VISUALS
// ======================================================
// The home map is not completely static. Once resources are deposited, edit the live 
// foreground tilemap at runtime so the farmland and beanstalk visibly change.
static unsigned short getHomeForegroundSourceEntry(int tileX, int tileY) {
    if (tileX < 0 || tileX >= HOME_MAP_W || tileY < 0 || tileY >= HOME_MAP_H) {
        return 0;
    }

    // Home is a 32x64 tall map:
    // rows 0-31 live in screenblock A and rows 32-63 live in screenblock B.
    if (tileY < 32) {
        return homebase_foregroundMap[tileY * 32 + tileX];
    }

    return homebase_foregroundMap[1024 + (tileY - 32) * 32 + tileX];
}

// Write one tile entry directly into the live home foreground screenblocks.
static void setHomeForegroundTileEntry(int tileX, int tileY, unsigned short entry) {
    volatile unsigned short* targetMap;
    int localY;

    if (tileX < 0 || tileX >= HOME_MAP_W || tileY < 0 || tileY >= HOME_MAP_H) {
        return;
    }

    if (tileY < 32) {
        targetMap = SCREENBLOCK[BG_FRONT_SB_A].tilemap;
        localY = tileY;
    } else {
        targetMap = SCREENBLOCK[BG_FRONT_SB_B].tilemap;
        localY = tileY - 32;
    }

    targetMap[localY * 32 + tileX] = entry;
}

// Draw a rectangular block of tile IDs from a fixed source area in the tileset.
static void drawHomeTileBlockFromTileset(int mapTileX, int mapTileY, int srcTileX, int srcTileY, int widthTiles, int heightTiles) {
    int row;
    int col;

    for (row = 0; row < heightTiles; row++) {
        for (col = 0; col < widthTiles; col++) {
            unsigned short tileId = (unsigned short)(((srcTileY + row) * 32) + (srcTileX + col));
            setHomeForegroundTileEntry(mapTileX + col, mapTileY + row, tileId);
        }
    }
}

// Rebuild the visible home beanstalk and farmland art from the current growth stage.
static void refreshHomeBeanstalkVisuals(void) {
    // Rebuild the visible home beanstalk and farmland art from the current growth stage.
    //
    // This function is the core of the growth system. It does two different jobs:
    // 1) it changes what the player sees by modifying the live tilemap
    // 2) together with canUseCurrentClimbPixel(), it keeps the visual state and
    //    usable climb region in sync
    //
    // Growth breakdown:
    // - Stage 0: regular farmland only
    // - Stage 1: planted farmland after bean sprout is deposited
    // - Stage 2: partial growth after bonemeal, including only the lower stalk
    //   and a decorative half-grown top so it looks intentional
    // - Stage 3: full beanstalk after water, with the original tall stalk restored
    int stage;
    int row;
    int col;

    // These edits only apply to the home foreground map.
    if (currentLevel != LEVEL_HOME) {
        return;
    }

    // Use the real deposited beanstalk growth stage
    stage = beanstalkGrowthStage;

    // --------------------------------------------------
    // Hide or reveal the tall beanstalk region.
    //
    // Stage 0 = no tall stalk
    // Stage 1 = no tall stalk yet, only seed farmland
    // Stage 2 = partial stalk visible
    // Stage 3 = full stalk visible
    // --------------------------------------------------
    for (row = HOME_BEANSTALK_TILE_TOP; row <= HOME_BEANSTALK_TILE_BOTTOM; row++) {
        for (col = HOME_BEANSTALK_TILE_LEFT; col <= HOME_BEANSTALK_TILE_RIGHT; col++) {
            if (stage >= 3) {
                // Full growth: restore the original home foreground beanstalk.
                setHomeForegroundTileEntry(col, row, getHomeForegroundSourceEntry(col, row));
            } else if (stage == 2 && row >= PARTIAL_BEANSTALK_VISIBLE_TOP_TILE) {
                // Partial growth: only reveal the lower visible section.
                setHomeForegroundTileEntry(col, row, getHomeForegroundSourceEntry(col, row));
            } else {
                // Hidden rows become blank / transparent.
                setHomeForegroundTileEntry(col, row, TRANSPARENT_TILE_ID);
            }
        }
    }

    // --------------------------------------------------
    // Farmland states
    //
    // Stage 0:
    //   regular farmland (0, 20), 8x5
    //
    // Stage 1:
    //   seed farmland (8, 20), 8x5
    //
    // Stage 2/3:
    //   start from seed farmland (8, 20), 8x5
    //   then overlay only the TOP 2 rows with the beanstalk base tiles
    // --------------------------------------------------
    if (stage == 0) {
        // Regular farmland before anything is planted.
        drawHomeTileBlockFromTileset(
            FARMLAND_TILE_X,
            FARMLAND_TILE_Y,
            EMPTY_FARMLAND_SRC_TILE_X,
            FARMLAND_SRC_TILE_Y,
            EMPTY_FARMLAND_SRC_W,
            EMPTY_FARMLAND_SRC_H
        );
    } else {
        // Stage 1/2/3 always begin with the full 8x5 seed farmland.
        drawHomeTileBlockFromTileset(
            FARMLAND_TILE_X,
            FARMLAND_TILE_Y,
            PLANTED_FARMLAND_SRC_TILE_X,
            FARMLAND_SRC_TILE_Y,
            EMPTY_FARMLAND_SRC_W,
            EMPTY_FARMLAND_SRC_H
        );

        // Once bonemeal is deposited, add the beanstalk/farmland transition.
        // These source tiles are only 8x2, so only stamp the TOP 2 rows
        // of the 8x5 farmland patch.
        if (stage >= 2) {
            drawHomeTileBlockFromTileset(
                FARMLAND_TILE_X,
                FARMLAND_TILE_Y,
                GROWN_FARMLAND_SRC_X,
                GROWN_FARMLAND_SRC_Y,
                GROWN_FARMLAND_SRC_W,
                GROWN_FARMLAND_SRC_H
            );
        }
    }

    // --------------------------------------------------
    // Stage 2 only:
    // The 2/3 grown state is intentionally a hybrid. The lower stalk is real
    // climbable beanstalk art, but the upper tip is drawn separately as a
    // decorative cap so the plant does not look like it was cut off flat.
    // --------------------------------------------------
    if (stage == 2) {
        drawHomeTileBlockFromTileset(
            PARTIAL_TOP_DEST_X,
            PARTIAL_TOP_DEST_Y,
            PARTIAL_TOP_SRC_X,
            PARTIAL_TOP_SRC_Y,
            PARTIAL_TOP_WIDTH,
            PARTIAL_TOP_HEIGHT
        );
    }
}

// True when the player is overlapping the 8x5 farmland patch in home.
static int isPlayerAtFarmland(void) {
    return currentLevel == LEVEL_HOME
        && rectsOverlap(
            player.x, player.y, player.width, player.height,
            FARMLAND_TILE_X * 8, FARMLAND_TILE_Y * 8,
            FARMLAND_WIDTH_TILES * 8, FARMLAND_HEIGHT_TILES * 8
        );
}

// Handle B-button deposits at the farmland.
// Deposit order is fixed:
//   bean sprout -> bonemeal -> water
static void tryDepositResource(void) {
    // Handle B-button deposits at the farmland.
    //
    // Inventory and growth are separate on purpose. Picking something up only
    // sets an inventory bit. The world does not change until the player comes
    // back home, stands on the farmland, and presses B. That makes the loop
    // clear: explore -> collect -> return home -> deposit -> grow.
    if (currentLevel != LEVEL_HOME) {
        return;
    }

    if (!BUTTON_PRESSED(BUTTON_B) || BUTTON_HELD(BUTTON_SELECT)) {
        return;
    }

    if (!isPlayerAtFarmland()) {
        return;
    }

    // Nothing happens if the inventory is empty or the player has the wrong item.
    if (beanstalkGrowthStage == 0) {
        if (inventoryFlags & INVENTORY_BEAN_SPROUT) {
            inventoryFlags &= ~INVENTORY_BEAN_SPROUT;
            beanstalkGrowthStage = 1;
            refreshHomeBeanstalkVisuals();
        }
    } else if (beanstalkGrowthStage == 1) {
        if (inventoryFlags & INVENTORY_BONEMEAL) {
            inventoryFlags &= ~INVENTORY_BONEMEAL;
            beanstalkGrowthStage = 2;
            refreshHomeBeanstalkVisuals();
        }
    } else if (beanstalkGrowthStage == 2) {
        if (inventoryFlags & INVENTORY_WATER) {
            inventoryFlags &= ~INVENTORY_WATER;
            beanstalkGrowthStage = 3;
            refreshHomeBeanstalkVisuals();
        }
    }
}

// Dynamic climb gate for the home beanstalk.
// The collision map still contains the full stalk, but this helper limits
// how much of it actually behaves like a climbable tile until the correct
// resource has been deposited.
static int canUseCurrentClimbPixel(int x, int y) {
    // Dynamic climb gate for the home beanstalk.
    //
    // The home collision map already contains the full beanstalk path, but the
    // player is not allowed to use all of it immediately. This helper acts as
    // a runtime gate: the collision art is there, but only the correct portion
    // is treated as climbable depending on the growth stage.
    int stage;

    if (!isClimbPixel(currentLevel, x, y)) {
        return 0;
    }

    if (currentLevel != LEVEL_HOME) {
        return 1;
    }

    // Use the real deposited beanstalk growth stage.
    stage = beanstalkGrowthStage;

    // Before bonemeal, none of the tall stalk should be climbable yet.
    if (stage <= 1) {
        return 0;
    }

    // Bonemeal reveals only the lower portion of the stalk.
    if (stage == 2) {
        return y >= (PARTIAL_BEANSTALK_VISIBLE_TOP_TILE * 8);
    }

    // Water completes the whole stalk.
    return 1;
}

// ======================================================
//                COLLISION / WORLD QUERY HELPERS
// ======================================================
static int bodyHitsSolidAt(int x, int y) {
    // Sample multiple points around the player's body instead of only the 4 corners.
    //
    // This is one of the most important collision fixes in the file. Because
    // the player sprite is tall, corner-only collision can miss short ledges or
    // let the body clip into nearby tiles. Sampling left/right edges plus top
    // and bottom points makes collision much more stable.
    int leftX   = x;
    int centerX = x + (player.width / 2);
    int rightX  = x + player.width - 1;

    int topY        = y;
    int upperMidY   = y + (player.height / 3);
    int lowerMidY   = y + ((player.height * 2) / 3);
    int bottomY     = y + player.height - 1;

    // Check left edge
    if (isBlockedPixel(currentLevel, leftX, topY) ||
        isBlockedPixel(currentLevel, leftX, upperMidY) ||
        isBlockedPixel(currentLevel, leftX, lowerMidY) ||
        isBlockedPixel(currentLevel, leftX, bottomY)) {
        return 1;
    }

    // Check right edge
    if (isBlockedPixel(currentLevel, rightX, topY) ||
        isBlockedPixel(currentLevel, rightX, upperMidY) ||
        isBlockedPixel(currentLevel, rightX, lowerMidY) ||
        isBlockedPixel(currentLevel, rightX, bottomY)) {
        return 1;
    }

    // Check top edge
    if (isBlockedPixel(currentLevel, leftX, topY) ||
        isBlockedPixel(currentLevel, centerX, topY) ||
        isBlockedPixel(currentLevel, rightX, topY)) {
        return 1;
    }

    // Check bottom edge
    if (isBlockedPixel(currentLevel, leftX, bottomY) ||
        isBlockedPixel(currentLevel, centerX, bottomY) ||
        isBlockedPixel(currentLevel, rightX, bottomY)) {
        return 1;
    }

    return 0;
}

static int canMoveTo(int newX, int newY) {
    // Test the player hitbox against blocked collision pixels using a
    // multi-point body check instead of only the 4 corners.
    //
    // This is the main 'is this next position legal?' helper used by movement.
    // Almost all movement eventually funnels through here.
    // This fixes bugs where a tall player can clip into short platforms or
    // miss narrow ledges while falling.

    int rightX  = newX + player.width - 1;
    int bottomY = newY + player.height - 1;

    if (newX < 0 || rightX >= levelWidth || newY < 0) {
        return 0;
    }

    // Fully below the level: allow movement so the player can properly fall out.
    if (newY >= levelHeight) {
        return 1;
    }

    // If the player is partially below the map, only test the part of the body
    // that is still inside the level.
    if (bottomY >= levelHeight) {
        int leftX   = newX;
        int centerX = newX + (player.width / 2);

        return !isBlockedPixel(currentLevel, leftX, newY)
            && !isBlockedPixel(currentLevel, centerX, newY)
            && !isBlockedPixel(currentLevel, rightX, newY);
    }

    return !bodyHitsSolidAt(newX, newY);
}

static int findStandingSpawnY(int spawnX, int preferredTopY) {
    // Find the nearest open Y position where the player can stand on solid ground.
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
        int centerFootX = clampedX + (player.width / 2);

        if (canMoveTo(clampedX, y) &&
            (y + player.height >= levelHeight ||
             isBlockedPixel(currentLevel, leftFootX, y + player.height) ||
             isBlockedPixel(currentLevel, centerFootX, y + player.height) ||
             isBlockedPixel(currentLevel, rightFootX, y + player.height))) {
            return y;
        }
    }

    // If that failed, search upward.
    for (y = clampedPreferredY - 1; y >= 0; y--) {
        int centerFootX = clampedX + (player.width / 2);

        if (canMoveTo(clampedX, y) &&
            (y + player.height >= levelHeight ||
             isBlockedPixel(currentLevel, leftFootX, y + player.height) ||
             isBlockedPixel(currentLevel, centerFootX, y + player.height) ||
             isBlockedPixel(currentLevel, rightFootX, y + player.height))) {
            return y;
        }
    }

    // Fallback: just clamp it.
    return clampedPreferredY;
}

static int onClimbTile(void) {
    // Check climb overlap using the player's current position.
    return onClimbTileAt(player.x, player.y);
}

static int onClimbTileAt(int x, int y) {
    // Check several points down the player body for ladder or vine collision.
    int centerX = x + (player.width / 2);

    // Sample multiple points vertically through the player's body.
    // This is much more reliable for ladders / beanstalks than checking
    // only one center point.
    int topY    = y + 2;
    int upperY  = y + 8;
    int midY    = y + (player.height / 2);
    int lowerY  = y + player.height - 8;
    int footY   = y + player.height - 2;

    return canUseCurrentClimbPixel(centerX, topY)
        || canUseCurrentClimbPixel(centerX, upperY)
        || canUseCurrentClimbPixel(centerX, midY)
        || canUseCurrentClimbPixel(centerX, lowerY)
        || canUseCurrentClimbPixel(centerX, footY);
}

// True when the player's lower body is still on the ladder / vine,
// but the upper body has already cleared it
static int isAtTopOfClimb(int x, int y) {
    // This very specific check is what detects the classic ladder-top trap.
    // Once the head and chest are clear but the hips/feet are still on the
    // climb tile, the code knows the player is trying to step onto the top.
    // Detect the specific case where the player has reached the top of a climbable area.
    int centerX = x + (player.width / 2);

    int headY  = y + 2;
    int chestY = y + 8;
    int hipY   = y + player.height - 10;
    int footY  = y + player.height - 2;

    int upperOnClimb = canUseCurrentClimbPixel(centerX, headY)
                    || canUseCurrentClimbPixel(centerX, chestY);

    int lowerOnClimb = canUseCurrentClimbPixel(centerX, hipY)
                    || canUseCurrentClimbPixel(centerX, footY);

    return lowerOnClimb && !upperOnClimb;
}

// Detect when the player is at the top of the ladder and launch them upward
// so they can land on the platform above instead of getting stuck.
//
// Strat:
//   1. Scan upward pixel-by-pixel until we find a Y where the player body
//      is FULLY clear of all climb tiles (not just open space).
//   2. Teleport the player to that Y so the next frame cannot re-engage
//      climb mode automatically.
//   3. Apply a small upward velocity so they arc cleanly onto the ledge.
//
// If no fully-clear position exists within the scan range (e.g. solid
// ceiling directly above), still leave climb mode and apply the boost
// so that the player is not permanently trapped
static int tryStartLadderTopExit(void) {
    // Pop the player upward off the ladder so they can step onto the platform above.
    int pop;

    // Scan upward. Try to find a spot where:
    //   (a) the position is walkable (no solid collision), AND
    //   (b) the player body no longer overlaps any climb tile.
    for (pop = 1; pop <= 10; pop++) {
        int testY = player.y - pop;

        if (testY < 0) {
            break;
        }

        if (canMoveTo(player.x, testY) && !onClimbTileAt(player.x, testY)) {
            // Found a clean exit position above the ladder top.
            player.y     = testY;
            player.climbing = 0;
            player.grounded = 0;
            player.yVel  = LADDER_EXIT_BOOST; // small upward arc (-4)
            return 1;
        }
    }

    // Fallback just in case: no clean gap found (tight ceiling, etc.).
    // Still force-exit climb mode so the player is never permanently stuck.
    player.climbing = 0;
    player.grounded = 0;
    player.yVel     = LADDER_EXIT_BOOST;
    return 1;
}

static int isStandingOnSolid(void) {
    // Sample left, center, and right under the player's feet.
    // The center check is important for narrow platforms.
    int leftFootX   = player.x + 2;
    int centerFootX = player.x + (player.width / 2);
    int rightFootX  = player.x + player.width - 3;
    int belowY      = player.y + player.height;

    if (belowY >= levelHeight) {
        return 1;
    }

    return isBlockedPixel(currentLevel, leftFootX, belowY)
        || isBlockedPixel(currentLevel, centerFootX, belowY)
        || isBlockedPixel(currentLevel, rightFootX, belowY);
}

static int touchesHazard(void) {
    // Sample the lower body against hazard tiles.
    int leftFootX  = player.x + 2;
    int rightFootX = player.x + player.width - 3;
    int footY      = player.y + player.height - 1;
    int midX       = player.x + (player.width / 2);
    int midY       = player.y + player.height - 4;

    return isHazardPixel(currentLevel, leftFootX, footY)
        || isHazardPixel(currentLevel, rightFootX, footY)
        || isHazardPixel(currentLevel, midX, midY);
}

static int fellOutOfLevel(void) {
    // Give the player a little extra room below the map before treating
    // it as a true fall-out. This feels better than triggering instantly.
    return player.y > levelHeight + 16;
}

static void recoverFromInvincibleFall(void) {
    // With invincibility enabled, falling out of the level should not kill
    // the player.
    player.x = CLAMP(respawnX, 0, levelWidth - player.width);
    player.y = CLAMP(respawnY, 0, levelHeight - player.height);

    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;
    player.isMoving = 0;
    player.animFrame = 0;
    player.animCounter = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    // Recenter the camera immediately after the rescue
    updateCamera();
}

static int touchesWinTile(void) {
    int leftFootX  = player.x + 2;
    int centerX    = player.x + (player.width / 2);
    int rightFootX = player.x + player.width - 3;
    int footY      = player.y + player.height - 1;

    return collisionAtPixel(currentLevel, leftFootX, footY) == COL_WIN
        || collisionAtPixel(currentLevel, centerX, footY) == COL_WIN
        || collisionAtPixel(currentLevel, rightFootX, footY) == COL_WIN;
}

static void handleLevelTransitions(void) {
    // Check collision-map transition markers under the player and swap maps when needed.
    //
    // Transitions are data-driven through collision-map palette indices.
    // It reads the exact collision value under the player's feet and chooses the destination spawn
    // based on that marker. There are 6 transitions
    int leftX   = player.x + 2;
    int centerX = player.x + (player.width / 2);
    int rightX  = player.x + player.width - 3;
    int probeY  = player.y + player.height - 1;

    // Read the exact collision-map palette index under each probe.
    u8 leftTile   = collisionAtPixel(currentLevel, leftX, probeY);
    u8 centerTile = collisionAtPixel(currentLevel, centerX, probeY);
    u8 rightTile  = collisionAtPixel(currentLevel, rightX, probeY);

    // --------------------------------------------------
    // HOME -> LEVEL ONE (bottom/original portal)
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME &&
        (leftTile   == COL_HOME_TO_LEVEL1_BOTTOM ||
         centerTile == COL_HOME_TO_LEVEL1_BOTTOM ||
         rightTile  == COL_HOME_TO_LEVEL1_BOTTOM)) {

        // Use custom spawn selection for the bottom Level 1 entry.
        respawnX = LEVEL1_FROM_HOME_BOTTOM_SPAWN_X;
        respawnY = -1;
        respawnPreferredY = LEVEL1_FROM_HOME_BOTTOM_PREF_Y;
        goToLevelOne(1);
        return;
    }

    // --------------------------------------------------
    // HOME -> LEVEL ONE (top/platform portal)
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME &&
        (leftTile   == COL_HOME_TO_LEVEL1_TOP ||
         centerTile == COL_HOME_TO_LEVEL1_TOP ||
         rightTile  == COL_HOME_TO_LEVEL1_TOP)) {

        // Use custom spawn selection for the upper Level 1 entry.
        respawnX = LEVEL1_FROM_HOME_TOP_SPAWN_X;
        respawnY = -1;
        respawnPreferredY = LEVEL1_FROM_HOME_TOP_PREF_Y;
        goToLevelOne(1);
        return;
    }

    // --------------------------------------------------
    // LEVEL ONE -> HOME (bottom/original return)
    // --------------------------------------------------
    if (currentLevel == LEVEL_ONE &&
        (leftTile   == COL_LEVEL1_TO_HOME_BOTTOM ||
         centerTile == COL_LEVEL1_TO_HOME_BOTTOM ||
         rightTile  == COL_LEVEL1_TO_HOME_BOTTOM)) {

        // Return to the lower/original home entry area.
        respawnX = HOME_FROM_LEVEL1_BOTTOM_SPAWN_X;
        respawnY = -1;
        respawnPreferredY = HOME_FROM_LEVEL1_BOTTOM_PREF_Y;
        goToHome(1);
        return;
    }

    // --------------------------------------------------
    // LEVEL ONE -> HOME (top/platform return)
    // --------------------------------------------------
    if (currentLevel == LEVEL_ONE &&
        (leftTile   == COL_LEVEL1_TO_HOME_TOP ||
         centerTile == COL_LEVEL1_TO_HOME_TOP ||
         rightTile  == COL_LEVEL1_TO_HOME_TOP)) {

        // Return to the upper platform area in home.
        respawnX = HOME_FROM_LEVEL1_TOP_SPAWN_X;
        respawnY = -1;
        respawnPreferredY = HOME_FROM_LEVEL1_TOP_PREF_Y;
        goToHome(1);
        return;
    }

    // --------------------------------------------------
    // HOME -> LEVEL TWO
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME &&
        (leftTile   == COL_HOME_TO_LEVEL2 ||
         centerTile == COL_HOME_TO_LEVEL2 ||
         rightTile  == COL_HOME_TO_LEVEL2)) {

        goToLevelTwo(0);
        return;
    }

    // --------------------------------------------------
    // LEVEL TWO -> HOME
    // --------------------------------------------------
    if (currentLevel == LEVEL_TWO &&
        (leftTile   == COL_LEVEL2_TO_HOME ||
         centerTile == COL_LEVEL2_TO_HOME ||
         rightTile  == COL_LEVEL2_TO_HOME)) {

        // This return already uses an exact, hand-tuned Y.
        respawnX = HOME_FROM_LEVEL2_SPAWN_X;
        respawnY = HOME_FROM_LEVEL2_SPAWN_Y - player.height;
        goToHome(1);
        return;
    }
}

static int beeBodyHitsSolidAt(int x, int y) {
    // Sample several points on the bee body so it obeys the collision map
    // instead of clipping into platforms or walls.
    int leftX   = x;
    int centerX = x + (BEE_WIDTH / 2);
    int rightX  = x + BEE_WIDTH - 1;

    int topY    = y;
    int middleY = y + (BEE_HEIGHT / 2);
    int bottomY = y + BEE_HEIGHT - 1;

    return isBlockedPixel(currentLevel, leftX, topY)
        || isBlockedPixel(currentLevel, leftX, middleY)
        || isBlockedPixel(currentLevel, leftX, bottomY)
        || isBlockedPixel(currentLevel, rightX, topY)
        || isBlockedPixel(currentLevel, rightX, middleY)
        || isBlockedPixel(currentLevel, rightX, bottomY)
        || isBlockedPixel(currentLevel, centerX, topY)
        || isBlockedPixel(currentLevel, centerX, bottomY);
}

static int canBeeMoveTo(int newX, int newY) {
    int rightX = newX + BEE_WIDTH - 1;
    int bottomY = newY + BEE_HEIGHT - 1;

    if (newX < 0 || rightX >= levelWidth || newY < 0 || bottomY >= levelHeight) {
        return 0;
    }

    return !beeBodyHitsSolidAt(newX, newY);
}

static void initBee(Bee* bee, int x, int y, int patrolRange, int startDir) {
    bee->x = x;
    bee->y = y;
    bee->active = canBeeMoveTo(x, y);
    bee->animFrame = 0;
    bee->animCounter = 0;
    bee->dir = (startDir >= 0) ? 1 : -1;

    // Bee patrols in a small fixed horizontal band.
    bee->minX = x - patrolRange;
    bee->maxX = x + patrolRange;

    if (bee->minX < 0) {
        bee->minX = 0;
    }
    if (bee->maxX > levelWidth - BEE_WIDTH) {
        bee->maxX = levelWidth - BEE_WIDTH;
    }
}

static void initLevelBees(void) {
    beeCount = 0;

    if (currentLevel == LEVEL_ONE) {
        initBee(&bees[0], LEVEL1_BEE_SPAWN_X, LEVEL1_BEE_SPAWN_Y, BEE_PATROL_RANGE, -1);
        beeCount = 1;
    } else if (currentLevel == LEVEL_TWO) {
        initBee(&bees[0], LEVEL2_BEE_A_SPAWN_X, LEVEL2_BEE_A_SPAWN_Y, BEE_PATROL_RANGE, -1);
        initBee(&bees[1], LEVEL2_BEE_B_SPAWN_X, LEVEL2_BEE_B_SPAWN_Y, BEE_PATROL_RANGE, 1);
        beeCount = 2;
    }
}

static void updateBees(void) {
    for (int i = 0; i < beeCount; i++) {
        Bee* bee = &bees[i];
        int nextX;

        if (!bee->active) {
            continue;
        }

        // Animate wings
        bee->animCounter++;
        if (bee->animCounter >= 8) {
            bee->animCounter = 0;
            bee->animFrame = (bee->animFrame + 1) % BEE_ANIM_FRAMES;
        }

        // Horizontal patrol
        nextX = bee->x + bee->dir * BEE_PATROL_SPEED;

        // Reverse if the bee would leave its patrol zone or hit level collision.
        if (nextX < bee->minX || nextX > bee->maxX || !canBeeMoveTo(nextX, bee->y)) {
            bee->dir *= -1;
            nextX = bee->x + bee->dir * BEE_PATROL_SPEED;

            // If both directions are blocked, stay put
            if (nextX < bee->minX || nextX > bee->maxX || !canBeeMoveTo(nextX, bee->y)) {
                continue;
            }
        }

        bee->x = nextX;
    }
}

static int playerTouchesBee(void) {
    // collision checks for the bee
    for (int i = 0; i < beeCount; i++) {
        if (bees[i].active && rectsOverlap(
                player.x, player.y, player.width, player.height,
                bees[i].x, bees[i].y, BEE_WIDTH, BEE_HEIGHT)) {
            return 1;
        }
    }

    return 0;
}

// ======================================================
//                COLLECTIBLES / INVENTORY
// ======================================================
static void initBeanSprout(void) {
    // Place the bean sprout collectible in home and hide it if it was already
    // collected or already deposited into the farmland.
    int playerGroundY;

    // Put the bean sprout about 5 tiles in front of the player spawn.
    beanSprout.x = HOME_SPAWN_X + (5 * 8);

    // findStandingSpawnY() returns a TOP-Y for a player-sized body.
    // Convert that into a ground Y, then place the bean sprout so its feet
    // sit on that same ground.
    playerGroundY = findStandingSpawnY(beanSprout.x, levelHeight - (8 * 8)) + PLAYER_HEIGHT;
    beanSprout.y = playerGroundY - BEAN_SPROUT_HEIGHT;

    // The bean sprout should only exist before the first deposit stage.
    // Once the player has picked it up OR deposited it, do not let it respawn.
    beanSprout.active =
        ((inventoryFlags & INVENTORY_BEAN_SPROUT) == 0) &&
        (beanstalkGrowthStage == 0);

    beanSprout.bob = 0;
}

static void initBonemeal(void) {
    // Place bonemeal near the left side of Level 1, sitting on the ground.
    int playerGroundY;

    bonemeal.x = LEVEL1_BONEMEAL_SPAWN_X;

    // Use the same ground-placement idea as the bean sprout:
    // find the top Y where a player could stand, then convert that to ground Y.
    playerGroundY = findStandingSpawnY(bonemeal.x, levelHeight - (8 * 8)) + PLAYER_HEIGHT;
    bonemeal.y = playerGroundY - RESOURCE_HEIGHT;

    // Hide if already collected.
    bonemeal.active = ((inventoryFlags & INVENTORY_BONEMEAL) == 0);

    // Start animation on frame 0.
    bonemeal.animFrame = 0;
    bonemeal.animCounter = 0;
    bonemeal.bob = 0;
}

static void initWaterDroplet(void) {
    // Place the water droplet near the right side of Level 2.
    waterDroplet.x = LEVEL2_WATER_SPAWN_X;
    waterDroplet.y = LEVEL2_WATER_SPAWN_Y;

    // Hide if already collected.
    waterDroplet.active = ((inventoryFlags & INVENTORY_WATER) == 0);

    // Start animation on frame 0.
    waterDroplet.animFrame = 0;
    waterDroplet.animCounter = 0;
    waterDroplet.bob = 0;
}

// Basic AABB collision helper for item pickup.
static int rectsOverlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    // Simple axis-aligned bounding box overlap test for pickups.
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

static void updateCollectibles(void) {
    // Handle item pickup logic for all active collectibles in the world.
    //
    // Pickups use simple AABB overlap checks because they are forgiving 
    // Once picked up, the corresponding inventory bit is enabled and the world object is hidden.

    // --------------------------------------------------
    // Bean sprout pickup in home
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME && beanSprout.active) {
        if (rectsOverlap(
                player.x, player.y, player.width, player.height,
                beanSprout.x, beanSprout.y, BEAN_SPROUT_WIDTH, BEAN_SPROUT_HEIGHT)) {

            // Mark bean sprout as collected and hide it.
            inventoryFlags |= INVENTORY_BEAN_SPROUT;
            beanSprout.active = 0;
        }
    }

    // --------------------------------------------------
    // Bonemeal pickup in Level 1
    // --------------------------------------------------
    if (currentLevel == LEVEL_ONE && bonemeal.active) {
        if (rectsOverlap(
                player.x, player.y, player.width, player.height,
                bonemeal.x, bonemeal.y, RESOURCE_WIDTH, RESOURCE_HEIGHT)) {

            // Mark bonemeal as collected and hide it.
            inventoryFlags |= INVENTORY_BONEMEAL;
            bonemeal.active = 0;
        }
    }

    // --------------------------------------------------
    // Water droplet pickup in Level 2
    // --------------------------------------------------
    if (currentLevel == LEVEL_TWO && waterDroplet.active) {
        if (rectsOverlap(
                player.x, player.y, player.width, player.height,
                waterDroplet.x, waterDroplet.y, RESOURCE_WIDTH, RESOURCE_HEIGHT)) {

            // Mark water as collected and hide it.
            inventoryFlags |= INVENTORY_WATER;
            waterDroplet.active = 0;
        }
    }
}

static void updateCollectibleAnimations(void) {
    // Slowly toggle each active collectible between its 2 animation frames.
    // Higher threshold = slower animation.
    if (currentLevel == LEVEL_ONE && bonemeal.active) {
        bonemeal.animCounter++;
        if (bonemeal.animCounter >= 20) {
            bonemeal.animCounter = 0;
            bonemeal.animFrame = (bonemeal.animFrame + 1) % RESOURCE_ANIM_FRAMES;
        }
    }

    if (currentLevel == LEVEL_TWO && waterDroplet.active) {
        waterDroplet.animCounter++;
        if (waterDroplet.animCounter >= 20) {
            waterDroplet.animCounter = 0;
            waterDroplet.animFrame = (waterDroplet.animFrame + 1) % RESOURCE_ANIM_FRAMES;
        }
    }
}

// --------------------------------------------------
// Build HUD inventory text.
// --------------------------------------------------
// --------------------------------------------------
// Cheat: give the next required progression resource.
// bonemeal comes first, then water.
//
// If bonemeal is already owned or has already been deposited,
// the cheat skips straight to the water droplet.
// --------------------------------------------------
static void giveNextResourceCheat(void) {
    // Bonemeal is the next needed item until it has either:
    // - been collected into inventory, or
    // - already been deposited into the beanstalk progression
    if ((beanstalkGrowthStage < 2) && ((inventoryFlags & INVENTORY_BONEMEAL) == 0)) {
        inventoryFlags |= INVENTORY_BONEMEAL;

        // If the bonemeal pickup is currently visible in Level 1,
        // remove it from the world so the cheat doesn't duplicate it.
        bonemeal.active = 0;
        return;
    }

    // Otherwise, water is the next needed item.
    // This can be added even if bonemeal is still sitting in inventory,
    // so both items can be held at the same time.
    if ((inventoryFlags & INVENTORY_WATER) == 0) {
        inventoryFlags |= INVENTORY_WATER;

        // Hide the world pickup if it is still active in Level 2.
        waterDroplet.active = 0;
    }
}

static const char* getInventoryText(void) {
    // Build the HUD string that lists the items currently in the inventory bitmask.
    //
    // Inventory is stored as a bitmask rather than separate booleans so the
    // player can hold multiple resources at once. Each item corresponds to one
    // bit, which makes checks, adds, removes, and HUD rendering simple.
    // Build the HUD string that lists the items currently in the inventory bitmask.
    static char buffer[48];

    buffer[0] = '\0';

    if (inventoryFlags == 0) {
        strcpy(buffer, "EMPTY");
        return buffer;
    }

    if (inventoryFlags & INVENTORY_BEAN_SPROUT) {
        strcat(buffer, "BEAN SPROUT");
    }

    if (inventoryFlags & INVENTORY_BONEMEAL) {
        if (buffer[0] != '\0') strcat(buffer, ", ");
        strcat(buffer, "BONEMEAL");
    }

    if (inventoryFlags & INVENTORY_WATER) {
        if (buffer[0] != '\0') strcat(buffer, ", ");
        strcat(buffer, "WATER");
    }

    return buffer;
}

// ======================================================
//                RENDERING / SPRITE DRAWING
// ======================================================
static void drawGameplay(void) {
    // Restore and animate the sky palette during gameplay.
    updateDaytimePalette();

    // Refresh HUD text, scroll the animated cloud layer plus the foreground,
    // and then draw all visible sprites.
    clearHud();
    drawHudText();

    applyBackgroundOffsets();

    drawSprites();
}

static void drawSprites(void) {
    // Write the current frame's sprite attributes into shadow OAM.
    //
    // The sprites are drawn in screen space, not world space, so every sprite
    // position is converted by subtracting the camera offsets first. The tile
    // index chooses the correct frame, while the palette row chooses the color
    // bank. That is why the player and world items can share OBJ memory cleanly
    // but still use different colors.
    int screenX = player.x - hOff;
    int screenY = player.y - vOff;

    // --------------------------------------------------
    // Draw player
    // --------------------------------------------------
    if (screenX < -player.width || screenX >= SCREENWIDTH ||
        screenY < -player.height || screenY >= SCREENHEIGHT) {
        hideSprite(OBJ_INDEX_PLAYER);
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
        shadowOAM[OBJ_INDEX_PLAYER].attr0 = ATTR0_Y(screenY) | ATTR0_TALL | ATTR0_4BPP;
        shadowOAM[OBJ_INDEX_PLAYER].attr1 = ATTR1_X(screenX) | ATTR1_MEDIUM;
        shadowOAM[OBJ_INDEX_PLAYER].attr2 = ATTR2_TILEID(tileBase)
            | ATTR2_PRIORITY(0)
            | ATTR2_PALROW(PLAYER_PALROW);
    }

    // --------------------------------------------------
    // Draw bean sprout
    //
    // Since 16x24 is not a native GBA sprite size, draw it in 2 pieces:
    //   top    = 16x16 using first 4 tiles
    //   bottom = 16x8  using last 2 tiles
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME && beanSprout.active) {
        int beanScreenX = beanSprout.x - hOff;
        int beanScreenY = beanSprout.y - vOff;

        if (beanScreenX < -BEAN_SPROUT_WIDTH || beanScreenX >= SCREENWIDTH ||
            beanScreenY < -BEAN_SPROUT_HEIGHT || beanScreenY >= SCREENHEIGHT) {
            hideSprite(OBJ_INDEX_BEAN_TOP);
            hideSprite(OBJ_INDEX_BEAN_BOTTOM);
        } else {
            // Top 16x16 portion
            shadowOAM[OBJ_INDEX_BEAN_TOP].attr0 =
                ATTR0_Y(beanScreenY) | ATTR0_SQUARE | ATTR0_4BPP;
            shadowOAM[OBJ_INDEX_BEAN_TOP].attr1 =
                ATTR1_X(beanScreenX) | ATTR1_SMALL;
            shadowOAM[OBJ_INDEX_BEAN_TOP].attr2 =
                ATTR2_TILEID(OBJ_TILE_BEAN_SPROUT)
                | ATTR2_PRIORITY(0)
                | ATTR2_PALROW(WORLD_SPRITE_PALROW);

            // Bottom 16x8 portion starts at tile offset +4
            shadowOAM[OBJ_INDEX_BEAN_BOTTOM].attr0 =
                ATTR0_Y(beanScreenY + 16) | ATTR0_WIDE | ATTR0_4BPP;
            shadowOAM[OBJ_INDEX_BEAN_BOTTOM].attr1 =
                ATTR1_X(beanScreenX) | ATTR1_TINY;
            shadowOAM[OBJ_INDEX_BEAN_BOTTOM].attr2 =
                ATTR2_TILEID(OBJ_TILE_BEAN_SPROUT + 4)
                | ATTR2_PRIORITY(0)
                | ATTR2_PALROW(WORLD_SPRITE_PALROW);
        }
    } else {
        hideSprite(OBJ_INDEX_BEAN_TOP);
        hideSprite(OBJ_INDEX_BEAN_BOTTOM);
    }

    // --------------------------------------------------
    // Draw bonemeal in Level 1
    // --------------------------------------------------
    if (currentLevel == LEVEL_ONE && bonemeal.active) {
        int itemScreenX = bonemeal.x - hOff;
        int itemScreenY = bonemeal.y - vOff;
        int tileBase = OBJ_TILE_BONEMEAL + bonemeal.animFrame * RESOURCE_TILES_PER_FRAME;

        if (itemScreenX < -RESOURCE_WIDTH || itemScreenX >= SCREENWIDTH ||
            itemScreenY < -RESOURCE_HEIGHT || itemScreenY >= SCREENHEIGHT) {
            hideSprite(OBJ_INDEX_BONEMEAL);
        } else {
            shadowOAM[OBJ_INDEX_BONEMEAL].attr0 =
                ATTR0_Y(itemScreenY) | ATTR0_SQUARE | ATTR0_4BPP;
            shadowOAM[OBJ_INDEX_BONEMEAL].attr1 =
                ATTR1_X(itemScreenX) | ATTR1_SMALL;
            shadowOAM[OBJ_INDEX_BONEMEAL].attr2 =
                ATTR2_TILEID(tileBase)
                | ATTR2_PRIORITY(0)
                | ATTR2_PALROW(WORLD_SPRITE_PALROW);
        }
    } else {
        hideSprite(OBJ_INDEX_BONEMEAL);
    }

    // --------------------------------------------------
    // Draw water droplet in Level 2
    // --------------------------------------------------
    if (currentLevel == LEVEL_TWO && waterDroplet.active) {
        int itemScreenX = waterDroplet.x - hOff;
        int itemScreenY = waterDroplet.y - vOff;
        int tileBase = OBJ_TILE_WATER + waterDroplet.animFrame * RESOURCE_TILES_PER_FRAME;

        if (itemScreenX < -RESOURCE_WIDTH || itemScreenX >= SCREENWIDTH ||
            itemScreenY < -RESOURCE_HEIGHT || itemScreenY >= SCREENHEIGHT) {
            hideSprite(OBJ_INDEX_WATER);
        } else {
            shadowOAM[OBJ_INDEX_WATER].attr0 =
                ATTR0_Y(itemScreenY) | ATTR0_SQUARE | ATTR0_4BPP;
            shadowOAM[OBJ_INDEX_WATER].attr1 =
                ATTR1_X(itemScreenX) | ATTR1_SMALL;
            shadowOAM[OBJ_INDEX_WATER].attr2 =
                ATTR2_TILEID(tileBase)
                | ATTR2_PRIORITY(0)
                | ATTR2_PALROW(WORLD_SPRITE_PALROW);
        }
    } else {
        hideSprite(OBJ_INDEX_WATER);
    }

    // --------------------------------------------------
    // Draw bees
    //
    // Each bee is 16x24, so like the bean sprout it is drawn as:
    //   top    16x16
    //   bottom 16x8
    //
    // The sprite sheet only includes one facing direction, so horizontal flip
    // is used when the bee is moving left.
    // --------------------------------------------------
    for (int i = 0; i < 3; i++) {
        int topIndex = OBJ_INDEX_BEE0_TOP + (i * 2);
        int bottomIndex = OBJ_INDEX_BEE0_BOTTOM + (i * 2);

        if (i >= beeCount || !bees[i].active) {
            hideSprite(topIndex);
            hideSprite(bottomIndex);
            continue;
        }

        {
            int beeScreenX = bees[i].x - hOff;
            int beeScreenY = bees[i].y - vOff;
            int tileBase = OBJ_TILE_BEE + bees[i].animFrame * BEE_TILES_PER_FRAME;
            
            // Bee art in the sprite sheet already faces left.
            // Only flip it when the bee is moving right.
            int flip = (bees[i].dir > 0) ? ATTR1_HFLIP : 0;

            if (beeScreenX < -BEE_WIDTH || beeScreenX >= SCREENWIDTH ||
                beeScreenY < -BEE_HEIGHT || beeScreenY >= SCREENHEIGHT) {
                hideSprite(topIndex);
                hideSprite(bottomIndex);
            } else {
                // Top 16x16 piece
                shadowOAM[topIndex].attr0 =
                    ATTR0_Y(beeScreenY) | ATTR0_SQUARE | ATTR0_4BPP;
                shadowOAM[topIndex].attr1 =
                    ATTR1_X(beeScreenX) | ATTR1_SMALL | flip;
                shadowOAM[topIndex].attr2 =
                    ATTR2_TILEID(tileBase)
                    | ATTR2_PRIORITY(0)
                    | ATTR2_PALROW(WORLD_SPRITE_PALROW);

                // Bottom 16x8 piece
                shadowOAM[bottomIndex].attr0 =
                    ATTR0_Y(beeScreenY + 16) | ATTR0_WIDE | ATTR0_4BPP;
                shadowOAM[bottomIndex].attr1 =
                    ATTR1_X(beeScreenX) | ATTR1_TINY | flip;
                shadowOAM[bottomIndex].attr2 =
                    ATTR2_TILEID(tileBase + 4)
                    | ATTR2_PRIORITY(0)
                    | ATTR2_PALROW(WORLD_SPRITE_PALROW);
            }
        }
    }

    // Hide everything else
    for (int i = 11; i < 128; i++) {
        hideSprite(i);
    }
}


static void hideSprite(int index) {
    // Hide one sprite entry in shadow OAM.
    shadowOAM[index].attr0 = ATTR0_HIDE;
    shadowOAM[index].attr1 = 0;
    shadowOAM[index].attr2 = 0;
}

// ======================================================
//                HUD / TEXT DRAWING
// ======================================================
static void drawHudText(void) {
    // Draw the current level label and inventory readout on the HUD layer.
    //
    // BG0 is reserved for text so UI remains stable even while BG1/BG2 scroll.
    // That means HUD text does not move with the camera and can be cleared and
    // redrawn each frame without disturbing the map art.
    if (currentLevel == LEVEL_HOME) {
        putText(1, 1, "HOMEBASE");
    } else if (currentLevel == LEVEL_ONE) {
        putText(1, 1, "LEVEL 1");
    } else {
        putText(1, 1, "LEVEL 2");
    }

    // Show cheat status only when cheat mode is enabled.
    if (cheatModeEnabled) {
        putText(16, 1, "CHEATS ENABLED");
    }

    // Inventory HUD
    putText(1, 3, "INVENTORY:");
    putText(1, 5, getInventoryText());
}

static void drawTitleScreen(void) {
    // The title artwork itself is now the BG1 tilemap.
    // This function only draws the overlay text on BG0.
    clearHud();
    putText(8, 6, "BEYOND THE SKY");
    putText(8, 8, "COZY FARM GAME");
    putText(9, 14, "PRESS START");
}

static void drawInstructionsPage(void) {
    // Draw the current instructions page.
    clearHud();

    if (instructionsPage == 0) {
        clearHud();
        putText(7, 4, "INSTRUCTIONS 1/2");
        putText(8, 8,"LEFT/RIGHT MOVE");
        putText(4, 10, "UP/DOWN JUMP OR CLIMB");
        putText(7, 16, "A NEXT  B BACK");
    } else {
        clearHud();
        putText(7, 4, "INSTRUCTIONS 2/2");
        putText(7, 8, "FINISH LEVELS TO");
        putText(4, 10, "GET RESOURCES, RETURN");
        putText(3, 12, "HOME & PRESS B ON SOIL");
        putText(6, 16, "B PREV  START PLAY");
    }
}

static void drawPauseScreen(void) {
    // Draw pause text and hide gameplay sprites while paused.
    clearHud();
    putText(9, 7, "PAUSED GAME");
    putText(7, 11, "START TO RESUME"); 
    putText(2, 14, "SELECT TO RETURN TO TITLE"); 
    hideSprites();
}

static void clearHud(void) {
    // Clear the HUD text map so the next screen can redraw cleanly.
    fontClearMap(HUD_SCREENBLOCK, 0);
}

static void putText(int col, int row, const char* str) {
    // Convenience wrapper for drawing a string into the HUD screenblock.
    fontDrawString(HUD_SCREENBLOCK, col, row, str);
}