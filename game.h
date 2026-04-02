#ifndef GAME_H
#define GAME_H

#include "gba.h"
#include "mode0.h"
#include "sprites.h"
#include "font.h"
#include "levels.h"

// ======================================================
//                    BACKGROUND SETUP
// ======================================================

// HUD background
#define HUD_SCREENBLOCK     27
#define HUD_CHARBLOCK       0

// Main gameplay background
#define GAME_SCREENBLOCK    28
#define GAME_CHARBLOCK      1

// Optional cloud/background layer
#define CLOUD_SCREENBLOCK   30

// ======================================================
//                    GAME CONSTANTS
// ======================================================

#define ITEM_SIZE           8
#define BEE_SIZE            8

#define GRAVITY             1
#define JUMP_VEL           -8
#define MAX_FALL_SPEED      4
#define MOVE_SPEED          2
#define CLIMB_SPEED         1

// Useful clamp macro
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

// ======================================================
//                    OBJ TILE INDICES
// ======================================================

// These are other sprite tile indices in OBJ memory.
// Keep these only if your sprite-loading code still uses them.
#define OBJ_TILE_ITEM       8
#define OBJ_TILE_BEE0       9
#define OBJ_TILE_BEE1       10

// ======================================================
//                    PLAYER SPRITE SETUP
// ======================================================

// Player is now 2 tiles wide x 4 tiles tall.
// That means:
// width  = 2 * 8 = 16 px
// height = 4 * 8 = 32 px
//
// This CAN be drawn as one single GBA OBJ sprite using 16x32.
#define PLAYER_WIDTH        16
#define PLAYER_HEIGHT       32

// ------------------------------------------------------
// Player animation layout in OBJ memory
//
// Each player frame is 2 tiles wide x 4 tiles tall = 8 tiles total.
// We copy each frame into contiguous OBJ tile memory so 1D sprite mode
// can draw it correctly as one 16x32 sprite.
//
// Row 0 on sheet: right  animations
// Row 1 on sheet: left   animations
// Row 2 on sheet: up     animations (climbing up)
// Row 3 on sheet: down   animations (falling / climbing down)
//
// Sheet top-lefts you gave:
// right row starts at (0, 0)
// left  row starts at (0, 4)
// up    row starts at (0, 8)
// down  row starts at (0, 12)
//
// Each frame is 2 tiles wide, so frame starts are:
// frame 0 = x 0
// frame 1 = x 2
// frame 2 = x 4
// frame 3 = x 6
// ------------------------------------------------------
#define PLAYER_ANIM_FRAMES      4
#define PLAYER_TILES_PER_FRAME  8   // 2 x 4 tiles

#define OBJ_TILE_PLAYER_RIGHT   0
#define OBJ_TILE_PLAYER_LEFT    (OBJ_TILE_PLAYER_RIGHT + PLAYER_ANIM_FRAMES * PLAYER_TILES_PER_FRAME)
#define OBJ_TILE_PLAYER_UP      (OBJ_TILE_PLAYER_LEFT  + PLAYER_ANIM_FRAMES * PLAYER_TILES_PER_FRAME)
#define OBJ_TILE_PLAYER_DOWN    (OBJ_TILE_PLAYER_UP    + PLAYER_ANIM_FRAMES * PLAYER_TILES_PER_FRAME)

// Palette row used by the player sprite.
#define PLAYER_PALROW           0

// Direction values for animation selection
#define DIR_RIGHT  0
#define DIR_LEFT   1
#define DIR_UP     2
#define DIR_DOWN   3

// ======================================================
//                    COLLECTIBLE SPRITES
// ======================================================

// Bean sprout art on the sprite sheet starts at tile (0, 21)
// and is 2 tiles wide x 3 tiles tall = 16x24 pixels.
//
// The GBA does not support a native 16x24 OBJ size, so we draw it
// as two sprites:
//   - top piece    = 16x16
//   - bottom piece = 16x8
#define BEAN_SPROUT_WIDTH       16
#define BEAN_SPROUT_HEIGHT      24
#define BEAN_SPROUT_TILES       6

// sprites / collectibles use palette bank 1
#define WORLD_SPRITE_PALROW     1

