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

// Cloud/background layer
#define CLOUD_SCREENBLOCK   30

// ======================================================
//                    GAME CONSTANTS
// ======================================================

#define ITEM_SIZE           8

#define BEE_WIDTH           16
#define BEE_HEIGHT          24
#define BEE_ANIM_FRAMES      3
#define BEE_TILES_PER_FRAME  6   
#define BEE_PATROL_SPEED     1

#define GRAVITY              1
#define JUMP_VEL            -12
#define MAX_FALL_SPEED       4
#define MOVE_SPEED           2
#define CLIMB_SPEED          1

#define TRANSPARENT_TILE_ID  4

// Useful clamp macro
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

// BG map entries store the 4bpp palette row in bits 12-15.
#define BG_TILE_PALROW(row)    (((row) & 0xF) << 12)

// Keep tile index + flip bits, replace only the palette row.
#define BG_TILE_ATTR_MASK      0x0FFF
// ======================================================
//                    OBJ TILE INDICES
// ======================================================

// These are other sprite tile indices in OBJ memory.
#define OBJ_TILE_ITEM       8
#define OBJ_TILE_BEE0       9
#define OBJ_TILE_BEE1       10

// ======================================================
//                    PLAYER SPRITE SETUP
// ======================================================

// Player is 2 tiles wide x 4 tiles tall.
// That means:
// width  = 2 * 8 = 16 px
// height = 4 * 8 = 32 px
//
// This can be drawn as one single GBA OBJ sprite using 16x32.
#define PLAYER_WIDTH        16
#define PLAYER_HEIGHT       32

// ------------------------------------------------------
// Player animation layout in OBJ memory
//
// Each player frame is 2 tiles wide x 4 tiles tall = 8 tiles total.
// We copy each frame into contiguous OBJ tile memory so 1D sprite mode
// can draw it correctly as one 16x32 sprite.
//
// Row 0 on sheet: right animations
// Row 1 on sheet: left animations
// Row 2 on sheet: up animations (climbing up)
// Row 3 on sheet: down animations (falling / climbing down)
//
// Sheet top-lefts (for reference):
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
#define PLAYER_TILES_PER_FRAME  8  

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
// The GBA does not support a native 16x24 OBJ size, so draw it
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
#define OBJ_INDEX_BONEMEAL      3
#define OBJ_INDEX_WATER         4
#define OBJ_INDEX_BEE0_TOP      5
#define OBJ_INDEX_BEE0_BOTTOM   6
#define OBJ_INDEX_BEE1_TOP      7
#define OBJ_INDEX_BEE1_BOTTOM   8
#define OBJ_INDEX_BEE2_TOP      9
#define OBJ_INDEX_BEE2_BOTTOM   10

// Bonemeal and water droplet are both 2 tiles wide x 2 tiles tall = 16x16.
#define RESOURCE_WIDTH           16
#define RESOURCE_HEIGHT          16
#define RESOURCE_ANIM_FRAMES      2
#define RESOURCE_TILES_PER_FRAME  4   

#define OBJ_TILE_BONEMEAL        (OBJ_TILE_BEAN_SPROUT + BEAN_SPROUT_TILES)
#define OBJ_TILE_WATER           (OBJ_TILE_BONEMEAL + RESOURCE_ANIM_FRAMES * RESOURCE_TILES_PER_FRAME)
#define OBJ_TILE_BEE             (OBJ_TILE_WATER + RESOURCE_ANIM_FRAMES * RESOURCE_TILES_PER_FRAME)

// ======================================================
//                    INVENTORY FLAGS
// ======================================================

// Bit flags so the inventory can hold multiple items
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
// There are TWO Home <-> Level 1 transition routes:
//   1. bottom/ground portal
//   2. top/platform portal
//
// For the Y positions, use the "preferred Y" values
// The game will scan from that Y to find a safe standing spot so things
// aren't floating or inside platforms
// ------------------------------------------------------

