#include "levels.h"

u16 homeMap[HOME_MAP_W * HOME_MAP_H];
u16 skyMap[SKY_MAP_W * SKY_MAP_H];
u16 cloudMap[HOME_MAP_W * HOME_MAP_H];

static u8 homeCollision[HOME_MAP_W * HOME_MAP_H];
static u8 skyCollision[SKY_MAP_W * SKY_MAP_H];

static void setHomeTile(int x, int y, u16 tile, u8 collision) {
    if (x < 0 || x >= HOME_MAP_W || y < 0 || y >= HOME_MAP_H) return;
    homeMap[OFFSET(x, y, HOME_MAP_W)] = tile;
    homeCollision[OFFSET(x, y, HOME_MAP_W)] = collision;
}

static void setSkyTile(int x, int y, u16 tile, u8 collision) {
    if (x < 0 || x >= SKY_MAP_W || y < 0 || y >= SKY_MAP_H) return;
    skyMap[OFFSET(x, y, SKY_MAP_W)] = tile;
    skyCollision[OFFSET(x, y, SKY_MAP_W)] = collision;
}

static void buildCloudLayer(void) {
    int x;
    int y;

    for (y = 0; y < HOME_MAP_H; y++) {
        for (x = 0; x < HOME_MAP_W; x++) {
            cloudMap[OFFSET(x, y, HOME_MAP_W)] = TILE_SKY;
        }
    }

    for (x = 2; x < 8; x++) {
        cloudMap[OFFSET(x, 4, HOME_MAP_W)] = TILE_CLOUD;
    }
    for (x = 18; x < 24; x++) {
        cloudMap[OFFSET(x, 7, HOME_MAP_W)] = TILE_CLOUD;
    }
    for (x = 34; x < 41; x++) {
        cloudMap[OFFSET(x, 5, HOME_MAP_W)] = TILE_CLOUD;
    }
    for (x = 49; x < 56; x++) {
        cloudMap[OFFSET(x, 9, HOME_MAP_W)] = TILE_CLOUD;
    }
}

void initLevelData(void) {
    int x;
    int y;

    for (y = 0; y < HOME_MAP_H; y++) {
        for (x = 0; x < HOME_MAP_W; x++) {
            homeMap[OFFSET(x, y, HOME_MAP_W)] = TILE_SKY;
            homeCollision[OFFSET(x, y, HOME_MAP_W)] = COL_EMPTY;
        }
    }

    for (y = 0; y < SKY_MAP_H; y++) {
        for (x = 0; x < SKY_MAP_W; x++) {
            skyMap[OFFSET(x, y, SKY_MAP_W)] = TILE_SKY;
            skyCollision[OFFSET(x, y, SKY_MAP_W)] = COL_EMPTY;
        }
    }

    buildCloudLayer();

    /* ---------------- HOME LEVEL ---------------- */
    for (y = 28; y < HOME_MAP_H; y++) {
        for (x = 0; x < HOME_MAP_W; x++) {
            setHomeTile(x, y, (y == 28) ? TILE_GRASS : TILE_DIRT, COL_SOLID);
        }
    }

    for (x = 8; x < 14; x++) setHomeTile(x, 22, TILE_PLATFORM, COL_SOLID);
    for (x = 19; x < 25; x++) setHomeTile(x, 18, TILE_PLATFORM, COL_SOLID);
    for (x = 28; x < 33; x++) setHomeTile(x, 14, TILE_PLATFORM, COL_SOLID);

    /* deposit patch */
    setHomeTile(6, 27, TILE_SOIL, COL_SOIL);
    setHomeTile(7, 27, TILE_SOIL, COL_SOIL);

    /* sign / portal area to go to side level */
    setHomeTile(58, 27, TILE_SIGN, COL_EMPTY);
    setHomeTile(59, 27, TILE_PORTAL, COL_PORTAL);
    setHomeTile(59, 26, TILE_PORTAL, COL_PORTAL);

    /* ---------------- SKY LEVEL ---------------- */
    for (y = 30; y < SKY_MAP_H; y++) {
        for (x = 0; x < SKY_MAP_W; x++) {
            setSkyTile(x, y, (y == 30) ? TILE_GRASS : TILE_DIRT, COL_SOLID);
        }
    }

    for (x = 4; x < 14; x++) setSkyTile(x, 24, TILE_PLATFORM, COL_SOLID);
    for (x = 18; x < 25; x++) setSkyTile(x, 20, TILE_PLATFORM, COL_SOLID);
    for (x = 31; x < 37; x++) setSkyTile(x, 16, TILE_PLATFORM, COL_SOLID);
    for (x = 43; x < 51; x++) setSkyTile(x, 12, TILE_PLATFORM, COL_SOLID);
    for (x = 55; x < 62; x++) setSkyTile(x, 8, TILE_PLATFORM, COL_SOLID);

    /* a ladder in the side-scrolling level */
    for (y = 21; y <= 29; y++) setSkyTile(22, y, TILE_LADDER, COL_LADDER);

    /* hazards */
    setSkyTile(27, 29, TILE_HAZARD, COL_HAZARD);
    setSkyTile(28, 29, TILE_HAZARD, COL_HAZARD);
    setSkyTile(29, 29, TILE_HAZARD, COL_HAZARD);

    setSkyTile(39, 29, TILE_HAZARD, COL_HAZARD);
    setSkyTile(40, 29, TILE_HAZARD, COL_HAZARD);

    /* return portal */
    setSkyTile(1, 29, TILE_PORTAL, COL_PORTAL);
    setSkyTile(1, 28, TILE_PORTAL, COL_PORTAL);
}