// Put the bean sprout tiles immediately after the player animation tiles.
#define OBJ_TILE_BEAN_SPROUT    (OBJ_TILE_PLAYER_DOWN + PLAYER_ANIM_FRAMES * PLAYER_TILES_PER_FRAME)

// OAM indices used in drawSprites()
#define OBJ_INDEX_PLAYER        0
#define OBJ_INDEX_BEAN_TOP      1
#define OBJ_INDEX_BEAN_BOTTOM   2

// ======================================================
//                    INVENTORY FLAGS
// ======================================================

// These are bit flags so the inventory can hold multiple items later.
#define INVENTORY_BEAN_SPROUT   (1 << 0)
#define INVENTORY_BONEMEAL      (1 << 1)
#define INVENTORY_WATER         (1 << 2)

// ======================================================
//                    SPAWN POSITIONS
// ======================================================

// Default home spawn when starting fresh
#define HOME_SPAWN_X                        (4 * 8)

// ------------------------------------------------------
// LEVEL 1 TRANSITION SPAWNS
//
// There are now TWO Home <-> Level 1 transition routes:
//   1. bottom/original portal
//   2. top/platform portal
//
// For the Y positions, we use "preferred Y" values.
// The game will scan from that Y to find a safe standing spot.
// That is more reliable than hard-coding exact top-Y values.
// ------------------------------------------------------

// HOME -> LEVEL 1 (bottom/original portal)
#define LEVEL1_FROM_HOME_BOTTOM_SPAWN_X     (60 * 8)
#define LEVEL1_FROM_HOME_BOTTOM_PREF_Y      (LEVEL1_PIXEL_H - (8 * 8))

// HOME -> LEVEL 1 (top/platform portal)
// Adjust these if your top entry in level one should land somewhere else.
#define LEVEL1_FROM_HOME_TOP_SPAWN_X        (60 * 8)
#define LEVEL1_FROM_HOME_TOP_PREF_Y         (8 * 8)

// LEVEL 1 -> HOME (bottom/original return)
#define HOME_FROM_LEVEL1_BOTTOM_SPAWN_X     (6 * 8)
#define HOME_FROM_LEVEL1_BOTTOM_PREF_Y      (HOME_PIXEL_H - (8 * 8))

// LEVEL 1 -> HOME (top/platform return)
// Adjust these if your upper platform in home is at a different x/y region.
#define HOME_FROM_LEVEL1_TOP_SPAWN_X        (6 * 8)
#define HOME_FROM_LEVEL1_TOP_PREF_Y         (LEVEL1_PIXEL_H - (8 * 8))
// ------------------------------------------------------
// LEVEL 2 TRANSITION SPAWNS
// ------------------------------------------------------

#define LEVEL2_SPAWN_X                      (4 * 8)

// Return position in HOME when coming back from Level 2
#define HOME_FROM_LEVEL2_SPAWN_X            (29 * 8)
#define HOME_FROM_LEVEL2_SPAWN_Y            (22 * 8)
// ======================================================
//                    COLORS / PALETTE HELPERS
// ======================================================

// Teal background color you wanted.
// This is useful for the backdrop color / palette index 0.
#define SKY_COLOR           RGB(0, 23, 31)

// ======================================================
//                    GAME STATES
// ======================================================

typedef enum {
    STATE_START,
    STATE_INSTRUCTIONS,
    STATE_HOME,
    STATE_LEVEL1,
    STATE_LEVEL2,
    STATE_PAUSE,
    STATE_WIN,
    STATE_LOSE
} GameState;

// ======================================================
//                    STRUCTS
// ======================================================

typedef struct {
    int x;
    int y;
    int oldX;
    int oldY;
    int xVel;
    int yVel;
    int width;
    int height;
    int grounded;
    int climbing;

    // Animation state
    int direction;      // DIR_RIGHT / DIR_LEFT / DIR_UP / DIR_DOWN
    int animFrame;      // 0..3
    int animCounter;    // frame timing
    int isMoving;       // true while actively walking/climbing
} Player;

typedef struct {
    int x;
    int y;
    int active;
    int bob;
} ResourceItem;

typedef struct {
    int x;
    int y;
    int active;
    int animFrame;
    int animCounter;
    int dir;
} Bee;

// ======================================================
//                    FUNCTION PROTOTYPES
// ======================================================

void initGame(void);
void updateGame(void);
void drawGame(void);

#endif