// HOME -> LEVEL 1 (bottom/ground portal)
#define LEVEL1_FROM_HOME_BOTTOM_SPAWN_X     (60 * 8)
#define LEVEL1_FROM_HOME_BOTTOM_PREF_Y      (LEVEL1_PIXEL_H - (8 * 8))

// HOME -> LEVEL 1 (top/platform portal)
#define LEVEL1_FROM_HOME_TOP_SPAWN_X        (60 * 8)
#define LEVEL1_FROM_HOME_TOP_PREF_Y         (8 * 8)

// LEVEL 1 -> HOME (bottom/ground return)
#define HOME_FROM_LEVEL1_BOTTOM_SPAWN_X     (6 * 8)
#define HOME_FROM_LEVEL1_BOTTOM_PREF_Y      (HOME_PIXEL_H - (8 * 8))

// LEVEL 1 -> HOME (top/platform return)
#define HOME_FROM_LEVEL1_TOP_SPAWN_X        (6 * 8)
#define HOME_FROM_LEVEL1_TOP_PREF_Y         (LEVEL1_PIXEL_H - (8 * 8))

// ------------------------------------------------------
// LEVEL 2 TRANSITION SPAWNS
// ------------------------------------------------------

#define LEVEL2_SPAWN_X                      (4 * 8)

// Return position in HOME when coming back from Level 2
#define HOME_FROM_LEVEL2_SPAWN_X            (29 * 8)
#define HOME_FROM_LEVEL2_SPAWN_Y            (22 * 8)

// ------------------------------------------------------
// LEVEL RESOURCE SPAWNS
// ------------------------------------------------------
// Level 1 bonemeal: near the left side of the level.
#define LEVEL1_BONEMEAL_SPAWN_X          (6 * 8)

// Level 2 water droplet: near the right side of the level.
#define LEVEL2_WATER_SPAWN_X             (LEVEL2_PIXEL_W - (6 * 8))

// Put the droplet about 2/3 of the way up the level.
#define LEVEL2_WATER_SPAWN_Y             (6 * 8) //8

// ------------------------------------------------------
// BEE ENEMY SPAWNS
// ------------------------------------------------------
#define LEVEL1_BEE_SPAWN_X               (27 * 8)
#define LEVEL1_BEE_SPAWN_Y               (18 * 8)

#define LEVEL2_BEE_A_SPAWN_X             (54 * 8)
#define LEVEL2_BEE_A_SPAWN_Y             (17 * 8)

#define LEVEL2_BEE_B_SPAWN_X             (40 * 8)
#define LEVEL2_BEE_B_SPAWN_Y             (10 * 8)

#define BEE_PATROL_RANGE                 (4 * 8)

// Small upward launch when the player reaches the top of a ladder / vine.
// This helps em "hop" onto the platform instead of getting stuck.
#define LADDER_EXIT_BOOST  -4

// ======================================================
//                    COLORS / PALETTE HELPERS
// ======================================================

// Sky colors used for the runtime day/night palette cycle.
// These all affect BG palette index 0 which was the backdrop color
#define SKY_COLOR_EARLY_MORNING   RGB(4, 18, 26)
#define SKY_COLOR_DAY             RGB(0, 23, 31)
#define SKY_COLOR_SUNSET          RGB(29, 18, 15)
#define SKY_COLOR_NIGHT           RGB(0, 7, 14)

// Keep SKY_COLOR as the default daytime value so any older code that still
// references SKY_COLOR will continue to work safely.
#define SKY_COLOR                 SKY_COLOR_DAY

// --------------------------------------------------
// Daytime cycle tuning
// --------------------------------------------------
// How many frames each transition segment lasts.
#define DAYTIME_CYCLE_SEGMENT_FRAMES     1200 //600

// starting offset into the full cycle so starting the game starts it at EARLY MORNING
#define DAYTIME_CYCLE_START_OFFSET_FRAMES   0

// Cute green background for menus / title / instructions / pause / win / lose
#define MENU_BG_COLOR   RGB(10, 25, 10) 

// ======================================================
//                 CLOUD ANIMATION SETTINGS
// ======================================================
// How slowly the cloud layer follows the camera.
// Higher number = slower parallax movement.
#define CLOUD_PARALLAX_DIVISOR           4 //6 

