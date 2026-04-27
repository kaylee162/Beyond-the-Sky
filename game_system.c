#include "game_internal.h"

/* DEV NOTES FOR THIS FILE:
 *
 * This file is responsible for the low-level visual setup that the rest of the
 * the game depends on. 
 * 
 * More specifically, it handles:
 *   - loading background and sprite assets into VRAM
 *   - repacking sprite graphics from the exported sprite sheet into OBJ memory
 *   - setting up palette rows for tiles, fonts, and sprites
 *   - running the real-time daytime sky color cycle
 *   - restoring the menu palette when the game is not in active gameplay
 *   - switching between gameplay display mode and menu display mode
 *   - initializing the shared graphics environment when a new game starts
 *
 * In the larger scope of the game:
 * this module is the visual foundation for the whole game. 
 * Other files decide what state the game is in and what should
 * happen, but this file makes sure the hardware is configured correctly so that
 * the right backgrounds, tiles, sprites, and palette colors actually appear on
 * the screen.
 *
 * This file does not control movement, collision, or game progression directly.
 * Instead, it provides the shared rendering setup that those systems rely on.
 */

// These functions are implemented in this file, but are also used by other
// modules, so they remain explicitly declared near the top for clarity.
void updateDaytimePalette(void);
void setMenuPalette(void);

// These helpers live in other modules but are referenced through the shared
// internal header / project architecture. They are listed here so this module
// can interact cleanly with the broader game systems when needed.
int bodyHitsSolidAt(int x, int y);
int canMoveTo(int newX, int newY);
int onClimbTile(void);
int onClimbTileAt(int x, int y);
int isAtTopOfClimb(int x, int y);
int tryStartLadderTopExit(void);
int isStandingOnSolid(void);
int findStandingSpawnY(int spawnX, int preferredTopY);
int touchesHazard(void);
int playerTouchesBee(void);
int fellOutOfLevel(void);
void recoverFromInvincibleFall(void);
int touchesWinTile(void);
void handleLevelTransitions(void);
void putText(int col, int row, const char* str);
void clearHud(void);

// Tracks which instructions page is currently being shown across menu states.
int instructionsPage = 0;

// --------------------------------------------------
//                SPRITE TILE REPACKING
// --------------------------------------------------

// Copies a rectangular group of sprite tiles from the exported sheet into a contiguous OBJ tile block. 
// this makes multi-tile sprites easier to manage at runtime.
void copySpriteBlockFromSheet(const unsigned short* sheetTiles, int sheetWidthTiles,
    int srcTileX, int srcTileY, int blockWidthTiles, int blockHeightTiles, int dstTileIndex) {

    // OBJ tile memory begins at 0x6010000 in Mode 0 sprite VRAM.
    volatile unsigned short* objTiles = (volatile unsigned short*)0x6010000;

    // Walk across the requested rectangular block one 8x8 tile at a time.
    for (int tileRow = 0; tileRow < blockHeightTiles; tileRow++) {
        for (int tileCol = 0; tileCol < blockWidthTiles; tileCol++) {
            int srcIndex = ((srcTileY + tileRow) * sheetWidthTiles) + (srcTileX + tileCol);
            int dstIndex = dstTileIndex + (tileRow * blockWidthTiles) + tileCol;

            // Each 4bpp tile occupies 32 bytes, which is 16 unsigned shorts.
            for (int i = 0; i < 16; i++) {
                objTiles[dstIndex * 16 + i] = sheetTiles[srcIndex * 16 + i];
            }
        }
    }
}

// ======================================================
//                RUNTIME DAYTIME PALETTE CYCLE
// ======================================================

// blends between two 15-bit GBA colors and returns the intermediate result. The daytime cycle uses this for smooth sky transitions
unsigned short lerpSkyColor(unsigned short startColor, unsigned short endColor, int step, int totalSteps) {
    // Break both source colors into their 5-bit channel components.
    int startR = startColor & 31;
    int startG = (startColor >> 5) & 31;
    int startB = (startColor >> 10) & 31;

    int endR = endColor & 31;
    int endG = (endColor >> 5) & 31;
    int endB = (endColor >> 10) & 31;

    // Interpolate each color channel independently.
    int r = startR + ((endR - startR) * step) / totalSteps;
    int g = startG + ((endG - startG) * step) / totalSteps;
    int b = startB + ((endB - startB) * step) / totalSteps;

    // Rebuild and return the blended 15-bit color.
    return RGB(r, g, b);
}

