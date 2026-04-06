#include <string.h>
#include "game.h"
#include "tileset.h"
#include "spriteSheet.h"
#include "homebase_foreground.h"

// ======================================================
//                FILE-SCOPE GAME STATE
// ======================================================

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

static int hOff;
static int vOff;
static int cloudHOff;
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
int instantGrowCheat;
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

static void initBeanSprout(void);
static void initBonemeal(void);
static void initWaterDroplet(void);
static void updateCollectibles(void);
static void updateCollectibleAnimations(void);
static int rectsOverlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh);
static const char* getInventoryText(void);
static int getEffectiveBeanstalkGrowthStage(void);
static unsigned short getHomeForegroundSourceEntry(int tileX, int tileY);
static void setHomeForegroundTileEntry(int tileX, int tileY, unsigned short entry);
static void drawHomeTileBlockFromTileset(int mapTileX, int mapTileY, int srcTileX, int srcTileY, int widthTiles, int heightTiles);
static void refreshHomeBeanstalkVisuals(void);
static int isPlayerAtFarmland(void);
static void tryDepositResource(void);
static int canUseCurrentClimbPixel(int x, int y);

// Collision / interaction helpers
static int canMoveTo(int newX, int newY);
static int onClimbTile(void);
static int onClimbTileAt(int x, int y);
static int isAtTopOfClimb(int x, int y);
static int tryStartLadderTopExit(void);
static int isStandingOnSolid(void);
static int findStandingSpawnY(int spawnX, int preferredTopY);
static int touchesHazard(void);
static int fellOutOfLevel(void);
static void recoverFromInvincibleFall(void);
static int touchesWinTile(void);
static void handleLevelTransitions(void);
static void putText(int col, int row, const char* str);
static void clearHud(void);

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
//                ASSET LOADING AND SETUP
// ======================================================
static void initBgAssets(void) {
    // Load shared background art and reserve a palette row for font text.
    // Load the tileset palette and tiles for the gameplay backgrounds.
    DMANow(3, (void*)tilesetPal, BG_PALETTE, tilesetPalLen / 2);
    DMANow(3, (void*)tilesetTiles, CHARBLOCK[1].tileimg, tilesetTilesLen / 2);

    // Palette index 0 is the global BG backdrop color.
    // Because the tilemaps now use index 0 as transparent, this is the sky
    // color that will show through anywhere index 0 appears.
    BG_PALETTE[0] = SKY_COLOR;

    // Give the HUD/menu font its own dedicated palette row so the text does
    // not inherit random colors from the tileset palette.
    BG_PALETTE[FONT_PALROW * 16 + 0] = SKY_COLOR;
    BG_PALETTE[FONT_PALROW * 16 + 1] = FONT_COLOR;
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
    instantGrowCheat = 0;
    invincibilityCheat = 0;
    cheatModeEnabled = 0;

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
            clearHud();
            drawMenuScreen("BEYOND THE SKY", "PRESS START", "A: NEXT   B: BACK", "SELECT+LEFT/RIGHT/DOWN = MAP CHEATS");
            break;

        case STATE_INSTRUCTIONS:
            clearHud();
            drawMenuScreen("INSTRUCTIONS", "LEFT/RIGHT MOVE  UP JUMP", "UP/DOWN CLIMB  START PAUSE", "GET WATER IN SKY LEVEL, RETURN, PRESS B ON SOIL");
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
    // Show only the text layer for menu-style screens and hide all sprites.
    // Menu/state screens only need BG0 text.
    // The blue backdrop color will fill the rest of the screen.
    REG_DISPCTL = MODE(0) | BG_ENABLE(0);

    // Keep menu text positioned correctly.
    setBackgroundOffset(0, 0, 0);

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
    drawMenuScreen("BEYOND THE SKY", "PRESS START", "A: NEXT   B: BACK", "SELECT+UP TOGGLE GROW CHEAT");
}

static void goToInstructions(void) {
    // Switch to the instructions screen.
    state = STATE_INSTRUCTIONS;
    enableMenuDisplay();
    clearHud();
    drawMenuScreen("INSTRUCTIONS", "LEFT/RIGHT MOVE  A JUMP", "UP/DOWN CLIMB  START PAUSE", "GET WATER IN SKY LEVEL, RETURN, PRESS B ON SOIL");
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

    // Reapply the dynamic farmland / beanstalk visuals every time home loads.
    refreshHomeBeanstalkVisuals();

    hOff = 0;
    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, hOff, vOff);
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

    hOff = levelWidth - SCREENWIDTH;
    if (hOff < 0) hOff = 0;

    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, hOff, vOff);
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
    // Remember the gameplay state we came from, then open the pause screen.
    gameplayStateBeforePause = state;
    state = STATE_PAUSE;
    enableMenuDisplay();
    clearHud();
    drawPauseScreen();
}

