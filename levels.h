#ifndef LEVELS_H
#define LEVELS_H

#include "gba.h"
#include "mode0.h"

// ======================================================
//                    LEVEL IDS
// ======================================================

#define LEVEL_HOME      0
#define LEVEL_ONE       1
#define LEVEL_TWO       2

// ======================================================
//                    MAP SIZES (IN TILES)
// ======================================================

#define HOME_MAP_W      32
#define HOME_MAP_H      64

#define LEVEL1_MAP_W    64
#define LEVEL1_MAP_H    32

#define LEVEL2_MAP_W    64
#define LEVEL2_MAP_H    32

// ======================================================
//                    MAP SIZES (IN PIXELS)
// ======================================================

#define HOME_PIXEL_W    (HOME_MAP_W * 8)
#define HOME_PIXEL_H    (HOME_MAP_H * 8)

#define LEVEL1_PIXEL_W  (LEVEL1_MAP_W * 8)
#define LEVEL1_PIXEL_H  (LEVEL1_MAP_H * 8)

#define LEVEL2_PIXEL_W  (LEVEL2_MAP_W * 8)
#define LEVEL2_PIXEL_H  (LEVEL2_MAP_H * 8)

// ======================================================
//                COLLISION MAP PALETTE INDICES
// ======================================================

#define COL_CAN_GO                  0
#define COL_CANNOT_GO               1
#define COL_DEATH                   2
#define COL_CLIMB                   3
#define COL_HOME_TO_LEVEL1          4
#define COL_LEVEL1_TO_HOME          5
#define COL_HOME_TO_LEVEL2          6
#define COL_LEVEL2_TO_HOME          7

// ======================================================
//                    SCREENBLOCKS
// ======================================================

#define HUD_SCREENBLOCK     27
#define BG_BACK_SB_A        28
#define BG_BACK_SB_B        29
#define BG_FRONT_SB_A       30
#define BG_FRONT_SB_B       31

// ======================================================
//                    MAP LOADING
// ======================================================

void configureMapBackgroundsForLevel(int level);
void loadLevelMaps(int level);

// ======================================================
//                 WORLD SIZE HELPERS
// ======================================================

int getLevelPixelWidth(int level);
int getLevelPixelHeight(int level);

// ======================================================
//               COLLISION MAP LOOKUP HELPERS
// ======================================================

u8 collisionAtPixel(int level, int x, int y);

int isBlockedPixel(int level, int x, int y);
int isHazardPixel(int level, int x, int y);
int isClimbPixel(int level, int x, int y);

int isHomeToLevel1Pixel(int level, int x, int y);
int isLevel1ToHomePixel(int level, int x, int y);
int isHomeToLevel2Pixel(int level, int x, int y);
int isLevel2ToHomePixel(int level, int x, int y);

#endif