#include "game_internal.h"

/* DEV NOTES FOR THIS FILE:
 *
 * This file is in charge of all the parts of the game that are unique to the
 * homebase map (which is mainly the farmland and beanstalk progression system).
 * Unlike the regular level maps (l1 & l2), the home map changes at runtime based on what
 * the player has collected and deposited. Because of that, this file acts as
 * the bridge between inventory progression and visible world changes.
 *
 * More specifically, this file handles:
 * - reading original tile entries from the home foreground map
 * - writing updated tile entries directly into the live home foreground map
 * - stamping rectangular tile regions from the tileset into the map
 * - rebuilding the farmland and beanstalk visuals based on growth stage
 * - checking whether the player is standing on the farmland
 * - handling B-button resource deposits in the correct progression order
 * - limiting which parts of the home beanstalk are actually climbable
 *
 * In the larger scope of the game:
 * - collectibles are picked up in the world and stored in inventory elsewhere
 * - this file uses those collected resources to visually and mechanically grow
 *   the beanstalk back in homebase
 */

// Returns the tileset entry for a home foreground tile at the given source coordinates. It packages the tile index the same way the home map expects it in VRAM
unsigned short getHomeForegroundSourceEntry(int tileX, int tileY) {
    if (tileX < 0 || tileX >= HOME_MAP_W || tileY < 0 || tileY >= HOME_MAP_H) {
        return 0;
    }

    // Home is a 32x64 tall map:
    // rows 0-31 live in screenblock A and rows 32-63 live in screenblock B.
    if (tileY < 32) {
        return homebase_foregroundMap[tileY * 32 + tileX];
    }

    return homebase_foregroundMap[1024 + (tileY - 32) * 32 + tileX];
}

// Writes a single tile entry into the home foreground map. This is the low-level helper used by the beanstalk tile modification logic
void setHomeForegroundTileEntry(int tileX, int tileY, unsigned short entry) {
    volatile unsigned short* targetMap;
    int localY;

    if (tileX < 0 || tileX >= HOME_MAP_W || tileY < 0 || tileY >= HOME_MAP_H) {
        return;
    }

    if (tileY < 32) {
        targetMap = SCREENBLOCK[BG_FRONT_SB_A].tilemap;
        localY = tileY;
    } else {
        targetMap = SCREENBLOCK[BG_FRONT_SB_B].tilemap;
        localY = tileY - 32;
    }

    targetMap[localY * 32 + tileX] = entry;
}

// Draws a rectangular block of tiles into the home foreground map using tiles
// from the shared tileset. Runtime-written home tiles use the standard home
// foreground palette row, and special cases like the castle are patched after.
void drawHomeTileBlockFromTileset(int mapTileX, int mapTileY, int srcTileX, int srcTileY, int widthTiles, int heightTiles) {
    int row;
    int col;

    for (row = 0; row < heightTiles; row++) {
        for (col = 0; col < widthTiles; col++) {
            unsigned short tileId = (unsigned short)((srcTileY + row) * 32 + (srcTileX + col));
            unsigned short entry = tileId | BG_TILE_PALROW(HOME_FOREGROUND_PALROW);
            setHomeForegroundTileEntry(mapTileX + col, mapTileY + row, entry);
        }
    }
}

static void applyHomeCastlePaletteRow(void) {
    int row;
    int col;

    for (row = HOME_CASTLE_TILE_TOP; row <= HOME_CASTLE_TILE_BOTTOM; row++) {
        for (col = HOME_CASTLE_TILE_LEFT; col <= HOME_CASTLE_TILE_RIGHT; col++) {
            volatile unsigned short* targetMap;
            int localY;
            unsigned short entry;

            if (row < 32) {
                targetMap = SCREENBLOCK[BG_FRONT_SB_A].tilemap;
                localY = row;
            } else {
                targetMap = SCREENBLOCK[BG_FRONT_SB_B].tilemap;
                localY = row - 32;
            }

            // Read what is actually on screen right now.
            entry = targetMap[localY * 32 + col];

            // Keep tile ID + flip bits, replace only palette row bits.
            entry = (entry & BG_TILE_ATTR_MASK) | BG_TILE_PALROW(HOME_CASTLE_PALROW);

            targetMap[localY * 32 + col] = entry;
        }
    }
}