static void goToWin(void) {
    // Switch to the win screen and summarize the cheat state used during the run.
    state = STATE_WIN;
    enableMenuDisplay();
    clearHud();
    drawMenuScreen("YOU WIN", "THE BEANSTALK BLOOMED", "PRESS START TO PLAY AGAIN", instantGrowCheat ? "CHEAT MODE WAS ON" : "CHEAT MODE WAS OFF");
}

static void goToLose(int levelToReturnTo) {
    // Switch to the lose screen and remember which level should be reloaded on respawn.
    loseReturnLevel = levelToReturnTo;
    state = STATE_LOSE;
    enableMenuDisplay();
    clearHud();
    drawMenuScreen("YOU WILTED", "HAZARD HIT OR FALL RESET", "PRESS START TO RESPAWN", "THIS STILL COUNTS AS YOUR RESET/KILL OBSTACLE");
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
    // Title screen input: continue to instructions.
    if (BUTTON_PRESSED(BUTTON_START) || BUTTON_PRESSED(BUTTON_A)) {
        goToInstructions();
    }
}

static void updateInstructions(void) {
    // Instructions screen input: go back to title or continue into gameplay.
    if (BUTTON_PRESSED(BUTTON_B)) {
        goToStart();
    }
    if (BUTTON_PRESSED(BUTTON_START) || BUTTON_PRESSED(BUTTON_A)) {
        // Start a completely fresh run from home.
        inventoryFlags = 0;
        beanstalkGrowthStage = 0;
        instantGrowCheat = 0;
        invincibilityCheat = 0;   // <-- ADD THIS LINE
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
        state = gameplayStateBeforePause;
        enableGameplayDisplay();
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
    // Win screen input: clear run progress and return to the title screen.
    if (BUTTON_PRESSED(BUTTON_START)) {
        // Fresh run after winning.
        inventoryFlags = 0;
        beanstalkGrowthStage = 0;
        instantGrowCheat = 0;
        invincibilityCheat = 0; 
        goToStart();
    }
}

// ======================================================
//                CORE GAMEPLAY UPDATE HELPERS
// ======================================================
static void updateGameplayCommon(void) {
    // Handle the shared gameplay loop used by all playable levels.
    // START pauses the game.
    if (BUTTON_PRESSED(BUTTON_START)) {
        goToPause();
        return;
    }

    // Optional cheat toggle: instant grow
    if (BUTTON_PRESSED(BUTTON_SELECT) && BUTTON_HELD(BUTTON_UP)) {
        instantGrowCheat = !instantGrowCheat;

        // The grow cheat temporarily forces the beanstalk to its final stage.
        // Refresh the home visuals immediately if the player is currently there.
        if (currentLevel == LEVEL_HOME) {
            refreshHomeBeanstalkVisuals();
        }
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
    // - falling out of the level snaps the player back to the current respawn
    if (invincibilityCheat) {
        if (fellOutOfLevel()) {
            recoverFromInvincibleFall();
            return;
        }
    } else if (touchesHazard() || fellOutOfLevel()) {
        goToLose(currentLevel);
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

    // Advance animation every few game frames.
    // Lower number = faster animation.
    player.animCounter++;
    if (player.animCounter >= 6) {
        player.animCounter = 0;
        player.animFrame = (player.animFrame + 1) % PLAYER_ANIM_FRAMES;
    }
}

static void updatePlayerMovement(void) {
    // Resolve player input, climbing, jumping, gravity, and collision one pixel at a time.
    int dx = 0;
    int step;
    int remaining;
    int wantsClimbUp;
    int wantsClimbDown;
    int wasActivelyMoving = 0;
    int onClimbNow;

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
    onClimbNow = onClimbTile();

    // --------------------------------------------------
    // Climbing logic
    // --------------------------------------------------
    if (onClimbNow && (wantsClimbUp || wantsClimbDown)) {
        player.climbing = 1;
        player.yVel = 0;
        player.grounded = 0;
    } else if (!onClimbNow) {
        player.climbing = 0;
    }

    // --------------------------------------------------
    // Jump logic
    //
    // Normal jump only when not climbing.
    // Ladder-top exit is handled separately below.
    // --------------------------------------------------
    if (!player.climbing && BUTTON_PRESSED(BUTTON_UP) && player.grounded) {
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
                int nextY = player.y + step;

                // Normal climb movement: keep moving if the next position is
                // open and still overlaps climb tiles.
                if (canMoveTo(player.x, nextY) &&
                    onClimbTileAt(player.x, nextY)) {
                    player.y = nextY;
                }
                // Special case: climbing upward at the top of the ladder / vine.
                // Leave climb mode and give the player a mini upward launch.
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

        // If the body no longer overlaps the climb tiles, leave climb mode.
        // If the body no longer overlaps the climb tiles, leave climb mode.
        // Do NOT zero yVel here — if tryStartLadderTopExit just gave the player
        // an upward boost, zeroing it would cancel the launch immediately.
        if (!onClimbTile()) {
            player.climbing = 0;
            player.grounded = isStandingOnSolid();
            // yVel intentionally preserved so ladder-exit boost is not cancelled
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

    // --------------------------------------------------
    // Decide whether to animate this frame
    // --------------------------------------------------
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

    cloudHOff = hOff / 3;
}


// ======================================================
//        HOME BEANSTALK / FARMLAND DYNAMIC VISUALS
// ======================================================

// Return the active beanstalk growth stage.
// The grow cheat temporarily forces the visuals and climb logic to full growth.
static int getEffectiveBeanstalkGrowthStage(void) {
    if (instantGrowCheat) {
        return 3;
    }

    return beanstalkGrowthStage;
}

// Read a tile entry out of the original exported home foreground map.
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
    int stage;
    int row;
    int col;

    // These edits only apply to the home foreground map.
    if (currentLevel != LEVEL_HOME) {
        return;
    }

    stage = getEffectiveBeanstalkGrowthStage();

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
    // Stamp the decorative half-grown beanstalk top so the stalk does not
    // end in a hard straight cutoff.
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
    int stage;

    if (!isClimbPixel(currentLevel, x, y)) {
        return 0;
    }

    if (currentLevel != LEVEL_HOME) {
        return 1;
    }

    stage = getEffectiveBeanstalkGrowthStage();

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
static int canMoveTo(int newX, int newY) {
    // Test the player hitbox against blocked collision pixels.
    //
    // Special case:
    // once the player starts falling below the bottom edge of the level,
    // treat that space as open so they can fully fall out of the map.
    // That allows normal fall deaths and invincibility-cheat recovery
    // to work instead of getting stuck on the bottom boundary.
    int rightX = newX + player.width - 1;
    int bottomY = newY + player.height - 1;

    if (newX < 0 || rightX >= levelWidth || newY < 0) {
        return 0;
    }

    // Fully below the level: allow movement
    if (newY >= levelHeight) {
        return 1;
    }

    // Partially below the level:
    // only test the top corners that are still inside the map
    if (bottomY >= levelHeight) {
        return !isBlockedPixel(currentLevel, newX, newY)
            && !isBlockedPixel(currentLevel, rightX, newY);
    }

    // Normal full 4-corner collision test
    return !isBlockedPixel(currentLevel, newX, newY)
        && !isBlockedPixel(currentLevel, rightX, newY)
        && !isBlockedPixel(currentLevel, newX, bottomY)
        && !isBlockedPixel(currentLevel, rightX, bottomY);
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
// so that the player is not permanently trapped.
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

    // Fallback: no clean gap found (tight ceiling, etc.).
    // Still force-exit climb mode so the player is never permanently stuck.
    player.climbing = 0;
    player.grounded = 0;
    player.yVel     = LADDER_EXIT_BOOST;
    return 1;
}

static int isStandingOnSolid(void) {
    // Sample the player's feet to decide whether they are standing on solid ground.
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
    // the player. Snap them back to the current level respawn instead.
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
    // Sample the player's feet area for transition tiles.
    // Checking left / center / right is more reliable than a single point.
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
//
// This is scaffolding for the future inventory system.
// When bonemeal and water droplet are implemented later, just OR in:
//   inventoryFlags |= INVENTORY_BONEMEAL;
//   inventoryFlags |= INVENTORY_WATER;
// --------------------------------------------------
static const char* getInventoryText(void) {
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
    // Refresh HUD text, scroll the backgrounds, and draw all visible sprites.
    clearHud();
    drawHudText();

    setBackgroundOffset(1, hOff, vOff);
    setBackgroundOffset(2, hOff, vOff);

    drawSprites();
}

static void drawSprites(void) {
    // Write the current frame's sprite attributes into shadow OAM.
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

    // Hide everything else
    for (int i = 5; i < 128; i++) {
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

static void drawMenuScreen(const char* title, const char* line1, const char* line2, const char* line3) {
    // Draw a simple four-line text menu layout on BG0.
    // This function should only DRAW the menu screen.
    setBackgroundOffset(1, 0, 0);
    setBackgroundOffset(2, 0, 0);
    putText(6, 5, title);
    putText(3, 10, line1);
    putText(3, 13, line2);
    putText(3, 16, line3);
}

static void drawPauseScreen(void) {
    // Draw pause text and hide gameplay sprites while paused.
    clearHud();
    putText(11, 7, "PAUSED");
    putText(6, 11, "START TO RESUME");
    putText(4, 14, "SELECT TO RETURN TO TITLE");
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