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
//                    SPAWN POSITIONS
// ======================================================

// Default home spawn when starting fresh
#define HOME_SPAWN_X                (4 * 8)

// Home return positions for transitions
#define HOME_FROM_LEVEL1_SPAWN_X    (6 * 8)   // adjust if needed
#define HOME_FROM_LEVEL2_SPAWN_X    (4 * 8)  // place near Home -> Level 2 doorway

// Level entry positions
#define LEVEL1_SPAWN_X              (60 * 8)   // moved left a bit
#define LEVEL2_SPAWN_X              (4 * 8)  // moved right a bit

// Return position in HOME when coming back from Level 2
#define HOME_FROM_LEVEL2_SPAWN_X    (29 * 8)
#define HOME_FROM_LEVEL2_SPAWN_Y    (22 * 8)
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