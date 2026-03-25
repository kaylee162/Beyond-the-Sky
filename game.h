#ifndef GAME_H
#define GAME_H

#include "gba.h"
#include "mode0.h"
#include "sprites.h"
#include "font.h"
#include "levels.h"

#define HUD_SCREENBLOCK     28
#define HUD_CHARBLOCK       0
#define GAME_SCREENBLOCK    24
#define GAME_CHARBLOCK      1
#define CLOUD_SCREENBLOCK   26

#define PLAYER_WIDTH        16
#define PLAYER_HEIGHT       16
#define ITEM_SIZE           8
#define BEE_SIZE            8

#define GRAVITY             1
#define JUMP_VEL           -8
#define MAX_FALL_SPEED      4
#define MOVE_SPEED          2
#define CLIMB_SPEED         1

#define OBJ_TILE_PLAYER0    0
#define OBJ_TILE_PLAYER1    4
#define OBJ_TILE_ITEM       8
#define OBJ_TILE_BEE0       9
#define OBJ_TILE_BEE1       10

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

typedef enum {
    STATE_START,
    STATE_INSTRUCTIONS,
    STATE_HOME,
    STATE_SKY,
    STATE_PAUSE,
    STATE_WIN,
    STATE_LOSE
} GameState;

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

void initGame(void);
void updateGame(void);
void drawGame(void);

#endif