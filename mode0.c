#include "mode0.h"
#include "sprites.h"
#include "gba.h"

// Initializes the display for Mode 0 tile rendering
void initMode0(void) {
    // Enable Mode 0, background 0, background 1, and sprites
    // *** CHANGE: enable 1D OBJ tile mapping for correct sprite layout ***
    REG_DISPCTL = MODE(0) | BG_ENABLE(0) | BG_ENABLE(1) | SPRITE_ENABLE | SPRITE_MODE_1D;

    // Clear scroll offsets so backgrounds start at the top-left
    REG_BG0HOFF = 0;
    REG_BG0VOFF = 0;
    REG_BG1HOFF = 0;
    REG_BG1VOFF = 0;
    REG_BG2HOFF = 0;
    REG_BG2VOFF = 0;
    REG_BG3HOFF = 0;
    REG_BG3VOFF = 0;

    // Set up BG0 as a HUD / text layer
    REG_BG0CNT = BG_PRIORITY(0) | BG_CHARBLOCK(0) | BG_SCREENBLOCK(28)
        | BG_4BPP | BG_SIZE_SMALL;

    // Set up BG1 as the gameplay background layer
    REG_BG1CNT = BG_PRIORITY(1) | BG_CHARBLOCK(1) | BG_SCREENBLOCK(24)
        | BG_4BPP | BG_SIZE_SMALL;
}

// Generic helper to configure a background in Mode 0
void setupBackground(int bg, unsigned short config) {
    if (bg == 0) {
        REG_BG0CNT = config;
    } else if (bg == 1) {
        REG_BG1CNT = config;
    } else if (bg == 2) {
        REG_BG2CNT = config;
    } else if (bg == 3) {
        REG_BG3CNT = config;
    }
}

// Generic helper to set a background's scroll values
void setBackgroundOffset(int bg, int hOff, int vOff) {
    if (bg == 0) {
        REG_BG0HOFF = hOff;
        REG_BG0VOFF = vOff;
    } else if (bg == 1) {
        REG_BG1HOFF = hOff;
        REG_BG1VOFF = vOff;
    } else if (bg == 2) {
        REG_BG2HOFF = hOff;
        REG_BG2VOFF = vOff;
    } else if (bg == 3) {
        REG_BG3HOFF = hOff;
        REG_BG3VOFF = vOff;
    }
}

// Fills an entire screenblock with the same tile entry
void fillScreenblock(int screenblock, unsigned short entry) {
    for (int i = 0; i < 1024; i++) {
        SCREENBLOCK[screenblock].tilemap[i] = entry;
    }
}

// Clears an entire screenblock to tile 0 with palette row 0
void clearScreenblock(int screenblock) {
    fillScreenblock(screenblock, 0);
}

// Copies a tilemap into a screenblock
void loadMapToScreenblock(int screenblock, const unsigned short* map, int count) {
    DMANow(3, (void*) map, SCREENBLOCK[screenblock].tilemap, count);
}

// Copies tile data into a charblock
void loadTilesToCharblock(int charblock, const unsigned short* tiles, int count) {
    DMANow(3, (void*) tiles, CHARBLOCK[charblock].tileimg, count);
}

// Copies palette data into the background palette memory
void loadBgPalette(const unsigned short* palette, int count) {
    DMANow(3, (void*) palette, BG_PALETTE, count);
}

void clearCharBlock(int charblock) {
    static u32 zero = 0;
    DMANow(3, &zero, CHARBLOCK[charblock].tileimg,
           (8192 / 2) | DMA_SOURCE_FIXED);
}