// Updates the active sky color for the current frame and writes it into
// the background palette. It also keeps both font systems synced so text
// transparency/background pixels blend into the animated sky color.
void updateDaytimePalette(void) {
    // Total cycle = 4 transitions:
    // morning -> day -> sunset -> night -> morning
    const int segmentLength = DAYTIME_CYCLE_SEGMENT_FRAMES;
    const int totalCycleLength = segmentLength * 4;

    // Offset lets a fresh gameplay start begin at a chosen time of day.
    int cycleFrame =
        (frameCounter + DAYTIME_CYCLE_START_OFFSET_FRAMES) % totalCycleLength;

    int segmentFrame = cycleFrame % segmentLength;

    unsigned short startColor;
    unsigned short endColor;

    if (cycleFrame < segmentLength) {
        // Early morning -> bright day
        startColor = SKY_COLOR_EARLY_MORNING;
        endColor   = SKY_COLOR_DAY;

    } else if (cycleFrame < segmentLength * 2) {
        // Day -> sunset
        startColor = SKY_COLOR_DAY;
        endColor   = SKY_COLOR_SUNSET;

    } else if (cycleFrame < segmentLength * 3) {
        // Sunset -> night
        startColor = SKY_COLOR_SUNSET;
        endColor   = SKY_COLOR_NIGHT;

    } else {
        // Night -> early morning
        startColor = SKY_COLOR_NIGHT;
        endColor   = SKY_COLOR_EARLY_MORNING;
    }

    // Blend between the two colors for this section of the cycle.
    unsigned short currentSkyColor =
        lerpSkyColor(startColor, endColor, segmentFrame, segmentLength);

    // Main world background color.
    BG_PALETTE[0] = currentSkyColor;

    // Original menu/font system transparency entry.
    BG_PALETTE[FONT_PALROW * 16 + 0] = currentSkyColor;

    // Custom HUD font transparency entry.
    BG_PALETTE[HUD_CUSTOM_FONT_PALROW * 16 + 0] = currentSkyColor;

    // Re-assert the custom HUD font visible colors in case another palette
    // load overwrote them earlier.
    BG_PALETTE[HUD_CUSTOM_FONT_PALROW * 16 + 1] = RGB(0, 0, 0);       // black fill
    BG_PALETTE[HUD_CUSTOM_FONT_PALROW * 16 + 2] = RGB(31, 31, 31);    // white outline
}

// Restores the fixed menu palette colors so menu screens are not tinted by the runtime daytime cycle
void setMenuPalette(void) {
    // Restore the original tilemap background color for menu/state screens.
    BG_PALETTE[0] = tilesetPal[0];

    // Match both font transparency colors to the menu background.
    BG_PALETTE[FONT_PALROW * 16 + 0] = tilesetPal[0];
    BG_PALETTE[HUD_CUSTOM_FONT_PALROW * 16 + 0] = tilesetPal[0];

    // Keep the HUD font's real colors alive in case the BG palette was reloaded.
    BG_PALETTE[HUD_CUSTOM_FONT_PALROW * 16 + 1] = RGB(0, 0, 0);
    BG_PALETTE[HUD_CUSTOM_FONT_PALROW * 16 + 2] = RGB(31, 31, 31);
}

// ======================================================
//                ASSET LOADING AND SETUP
// ======================================================

// loads the shared background graphics and prepares the font palette row. This establishes the base tile and palette data used by the game's background layers
void initBgAssets(void) {
    // Load the full 256-entry BG palette exactly as grit exported it.
    // Row 0 = normal tiles
    // Row 1 = castle tiles
    DMANow(3, (void*)tilesetPal, BG_PALETTE, tilesetPalLen / 2);

    // The main tileset palette reloads all 256 BG colors,
    // so restore the custom HUD font colors afterward.
    hudFontLoadPalette();

    // Load the shared 4bpp background tiles.
    DMANow(3, (void*)tilesetTiles, CHARBLOCK[1].tileimg, tilesetTilesLen / 2);

    // Font uses its own row, so keep its visible text color stable.
    BG_PALETTE[FONT_PALROW * 16 + 1] = FONT_COLOR;

    updateDaytimePalette();
}

