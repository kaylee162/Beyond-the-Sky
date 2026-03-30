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

// Start tile for the player in OBJ tile memory.
// This assumes you copy the player's 2x4 tile art into OBJ tile slot 0.
#define OBJ_TILE_PLAYER     0

// Palette row used by the player sprite.
// Leave as 0 unless your sprite art is exported into a different OBJ palette row.
#define PLAYER_PALROW       0

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
    int facingLeft;
    int animFrame;
    int animCounter;
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