// Rebuilds the home beanstalk area based on the current growth stage. This is the main runtime tile modification routine for the home map
void refreshHomeBeanstalkVisuals(void) {
    // Rebuild the visible home beanstalk and farmland art from the current growth stage.
    //
    // This function is the core of the growth system. It does two different jobs:
    // 1) it changes what the player sees by modifying the live tilemap
    // 2) together with canUseCurrentClimbPixel(), it keeps the visual state and
    //    usable climb region in sync
    //
    // Growth breakdown:
    // - Stage 0: regular farmland only
    // - Stage 1: planted farmland after bean sprout is deposited
    // - Stage 2: partial growth after bonemeal, including only the lower stalk
    //   and a decorative half-grown top so it looks intentional
    // - Stage 3: full beanstalk after water, with the original tall stalk restored
    int stage;
    int row;
    int col;

    // These edits only apply to the home foreground map.
    if (currentLevel != LEVEL_HOME) {
        return;
    }

    // Use the real deposited beanstalk growth stage.
    stage = beanstalkGrowthStage;

    // --------------------------------------------------
    // Hide or reveal the tall beanstalk region.
    //
    // The original home foreground already contains the full stalk art, but
    // the game does not want all of it visible immediately. So this loop either
    // restores original tiles or replaces them with a transparent tile,
    // depending on the current growth stage.
    //
    // Stage 0 = no tall stalk
    // Stage 1 = no tall stalk yet, only seed farmland
    // Stage 2 = partial stalk visible
    // Stage 3 = full stalk visible
    // --------------------------------------------------
    for (row = HOME_BEANSTALK_TILE_TOP; row <= HOME_BEANSTALK_TILE_BOTTOM; row++) {
        for (col = HOME_BEANSTALK_TILE_LEFT; col <= HOME_BEANSTALK_TILE_RIGHT; col++) {
            if (stage >= 3) {
                // Full growth: restore the original home foreground beanstalk.
                setHomeForegroundTileEntry(col, row, getHomeForegroundSourceEntry(col, row));
            } else if (stage == 2 && row >= PARTIAL_BEANSTALK_VISIBLE_TOP_TILE) {
                // Partial growth: only reveal the lower visible section.
                setHomeForegroundTileEntry(col, row, getHomeForegroundSourceEntry(col, row));
            } else {
                // Hidden rows become blank / transparent.
                setHomeForegroundTileEntry(col, row, TRANSPARENT_TILE_ID);
            }
        }
    }

    // --------------------------------------------------
    // Farmland states
    //
    // The farmland is updated separately from the tall stalk because it has its
    // own progression art and needs to remain visually grounded the entire time.
    //
    // Stage 0:
    //   regular farmland (0, 20), 8x5
    //
    // Stage 1:
    //   seed farmland (8, 20), 8x5
    //
    // Stage 2/3:
    //   start from seed farmland (8, 20), 8x5
    //   then overlay only the TOP 2 rows with the beanstalk base tiles
    //
    // That top-only overlay is important because the farmland itself is 5 tiles
    // tall, but the beanstalk base art only replaces the upper portion.
    // --------------------------------------------------
    if (stage == 0) {
        // Regular farmland before anything is planted.
        drawHomeTileBlockFromTileset(
            FARMLAND_TILE_X,
            FARMLAND_TILE_Y,
            EMPTY_FARMLAND_SRC_TILE_X,
            FARMLAND_SRC_TILE_Y,
            EMPTY_FARMLAND_SRC_W,
            EMPTY_FARMLAND_SRC_H
        );
    } else {
        // Stage 1/2/3 always begin with the full 8x5 seed farmland.
        drawHomeTileBlockFromTileset(
            FARMLAND_TILE_X,
            FARMLAND_TILE_Y,
            PLANTED_FARMLAND_SRC_TILE_X,
            FARMLAND_SRC_TILE_Y,
            EMPTY_FARMLAND_SRC_W,
            EMPTY_FARMLAND_SRC_H
        );

        // Once bonemeal is deposited, add the beanstalk/farmland transition.
        // These source tiles are only 8x2, so only stamp the TOP 2 rows
        // of the 8x5 farmland patch.
        if (stage >= 2) {
            drawHomeTileBlockFromTileset(
                FARMLAND_TILE_X,
                FARMLAND_TILE_Y,
                GROWN_FARMLAND_SRC_X,
                GROWN_FARMLAND_SRC_Y,
                GROWN_FARMLAND_SRC_W,
                GROWN_FARMLAND_SRC_H
            );
        }
    }

    // --------------------------------------------------
    // Stage 2 only:
    //
    // The 2/3 grown state is intentionally a hybrid. The lower stalk is real
    // climbable beanstalk art, but the upper tip is drawn separately as a
    // decorative cap so the plant does not look like it was cut off flat.
    //
    // This is purely a visual polish step, but it makes the intermediate state
    // read much more clearly to the player.
    // --------------------------------------------------
    if (stage == 2) {
        drawHomeTileBlockFromTileset(
            PARTIAL_TOP_DEST_X,
            PARTIAL_TOP_DEST_Y,
            PARTIAL_TOP_SRC_X,
            PARTIAL_TOP_SRC_Y,
            PARTIAL_TOP_WIDTH,
            PARTIAL_TOP_HEIGHT
        );
    }

    // Reapply the castle's special palette row after rebuilding home visuals.
    // The rest of the map stays on the normal home foreground palette row.
    applyHomeCastlePaletteRow();
}