// How often the clouds auto-scroll by 1 pixel.
// Higher number = slower cloud drift.
#define CLOUD_SCROLL_DELAY               6 //4

// --------------------------------------------------
// HOME beanstalk / farmland layout
// --------------------------------------------------
// The soil patch sits at tile (22, 59) in the 32x64 home map.
// It is 8 tiles wide x 5 tiles tall.
#define FARMLAND_TILE_X                 21
#define FARMLAND_TILE_Y                 59
#define FARMLAND_WIDTH_TILES             8
#define FARMLAND_HEIGHT_TILES            5

// Tileset source rectangles for the farmland art.
// Stage 0 uses the empty farmland at tileset tile (0, 20).
// Stages 1-3 use the planted farmland at tileset tile (8, 20).
// Tileset source rectangles for the farmland art.
#define EMPTY_FARMLAND_SRC_TILE_X        0
#define PLANTED_FARMLAND_SRC_TILE_X      8
#define FARMLAND_SRC_TILE_Y             20
#define EMPTY_FARMLAND_SRC_W             8
#define EMPTY_FARMLAND_SRC_H             5

// The full home beanstalk already exists in the exported foreground map.
// We hide and reveal it by overwriting the affected tile rows in VRAM.
#define HOME_BEANSTALK_TILE_LEFT        21
#define HOME_BEANSTALK_TILE_RIGHT       28 
#define HOME_BEANSTALK_TILE_TOP          8
#define HOME_BEANSTALK_TILE_BOTTOM      58

// New beanstalk-base farmland art in the tileset.
// This replaces the older "seed" graphic after the sprout is deposited.
#define GROWN_FARMLAND_SRC_X            24
#define GROWN_FARMLAND_SRC_Y            30
#define GROWN_FARMLAND_SRC_W             8
#define GROWN_FARMLAND_SRC_H             2

// Align the 8x2 beanstalk-base farmland to the bottom of the 8x5 farmland area.
#define GROWN_FARMLAND_DEST_X        FARMLAND_TILE_X
#define GROWN_FARMLAND_DEST_Y        (FARMLAND_TILE_Y + (FARMLAND_HEIGHT_TILES - GROWN_FARMLAND_SRC_H))

// Partial-grown beanstalk should visually start here so it is about 40-ish tiles tall.
#define PARTIAL_BEANSTALK_VISIBLE_TOP_TILE   17

// Decorative top for the half-grown / 2/3-grown beanstalk.
// These tiles come from the tileset and are stamped on top of the partial stalk.
#define PARTIAL_TOP_SRC_X                    19
#define PARTIAL_TOP_SRC_Y                    21
#define PARTIAL_TOP_WIDTH                     5
#define PARTIAL_TOP_HEIGHT                    5

#define PARTIAL_TOP_DEST_X                   22
#define PARTIAL_TOP_DEST_Y                   17

// Default palette row used by normal home foreground tiles.
#define HOME_FOREGROUND_PALROW          1

// The castle in homebase uses its own palette row in the tileset.
// These tilemap coordinates are in map-space, not tileset-space.
#define HOME_CASTLE_PALROW              2
#define HOME_CASTLE_TILE_LEFT           5
#define HOME_CASTLE_TILE_RIGHT         14
#define HOME_CASTLE_TILE_TOP            1
#define HOME_CASTLE_TILE_BOTTOM         7
// ======================================================
//                    GAME STATE
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

    // Simple 2-frame animation state for floating collectibles.
    int animFrame;
    int animCounter;

    int bob;
} ResourceItem;

typedef struct {
    int x;
    int y;
    int active;
    int animFrame;
    int animCounter;
    int dir;

    // Horizontal patrol limits in world pixels.
    int minX;
    int maxX;
} Bee;

// ======================================================
//                    FUNCTION PROTOTYPES
// ======================================================

void initGame(void);
void updateGame(void);
void drawGame(void);

// Cheats
extern int invincibilityCheat;

#endif