// loads sprite palette data and repacks the sprite sheet art into clean OBJ tile regions. This prepares the player, collectible, and enemy graphics for rendering
void initObjAssets(void) {
    // Load the sprite palette rows used by the project.
    // palette row 0 = player
    // palette row 1 = all other world sprites
    DMANow(3, (void*)spriteSheetPal, &SPRITE_PAL[16 * PLAYER_PALROW], 16);
    DMANow(3, (void*)(spriteSheetPal + 16), &SPRITE_PAL[16 * WORLD_SPRITE_PALROW], 16);

    // Clear OBJ tile memory first so stale art never appears accidentally.
    static unsigned short zero = 0;
    DMANow(3, &zero, (volatile unsigned short*)0x6010000, (32 * 32) | DMA_SOURCE_FIXED);

    // --------------------------------------------------
    // Copy all player animation rows from the exported sheet.
    //
    // Each frame is 2x4 tiles = 16x32 pixels.
    // The sheet stores four directional rows:
    //   right at y = 0
    //   left  at y = 4
    //   up    at y = 8
    //   down  at y = 12
    // --------------------------------------------------
    for (int frame = 0; frame < PLAYER_ANIM_FRAMES; frame++) {
        int srcTileX = frame * 2;

        // Right-facing movement row
        copySpriteBlockFromSheet(
            spriteSheetTiles, 32,
            srcTileX, 0,
            2, 4,
            OBJ_TILE_PLAYER_RIGHT + frame * PLAYER_TILES_PER_FRAME
        );

        // Left-facing movement row
        copySpriteBlockFromSheet(
            spriteSheetTiles, 32,
            srcTileX, 4,
            2, 4,
            OBJ_TILE_PLAYER_LEFT + frame * PLAYER_TILES_PER_FRAME
        );

        // Up / climbing up row
        copySpriteBlockFromSheet(
            spriteSheetTiles, 32,
            srcTileX, 8,
            2, 4,
            OBJ_TILE_PLAYER_UP + frame * PLAYER_TILES_PER_FRAME
        );

        // Down / falling / climbing down row
        copySpriteBlockFromSheet(
            spriteSheetTiles, 32,
            srcTileX, 12,
            2, 4,
            OBJ_TILE_PLAYER_DOWN + frame * PLAYER_TILES_PER_FRAME
        );
    }

    // --------------------------------------------------
    // Copy bean sprout sprite art.
    //
    // Sheet position: (0, 21)
    // Size: 2x3 tiles
    //
    // This gives a contiguous 6-tile block in OBJ VRAM:
    //   top    16x16 uses tiles 0..3
    //   bottom 16x8  uses tiles 4..5
    // --------------------------------------------------
    copySpriteBlockFromSheet(
        spriteSheetTiles, 32,
        0, 21,
        2, 3,
        OBJ_TILE_BEAN_SPROUT
    );

    // --------------------------------------------------
    // Copy bonemeal sprite art.
    // Two 16x16 frames stored as 2x2 tile blocks.
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
    // Copy water droplet sprite art.
    // Two 16x16 frames stored as 2x2 tile blocks.
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
    // Copy bee enemy animation frames.
    // Each frame is 2x3 tiles = 16x24 pixels.
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

// Initializes the display, backgrounds, sprite system, and shared visual assets needed by the game. This is the main graphics setup entry point
void initGraphics(void) {
    // Initialize Mode 0 and prepare the GBA video system.
    initMode0();

    // Start with the full gameplay layer set available by default.
    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1) | BG_ENABLE(2)
        | SPRITE_ENABLE | SPRITE_MODE_1D;

    // BG0 is reserved for HUD text and menu text.
    setupBackground(0, BG_PRIORITY(0) | BG_CHARBLOCK(0)
        | BG_SCREENBLOCK(HUD_SCREENBLOCK) | BG_4BPP | BG_SIZE_SMALL);

    // BG1/BG2 shape and screenblock usage depends on the level layout.
    configureMapBackgroundsForLevel(LEVEL_HOME);

    // Clear visible/background memory regions so old data does not linger.
    hideSprites();
    clearCharBlock(0);
    clearScreenblock(HUD_SCREENBLOCK);
    clearScreenblock(BG_BACK_SB_A);
    clearScreenblock(BG_BACK_SB_B);
    clearScreenblock(BG_FRONT_SB_A);
    clearScreenblock(BG_FRONT_SB_B);

    // Initialize text rendering, then load BG and OBJ assets.
    fontInit(0, 0);

    // Load the custom HUD font after the normal menu font.
    // It starts at tile 256 so title/menu text still uses the old font.
    hudFontInit(0, HUD_CUSTOM_FONT_TILEBASE);

    initBgAssets();
    initObjAssets();
}

// loads the shared menu tilemap background into VRAM. Title, instructions, pause, win, and lose screens all reuse this same background
void loadMenuBackground(void) {
    // Clear the old BG1 map data before loading the menu tilemap.
    clearScreenblock(BG_BACK_SB_A);

    // Configure BG1 as the decorative menu background layer.
    setupBackground(
        1,
        BG_PRIORITY(1) | BG_CHARBLOCK(1) | BG_SCREENBLOCK(BG_BACK_SB_A) | BG_4BPP | BG_SIZE_SMALL
    );

    // Copy the shared 32x32 title/menu tilemap into BG1.
    DMANow(3, (void*)titleMap, SCREENBLOCK[BG_BACK_SB_A].tilemap, 1024);

    // Keep the menu background fixed to the screen.
    setBackgroundOffset(1, 0, 0);
}

// Enables the background layers and sprite settings used during gameplay. It restores the normal display configuration after leaving menu states
void enableGameplayDisplay(void) {
    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1) | BG_ENABLE(2)
        | SPRITE_ENABLE | SPRITE_MODE_1D;
}

// enables the simpler display configuration used by menu and state screens. This keeps menu presentation separate from gameplay rendering
void enableMenuDisplay(void) {
    // Menus use text on BG0 and the decorative tilemap on BG1.
    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1);

    // Reload the shared menu background whenever entering a menu/state screen.
    loadMenuBackground();

    // Keep both menu layers fixed to the screen origin.
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);

    // Hide all sprites so no gameplay OBJ art leaks into menus.
    hideSprites();
}