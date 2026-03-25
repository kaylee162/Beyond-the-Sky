#ifndef GBA_SPRITES_H
#define GBA_SPRITES_H

#include "gba.h"

// Sprite Palette
#define SPRITE_PAL ((u16 *)0x5000200)

// Object Attribute Struct
typedef struct {
  u16 attr0;
  u16 attr1;
  u16 attr2;
  u16 fill;
} OBJ_ATTR;

// Object Attribute Memory
#define OAM ((OBJ_ATTR*)(0x7000000))
extern OBJ_ATTR shadowOAM[128];

struct attr0 {
  u16 regular;       // Normal Rendering (default)
  u16 affine;        // Affine Rendering
  u16 hide;          // Hidden
  u16 double_affine; // Double Affine Rendering
  u16 enable_alpha;  // Enable Alpha Blending
  u16 enable_window; // Object Window Mode (see GBATek)
  u16 enable_mosaic; // Enable Mosaic Effect
  u16 fourBpp;       // 4 Bits Per Pixel (default)
  u16 eightBpp;      // 8 Bits Per Pixel
  u16 square;        // Square Shape (default)
  u16 wide;          // Wide Shape
  u16 tall;          // Tall Shape
};

struct attr1 {
  u16 hflip;  // Flip horizontally
  u16 vflip;  // Flip Vertically
  u16 tiny;   // See Size Chart
  u16 small;  // See Size Chart
  u16 medium; // See Size Chart
  u16 large;  // See Size Chart
};

struct oam_attrs {
  struct attr0 attr0;
  struct attr1 attr1;
};

// OAM Attributes
// Attribute 0
#define ATTR0_Y(y)         ((y) & 0xFF) // Bits 0-7: Y coordinate
#define ATTR0_REGULAR      (0<<8)       // Bits 8-9: Object mode, regular rendering (default)
#define ATTR0_AFFINE       (1<<8)       // Bits 8-9: Object mode, affine rendering
#define ATTR0_HIDE         (2<<8)       // Bits 8-9: Object mode, no rendering (Hidden)
#define ATTR0_DOUBLEAFFINE (3<<8)       // Bits 8-9: Object mode, double affine rendering
#define ATTR0_NOBLEND      (0<<10)      // Bits A-B: GFX, disable alpha blending (default)
#define ATTR0_BLEND        (1<<10)      // Bits A-B: GFX, enable alpha blending
#define ATTR0_WINDOW       (2<<10)      // Bits A-B: GFX, enable object window mode
#define ATTR0_MOSAIC       (1<<12)      // Bit C: Mosaic, enable
#define ATTR0_4BPP         (0<<13)      // Bit D: BPP, 4 (default)
#define ATTR0_8BPP         (1<<13)      // Bit D: BPP, 8
#define ATTR0_SQUARE       (0<<14)      // Bits E-F: Shape, square (default)
#define ATTR0_WIDE         (1<<14)      // Bits E-F: Shape, wide
#define ATTR0_TALL         (2<<14)      // Bits E-F: Shape, tall

// Attribute 1   
#define ATTR1_X(x)    ((x) & 0x1FF) // Bits 0-8: X coordinate
#define ATTR1_HFLIP   (1<<12)       // Bits C-D: Flip, enable horizontal
#define ATTR1_VFLIP   (1<<13)       // Bits C-D: Flip, enable vertical
#define ATTR1_TINY    (0<<14)       // Bits E-F: Size, tiny (default) 
#define ATTR1_SMALL   (1<<14)       // Bits E-F: Size, small
#define ATTR1_MEDIUM  (2<<14)       // Bits E-F: Size, medium
#define ATTR1_LARGE   (3<<14)       // Bits E-F: Size, large

// Attribute 2
#define ATTR2_TILEID(id)   ((id) & 0x3FF)                 // Bits 0-9: Tile ID (flat 1D tile index)
#define ATTR2_PRIORITY(num)    (((num) & 3) << 10) // Bits A-B: Priority
#define ATTR2_PALROW(y)      (((y) & 0xF) <<12)    // Bits C-F: Palette row (4bpp)

// ----------- Sprite Size Chart --------------
// --------------------------------------------
//        |  TINY  | SMALL | MEDIUM | LARGE  |
// --------------------------------------------
// SQUARE |  8x8   | 16x16 | 32x32  | 64x64  |
// --------------------------------------------
//  WIDE  |  16x8  | 32x8  | 32x16  | 64x32  |
// --------------------------------------------
//  TALL  |  8x16  | 8x32  | 16x32  | 32x64  |
// --------------------------------------------

// Sprite Functions
void hideSprites();

// Generic struct for animated sprite
typedef struct {
    int x;
    int y;
    int xVel;
    int yVel;
    int width;
    int height;
    int timeUntilNextFrame;
    int direction;
    int isAnimating;
    int currentFrame;
    int numFrames;
    u8 oamIndex; 
} SPRITE;

#endif