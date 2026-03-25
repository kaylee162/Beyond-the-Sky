#ifndef LEVELS_H
#define LEVELS_H

#include "gba.h"
#include "mode0.h"

#define HOME_MAP_W      64
#define HOME_MAP_H      32
#define SKY_MAP_W       64
#define SKY_MAP_H       32

#define HOME_PIXEL_W    (HOME_MAP_W * 8)
#define HOME_PIXEL_H    (HOME_MAP_H * 8)
#define SKY_PIXEL_W     (SKY_MAP_W * 8)
#define SKY_PIXEL_H     (SKY_MAP_H * 8)

#define LEVEL_HOME      0
#define LEVEL_SKY       1

#define COL_EMPTY       0
#define COL_SOLID       1
#define COL_LADDER      2
#define COL_HAZARD      3
#define COL_SOIL        4
#define COL_PORTAL      5

#define TILE_SKY        1
#define TILE_CLOUD      2
#define TILE_GRASS      3
#define TILE_DIRT       4
#define TILE_PLATFORM   5
#define TILE_LADDER     6
#define TILE_HAZARD     7
#define TILE_SOIL       8
#define TILE_BEANSTALK  9
#define TILE_PORTAL     10
#define TILE_SIGN       11

extern u16 homeMap[HOME_MAP_W * HOME_MAP_H];
extern u16 skyMap[SKY_MAP_W * SKY_MAP_H];
extern u16 cloudMap[HOME_MAP_W * HOME_MAP_H];

void initLevelData(void);
void loadWideMap(int screenblock, const u16* map, int width, int height);
void loadHomeLevelMap(void);
void loadSkyLevelMap(void);

u8 collisionAtPixel(int level, int x, int y);
int isSolidPixel(int level, int x, int y);
int isLadderPixel(int level, int x, int y);
int isHazardPixel(int level, int x, int y);
int isSoilPixel(int level, int x, int y);

void growHomeBeanstalk(void);

#endif