// Checks whether the player is standing in the farmland interaction area. Deposit logic uses this to decide when resource placement is allowed
int isPlayerAtFarmland(void) {
    return currentLevel == LEVEL_HOME
        && rectsOverlap(
            player.x, player.y, player.width, player.height,
            FARMLAND_TILE_X * 8, FARMLAND_TILE_Y * 8,
            FARMLAND_WIDTH_TILES * 8, FARMLAND_HEIGHT_TILES * 8
        );
}

// Tries to deposit the next required resource into the farmland if the player is in the correct spot. Successful deposits advance the beanstalk and refresh the home visuals
void tryDepositResource(void) {
    // Handle B-button deposits at the farmland.
    //
    // Inventory and growth are separate on purpose. Picking something up only
    // sets an inventory bit. The world does not change until the player comes
    // back home, stands on the farmland, and presses B. That makes the loop
    // clear: explore -> collect -> return home -> deposit -> grow.
    if (currentLevel != LEVEL_HOME) {
        return;
    }

    // Require a clean B press, and ignore this action while SELECT is held so
    // cheat combinations do not accidentally trigger deposits.
    if (!BUTTON_PRESSED(BUTTON_B) || BUTTON_HELD(BUTTON_SELECT)) {
        return;
    }

    // Deposits only work while the player is actually standing at the farmland.
    if (!isPlayerAtFarmland()) {
        return;
    }

    // Nothing happens if the inventory is empty or the player has the wrong item.
    //
    // This strict ordering is important because the beanstalk system is meant
    // to feel like a progression chain, not a free-form crafting system.
    if (beanstalkGrowthStage == 0) {
        if (inventoryFlags & INVENTORY_BEAN_SPROUT) {
            inventoryFlags &= ~INVENTORY_BEAN_SPROUT;
            beanstalkGrowthStage = 1;
            refreshHomeBeanstalkVisuals();
        }
    } else if (beanstalkGrowthStage == 1) {
        if (inventoryFlags & INVENTORY_BONEMEAL) {
            inventoryFlags &= ~INVENTORY_BONEMEAL;
            beanstalkGrowthStage = 2;
            refreshHomeBeanstalkVisuals();
        }
    } else if (beanstalkGrowthStage == 2) {
        if (inventoryFlags & INVENTORY_WATER) {
            inventoryFlags &= ~INVENTORY_WATER;
            beanstalkGrowthStage = 3;
            refreshHomeBeanstalkVisuals();
        }
    }
}

// Checks whether a given pixel already counts as climbable on the home map. It helps the game preserve correct climbing behavior while the beanstalk visuals change
int canUseCurrentClimbPixel(int x, int y) {
    // Dynamic climb gate for the home beanstalk.
    //
    // The home collision map already contains the full beanstalk path, but the
    // player is not allowed to use all of it immediately. This helper acts as
    // a runtime gate: the collision art is there, but only the correct portion
    // is treated as climbable depending on the growth stage.
    int stage;

    // If the map itself does not mark this pixel as climbable, reject it first.
    if (!isClimbPixel(currentLevel, x, y)) {
        return 0;
    }

    // Outside of home, regular climbable behavior applies with no growth gating.
    if (currentLevel != LEVEL_HOME) {
        return 1;
    }

    // Use the real deposited beanstalk growth stage.
    stage = beanstalkGrowthStage;

    // Before bonemeal, none of the tall stalk should be climbable yet.
    if (stage <= 1) {
        return 0;
    }

    // Bonemeal reveals only the lower portion of the stalk.
    if (stage == 2) {
        return y >= (PARTIAL_BEANSTALK_VISIBLE_TOP_TILE * 8);
    }

    // Water completes the whole stalk.
    return 1;
}