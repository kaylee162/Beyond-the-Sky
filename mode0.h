#ifndef GBA_MODE0_H
#define GBA_MODE0_H

#include "gba.h"

// Background registers
#define REG_BG0CNT (*(volatile unsigned short*) 0x4000008)
#define REG_BG1CNT (*(volatile unsigned short*) 0x400000A)
#define REG_BG2CNT (*(volatile unsigned short*) 0x400000C)
#define REG_BG3CNT (*(volatile unsigned short*) 0x400000E)

#define REG_BG0HOFF (*(volatile unsigned short*) 0x04000010)
#define REG_BG0VOFF (*(volatile unsigned short*) 0x04000012)
#define REG_BG1HOFF (*(volatile unsigned short*) 0x04000014)
#define REG_BG1VOFF (*(volatile unsigned short*) 0x04000016)
#define REG_BG2HOFF (*(volatile unsigned short*) 0x04000018)
#define REG_BG2VOFF (*(volatile unsigned short*) 0x0400001A)
#define REG_BG3HOFF (*(volatile unsigned short*) 0x0400001C)
#define REG_BG3VOFF (*(volatile unsigned short*) 0x0400001E)

// Background control bits
#define BG_PRIORITY(n)    ((n) & 3)
#define BG_CHARBLOCK(n)   ((n) << 2)
#define BG_MOSAIC         (1 << 6)
#define BG_4BPP           (0 << 7)
#define BG_8BPP           (1 << 7)
#define BG_SCREENBLOCK(n) ((n) << 8)
#define BG_WRAPAROUND     (1 << 13)
#define BG_SIZE_SMALL     (0 << 14)
#define BG_SIZE_WIDE      (1 << 14)
#define BG_SIZE_TALL      (2 << 14)
#define BG_SIZE_LARGE     (3 << 14)

#define SPRITE_MODE_1D (1 << 6)

//extern OBJ_ATTR shadowOAM[128];

// Character blocks store tiles
typedef struct {
    u16 tileimg[8192];
} CB;
#define CHARBLOCK ((CB*) 0x6000000)

// Screenblocks store tilemaps
typedef struct {
    u16 tilemap[1024];
} SB;
#define SCREENBLOCK ((SB*) 0x6000000)

// Tilemap entry helpers
#define TILEMAP_ENTRY_TILEID(x) ((x) & 1023)
#define TILEMAP_ENTRY_HFLIP     (1 << 10)
#define TILEMAP_ENTRY_VFLIP     (1 << 11)
#define TILEMAP_ENTRY_PALROW(x) (((x) & 15) << 12)

// Initializes the display for tiled Mode 0 rendering
void initMode0(void);

// Generic background helpers
void setupBackground(int bg, unsigned short config);
void setBackgroundOffset(int bg, int hOff, int vOff);

// Tilemap / tileset helpers
void fillScreenblock(int screenblock, unsigned short entry);
void clearScreenblock(int screenblock);
void loadMapToScreenblock(int screenblock, const unsigned short* map, int count);
void loadTilesToCharblock(int charblock, const unsigned short* tiles, int count);
void loadBgPalette(const unsigned short* palette, int count);


void clearCharBlock(int charblock);
#endif