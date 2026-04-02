#include "levels.h"

#include "homebase_background.h"
#include "homebase_foreground.h"
#include "level_background.h"
#include "levelone_foreground.h"
#include "leveltwo_foreground.h"

#include "homebase_collisionMap.h"
#include "levelone_collisionMap.h"
#include "leveltwo_collisionMap.h"

// ======================================================
//        INTERNAL HELPER: COPY ONE SCREENBLOCK
// ======================================================

static void copyScreenblock(int screenblock, const unsigned short* src) {
    DMANow(3, (void*)src, SCREENBLOCK[screenblock].tilemap, 1024);
}

// ======================================================
//        INTERNAL HELPER: LOAD 64x32 "WIDE" MAP
// ======================================================

static void loadWideMap(int screenblock, const unsigned short* map) {
    copyScreenblock(screenblock, map);
    copyScreenblock(screenblock + 1, map + 1024);
}

// ======================================================
//        INTERNAL HELPER: LOAD 32x64 "TALL" MAP
// ======================================================

static void loadTallMap(int screenblock, const unsigned short* map) {
    copyScreenblock(screenblock, map);
    copyScreenblock(screenblock + 1, map + 1024);
}

// ======================================================
//          CONFIGURE BG1 / BG2 FOR MAP SHAPE
// ======================================================

void configureMapBackgroundsForLevel(int level) {
    if (level == LEVEL_HOME) {
        setupBackground(1, BG_PRIORITY(1) | BG_CHARBLOCK(1) | BG_SCREENBLOCK(BG_BACK_SB_A)  | BG_4BPP | BG_SIZE_TALL);
        setupBackground(2, BG_PRIORITY(0) | BG_CHARBLOCK(1) | BG_SCREENBLOCK(BG_FRONT_SB_A) | BG_4BPP | BG_SIZE_TALL);
    } else {
        setupBackground(1, BG_PRIORITY(1) | BG_CHARBLOCK(1) | BG_SCREENBLOCK(BG_BACK_SB_A)  | BG_4BPP | BG_SIZE_WIDE);
        setupBackground(2, BG_PRIORITY(0) | BG_CHARBLOCK(1) | BG_SCREENBLOCK(BG_FRONT_SB_A) | BG_4BPP | BG_SIZE_WIDE);
    }
}

// ======================================================
//                 LOAD VISUAL TILEMAPS
// ======================================================

void loadLevelMaps(int level) {
    clearScreenblock(BG_BACK_SB_A);
    clearScreenblock(BG_BACK_SB_B);
    clearScreenblock(BG_FRONT_SB_A);
    clearScreenblock(BG_FRONT_SB_B);

    if (level == LEVEL_HOME) {
        loadTallMap(BG_BACK_SB_A,  homebase_backgroundMap);
        loadTallMap(BG_FRONT_SB_A, homebase_foregroundMap);
    } else if (level == LEVEL_ONE) {
        loadWideMap(BG_BACK_SB_A,  level_backgroundMap);
        loadWideMap(BG_FRONT_SB_A, levelone_foregroundMap);
    } else {
        loadWideMap(BG_BACK_SB_A,  level_backgroundMap);
        loadWideMap(BG_FRONT_SB_A, leveltwo_foregroundMap);
    }
}

// ======================================================
//                 WORLD SIZE LOOKUP
// ======================================================

int getLevelPixelWidth(int level) {
    if (level == LEVEL_HOME) return HOME_PIXEL_W;
    if (level == LEVEL_ONE)  return LEVEL1_PIXEL_W;
    return LEVEL2_PIXEL_W;
}

int getLevelPixelHeight(int level) {
    if (level == LEVEL_HOME) return HOME_PIXEL_H;
    if (level == LEVEL_ONE)  return LEVEL1_PIXEL_H;
    return LEVEL2_PIXEL_H;
}

// ======================================================
//      INTERNAL HELPER: READ ONE BYTE FROM COLLISION MAP
// ======================================================
//
// Important:
// These collision maps were exported as full 8bpp bitmaps at the
// actual world pixel size:
//
//   homebase  = 256 x 512
//   level one = 512 x 256
//   level two = 512 x 256
//
// That means each entry in the bitmap already corresponds to ONE
// world pixel.
//
static u8 collisionByteFromBitmap(const unsigned short* bitmap, int pixelWidth, int pixelHeight, int x, int y) {
    const unsigned char* bytes = (const unsigned char*)bitmap;

    if (x < 0 || y < 0 || x >= pixelWidth || y >= pixelHeight) {
        return COL_CANNOT_GO;
    }

    return bytes[OFFSET(x, y, pixelWidth)];
}

// ======================================================
//        PUBLIC HELPER: COLLISION AT WORLD PIXEL
// ======================================================

u8 collisionAtPixel(int level, int x, int y) {
    if (x < 0 || y < 0) {
        return COL_CANNOT_GO;
    }

    // The collision maps are full-size 8bpp bitmaps at world-pixel size.

    // HomeBase
    if (level == LEVEL_HOME) {
        return collisionByteFromBitmap(
            homebase_collisionMapBitmap,
            HOME_PIXEL_W,
            HOME_PIXEL_H,
            x,
            y
        );
    }

    // Level One
    if (level == LEVEL_ONE) {
        return collisionByteFromBitmap(
            levelone_collisionMapBitmap,
            LEVEL1_PIXEL_W,
            LEVEL1_PIXEL_H,
            x,
            y
        );
    }

    // Level Two
    return collisionByteFromBitmap(
        leveltwo_collisionMapBitmap,
        LEVEL2_PIXEL_W,
        LEVEL2_PIXEL_H,
        x,
        y
    );
}
// ======================================================
//              CONVENIENCE QUERY FUNCTIONS
// ======================================================

int isBlockedPixel(int level, int x, int y) {
    return collisionAtPixel(level, x, y) == COL_CANNOT_GO;
}

int isHazardPixel(int level, int x, int y) {
    return collisionAtPixel(level, x, y) == COL_DEATH;
}

int isClimbPixel(int level, int x, int y) {
    return collisionAtPixel(level, x, y) == COL_CLIMB;
}

// ------------------------------------------------------
// Transition tile helpers
// These return true when the sampled pixel matches the
// correct palette index in the collision map.
// ------------------------------------------------------

int isHomeToLevel1Pixel(int level, int x, int y) {
    u8 value = collisionAtPixel(level, x, y);

    // Treat either home->level1 transition tile as valid.
    return value == COL_HOME_TO_LEVEL1_BOTTOM
        || value == COL_HOME_TO_LEVEL1_TOP;
}

int isLevel1ToHomePixel(int level, int x, int y) {
    u8 value = collisionAtPixel(level, x, y);

    // Treat either level1->home transition tile as valid.
    return value == COL_LEVEL1_TO_HOME_BOTTOM
        || value == COL_LEVEL1_TO_HOME_TOP;
}

int isHomeToLevel2Pixel(int level, int x, int y) {
    return collisionAtPixel(level, x, y) == COL_HOME_TO_LEVEL2;
}

int isLevel2ToHomePixel(int level, int x, int y) {
    return collisionAtPixel(level, x, y) == COL_LEVEL2_TO_HOME;
}