void growHomeBeanstalk(void) {
    int y;

    for (y = 27; y >= 8; y--) {
        setHomeTile(7, y, TILE_BEANSTALK, COL_LADDER);
        setHomeTile(8, y, TILE_BEANSTALK, COL_LADDER);
    }

    for (y = 8; y <= 10; y++) {
        setHomeTile(9, y, TILE_PLATFORM, COL_SOLID);
        setHomeTile(10, y, TILE_PLATFORM, COL_SOLID);
        setHomeTile(11, y, TILE_PLATFORM, COL_SOLID);
    }
}

void loadWideMap(int screenblock, const u16* map, int width, int height) {
    int x;
    int y;

    for (y = 0; y < height; y++) {
        for (x = 0; x < 32; x++) {
            SCREENBLOCK[screenblock].tilemap[OFFSET(x, y, 32)] = map[OFFSET(x, y, width)];
            SCREENBLOCK[screenblock + 1].tilemap[OFFSET(x, y, 32)] = map[OFFSET(x + 32, y, width)];
        }
    }
}

void loadHomeLevelMap(void) {
    loadWideMap(GAME_SCREENBLOCK, homeMap, HOME_MAP_W, HOME_MAP_H);
    loadWideMap(CLOUD_SCREENBLOCK, cloudMap, HOME_MAP_W, HOME_MAP_H);
}

void loadSkyLevelMap(void) {
    loadWideMap(GAME_SCREENBLOCK, skyMap, SKY_MAP_W, SKY_MAP_H);
    loadWideMap(CLOUD_SCREENBLOCK, cloudMap, HOME_MAP_W, HOME_MAP_H);
}

u8 collisionAtPixel(int level, int x, int y) {
    int tileX;
    int tileY;

    if (x < 0 || y < 0) return COL_SOLID;

    tileX = x >> 3;
    tileY = y >> 3;

    if (level == LEVEL_HOME) {
        if (tileX >= HOME_MAP_W || tileY >= HOME_MAP_H) return COL_SOLID;
        return homeCollision[OFFSET(tileX, tileY, HOME_MAP_W)];
    }

    if (tileX >= SKY_MAP_W || tileY >= SKY_MAP_H) return COL_SOLID;
    return skyCollision[OFFSET(tileX, tileY, SKY_MAP_W)];
}

int isSolidPixel(int level, int x, int y) {
    return collisionAtPixel(level, x, y) == COL_SOLID;
}

int isLadderPixel(int level, int x, int y) {
    return collisionAtPixel(level, x, y) == COL_LADDER;
}

int isHazardPixel(int level, int x, int y) {
    return collisionAtPixel(level, x, y) == COL_HAZARD;
}

int isSoilPixel(int level, int x, int y) {
    return collisionAtPixel(level, x, y) == COL_SOIL;
}