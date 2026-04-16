#include "game_internal.h"

/* DEV NOTES FOR THIS FILE:
 * 
 * This file contains the world-query logic that the rest of the gameplay code
 * depends on when the game handles any kinda collision
 * so all the collision, traversal, spawn, hazard, and level-transition helpers :)
 *
 * More specifically, this file handles:
 * - multi-point player body collision checks
 * - movement legality checks against the collision map
 * - safe grounded spawn searching
 * - climb / ladder / beanstalk overlap checks
 * - ladder-top exit detection and rescue behavior
 * - grounded-state checks
 * - hazard and fall-out detection
 * - invincibility-fall recovery
 * - win-tile detection
 * - collision-map-driven level transitions
 *
 * In the larger scope of the game:
 * - gameplay movement code asks this file whether a move is legal
 * - state transition code relies on this file to detect map exits
 * - home beanstalk logic hooks into this file to dynamically gate climbing
 */

// Checks whether the player's body would overlap solid terrain at the given position. This is the main low-level collision test for movement
int bodyHitsSolidAt(int x, int y) {
    // Sample multiple points around the player's body instead of only the 4 corners.
    int leftX   = x;
    int centerX = x + (player.width / 2);
    int rightX  = x + player.width - 1;

    int topY        = y;
    int upperMidY   = y + (player.height / 3);
    int lowerMidY   = y + ((player.height * 2) / 3);
    int bottomY     = y + player.height - 1;

    // Check left edge
    if (isBlockedPixel(currentLevel, leftX, topY) ||
        isBlockedPixel(currentLevel, leftX, upperMidY) ||
        isBlockedPixel(currentLevel, leftX, lowerMidY) ||
        isBlockedPixel(currentLevel, leftX, bottomY)) {
        return 1;
    }

    // Check right edge
    if (isBlockedPixel(currentLevel, rightX, topY) ||
        isBlockedPixel(currentLevel, rightX, upperMidY) ||
        isBlockedPixel(currentLevel, rightX, lowerMidY) ||
        isBlockedPixel(currentLevel, rightX, bottomY)) {
        return 1;
    }

    // Check top edge
    if (isBlockedPixel(currentLevel, leftX, topY) ||
        isBlockedPixel(currentLevel, centerX, topY) ||
        isBlockedPixel(currentLevel, rightX, topY)) {
        return 1;
    }

    // Check bottom edge
    if (isBlockedPixel(currentLevel, leftX, bottomY) ||
        isBlockedPixel(currentLevel, centerX, bottomY) ||
        isBlockedPixel(currentLevel, rightX, bottomY)) {
        return 1;
    }

    return 0;
}

// Returns whether the player can move to a new position without entering blocked tiles. Movement code uses this before applying position changes
int canMoveTo(int newX, int newY) {
    // Test the player hitbox against blocked collision pixels using a
    // multi-point body check instead of only the 4 corners.
    int rightX  = newX + player.width - 1;
    int bottomY = newY + player.height - 1;

    if (newX < 0 || rightX >= levelWidth || newY < 0) {
        return 0;
    }

    // Fully below the level: allow movement so the player can properly fall out.
    if (newY >= levelHeight) {
        return 1;
    }

    // If the player is partially below the map, only test the part of the body
    // that is still inside the level.
    if (bottomY >= levelHeight) {
        int leftX   = newX;
        int centerX = newX + (player.width / 2);

        return !isBlockedPixel(currentLevel, leftX, newY)
            && !isBlockedPixel(currentLevel, centerX, newY)
            && !isBlockedPixel(currentLevel, rightX, newY);
    }

    return !bodyHitsSolidAt(newX, newY);
}

// Searches for a safe standing Y position near the requested spawn point. This helps transitions and respawns place the player back on solid ground
int findStandingSpawnY(int spawnX, int preferredTopY) {
    // Find the nearest open Y position where the player can stand on solid ground.
    int y;
    int clampedX;
    int clampedPreferredY;
    int leftFootX;
    int rightFootX;

    // Keep x inside the world
    clampedX = CLAMP(spawnX, 0, levelWidth - player.width);

    // Keep preferred y inside the world
    clampedPreferredY = CLAMP(preferredTopY, 0, levelHeight - player.height);

    leftFootX = clampedX + 2;
    rightFootX = clampedX + player.width - 3;

    // First search downward from the preferred position
    // until we find a valid open spot with solid ground under the feet.
    for (y = clampedPreferredY; y <= levelHeight - player.height; y++) {
        int centerFootX = clampedX + (player.width / 2);

        if (canMoveTo(clampedX, y) &&
            (y + player.height >= levelHeight ||
             isBlockedPixel(currentLevel, leftFootX, y + player.height) ||
             isBlockedPixel(currentLevel, centerFootX, y + player.height) ||
             isBlockedPixel(currentLevel, rightFootX, y + player.height))) {
            return y;
        }
    }

    // If that failed, search upward.
    for (y = clampedPreferredY - 1; y >= 0; y--) {
        int centerFootX = clampedX + (player.width / 2);

        if (canMoveTo(clampedX, y) &&
            (y + player.height >= levelHeight ||
             isBlockedPixel(currentLevel, leftFootX, y + player.height) ||
             isBlockedPixel(currentLevel, centerFootX, y + player.height) ||
             isBlockedPixel(currentLevel, rightFootX, y + player.height))) {
            return y;
        }
    }

    // Fallback: just clamp it.
    return clampedPreferredY;
}

// Returns whether the player is currently touching climbable terrain. This is the basic ladder and beanstalk detection check
int onClimbTile(void) {
    // Check climb overlap using the player's current position.
    return onClimbTileAt(player.x, player.y);
}

// Checks for climbable terrain at a specific position instead of the current player location. Helper routines use this for movement and ladder-edge logic
int onClimbTileAt(int x, int y) {
    // Check several points down the player body for ladder or vine collision.
    int centerX = x + (player.width / 2);

    // Sample multiple points vertically through the player's body.
    // This is much more reliable for ladders / beanstalks than checking
    // only one center point.
    int topY    = y + 2;
    int upperY  = y + 8;
    int midY    = y + (player.height / 2);
    int lowerY  = y + player.height - 8;
    int footY   = y + player.height - 2;

    return canUseCurrentClimbPixel(centerX, topY)
        || canUseCurrentClimbPixel(centerX, upperY)
        || canUseCurrentClimbPixel(centerX, midY)
        || canUseCurrentClimbPixel(centerX, lowerY)
        || canUseCurrentClimbPixel(centerX, footY);
}

// Detects whether the player has reached the top edge of a climbable area. This supports the ladder-top exit behavior
int isAtTopOfClimb(int x, int y) {
    int centerX = x + (player.width / 2);

    int headY  = y + 2;
    int chestY = y + 8;
    int hipY   = y + player.height - 10;
    int footY  = y + player.height - 2;

    int upperOnClimb = canUseCurrentClimbPixel(centerX, headY)
                    || canUseCurrentClimbPixel(centerX, chestY);

    int lowerOnClimb = canUseCurrentClimbPixel(centerX, hipY)
                    || canUseCurrentClimbPixel(centerX, footY);

    return lowerOnClimb && !upperOnClimb;
}

// Tries to nudge the player out of a climb state and onto the platform above when they reach the top. This is an important helper for preventing the player from getting stuck on ladders
int tryStartLadderTopExit(void) {
    // Pop the player upward off the ladder so they can step onto the platform above.
    int pop;

    // Scan upward. Try to find a spot where:
    //   (a) the position is walkable (no solid collision), AND
    //   (b) the player body no longer overlaps any climb tile.
    for (pop = 1; pop <= 10; pop++) {
        int testY = player.y - pop;

        if (testY < 0) {
            break;
        }

        if (canMoveTo(player.x, testY) && !onClimbTileAt(player.x, testY)) {
            // Found a clean exit position above the ladder top.
            player.y     = testY;
            player.climbing = 0;
            player.grounded = 0;
            player.yVel  = LADDER_EXIT_BOOST; // small upward arc (-4)
            return 1;
        }
    }

    // Fallback just in case: no clean gap found (tight ceiling, etc.).
    // Still force-exit climb mode so the player is never permanently stuck.
    player.climbing = 0;
    player.grounded = 0;
    player.yVel     = LADDER_EXIT_BOOST;
    return 1;
}

// returns whether the player currently has solid ground under their feet. Grounded movement and falling logic rely on this check
int isStandingOnSolid(void) {
    // Sample left, center, and right under the player's feet.
    // The center check is important for narrow platforms.
    int leftFootX   = player.x + 2;
    int centerFootX = player.x + (player.width / 2);
    int rightFootX  = player.x + player.width - 3;
    int belowY      = player.y + player.height;

    if (belowY >= levelHeight) {
        return 1;
    }

    return isBlockedPixel(currentLevel, leftFootX, belowY)
        || isBlockedPixel(currentLevel, centerFootX, belowY)
        || isBlockedPixel(currentLevel, rightFootX, belowY);
}

// Checks whether the player is touching a hazard tile. The gameplay loop uses this to trigger death handling
int touchesHazard(void) {
    // Sample the lower body against hazard tiles.
    int leftFootX  = player.x + 2;
    int rightFootX = player.x + player.width - 3;
    int footY      = player.y + player.height - 1;
    int midX       = player.x + (player.width / 2);
    int midY       = player.y + player.height - 4;

    return isHazardPixel(currentLevel, leftFootX, footY)
        || isHazardPixel(currentLevel, rightFootX, footY)
        || isHazardPixel(currentLevel, midX, midY);
}

// Returns whether the player has fallen past the bottom of the level. This acts as a backup fail condition when the player misses the map entirely
int fellOutOfLevel(void) {
    // Give the player a little extra room below the map before treating it as a true fall-out
    return player.y > levelHeight + 16;
}

// Resets the player to a safe spot after a fall while invincibility is active. This prevents endless hazard loops during testing
void recoverFromInvincibleFall(void) {
    // With invincibility enabled, falling out of the level should not kill
    // the player.
    player.x = CLAMP(respawnX, 0, levelWidth - player.width);
    player.y = CLAMP(respawnY, 0, levelHeight - player.height);

    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;
    player.isMoving = 0;
    player.animFrame = 0;
    player.animCounter = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    // Recenter the camera immediately after the rescue
    updateCamera();
}

// Checks whether the player is standing on the win tile. The gameplay loop uses this to trigger the win state
int touchesWinTile(void) {
    int leftFootX  = player.x + 2;
    int centerX    = player.x + (player.width / 2);
    int rightFootX = player.x + player.width - 3;
    int footY      = player.y + player.height - 1;

    return collisionAtPixel(currentLevel, leftFootX, footY) == COL_WIN
        || collisionAtPixel(currentLevel, centerX, footY) == COL_WIN
        || collisionAtPixel(currentLevel, rightFootX, footY) == COL_WIN;
}

// Checks for collision-map transition tiles and moves the player to the correct state or level when one is touched
void handleLevelTransitions(void) {
    // Check collision-map transition markers under the player and swap maps when needed.
    //
    // Transitions are data-driven through collision-map palette indices.
    int leftX   = player.x + 2;
    int centerX = player.x + (player.width / 2);
    int rightX  = player.x + player.width - 3;
    int probeY  = player.y + player.height - 1;

    // Read the exact collision-map palette index under each probe.
    u8 leftTile   = collisionAtPixel(currentLevel, leftX, probeY);
    u8 centerTile = collisionAtPixel(currentLevel, centerX, probeY);
    u8 rightTile  = collisionAtPixel(currentLevel, rightX, probeY);

    // --------------------------------------------------
    // HOME -> LEVEL ONE (bottom/original portal)
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME &&
        (leftTile   == COL_HOME_TO_LEVEL1_BOTTOM ||
         centerTile == COL_HOME_TO_LEVEL1_BOTTOM ||
         rightTile  == COL_HOME_TO_LEVEL1_BOTTOM)) {

        // Use custom spawn selection for the bottom Level 1 entry.
        respawnX = LEVEL1_FROM_HOME_BOTTOM_SPAWN_X;
        respawnY = -1;
        respawnPreferredY = LEVEL1_FROM_HOME_BOTTOM_PREF_Y;
        goToLevelOne(1);
        return;
    }

    // --------------------------------------------------
    // HOME -> LEVEL ONE (top/platform portal)
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME &&
        (leftTile   == COL_HOME_TO_LEVEL1_TOP ||
         centerTile == COL_HOME_TO_LEVEL1_TOP ||
         rightTile  == COL_HOME_TO_LEVEL1_TOP)) {

        // Use custom spawn selection for the upper Level 1 entry.
        respawnX = LEVEL1_FROM_HOME_TOP_SPAWN_X;
        respawnY = -1;
        respawnPreferredY = LEVEL1_FROM_HOME_TOP_PREF_Y;
        goToLevelOne(1);
        return;
    }

    // --------------------------------------------------
    // LEVEL ONE -> HOME (bottom/original return)
    // --------------------------------------------------
    if (currentLevel == LEVEL_ONE &&
        (leftTile   == COL_LEVEL1_TO_HOME_BOTTOM ||
         centerTile == COL_LEVEL1_TO_HOME_BOTTOM ||
         rightTile  == COL_LEVEL1_TO_HOME_BOTTOM)) {

        // Return to the lower/original home entry area.
        respawnX = HOME_FROM_LEVEL1_BOTTOM_SPAWN_X;
        respawnY = -1;
        respawnPreferredY = HOME_FROM_LEVEL1_BOTTOM_PREF_Y;
        goToHome(1);
        return;
    }

    // --------------------------------------------------
    // LEVEL ONE -> HOME (top/platform return)
    // --------------------------------------------------
    if (currentLevel == LEVEL_ONE &&
        (leftTile   == COL_LEVEL1_TO_HOME_TOP ||
         centerTile == COL_LEVEL1_TO_HOME_TOP ||
         rightTile  == COL_LEVEL1_TO_HOME_TOP)) {

        // Return to the upper platform area in home.
        respawnX = HOME_FROM_LEVEL1_TOP_SPAWN_X;
        respawnY = -1;
        respawnPreferredY = HOME_FROM_LEVEL1_TOP_PREF_Y;
        goToHome(1);
        return;
    }

    // --------------------------------------------------
    // HOME -> LEVEL TWO
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME &&
        (leftTile   == COL_HOME_TO_LEVEL2 ||
         centerTile == COL_HOME_TO_LEVEL2 ||
         rightTile  == COL_HOME_TO_LEVEL2)) {

        goToLevelTwo(0);
        return;
    }

    // --------------------------------------------------
    // LEVEL TWO -> HOME
    // --------------------------------------------------
    if (currentLevel == LEVEL_TWO &&
        (leftTile   == COL_LEVEL2_TO_HOME ||
         centerTile == COL_LEVEL2_TO_HOME ||
         rightTile  == COL_LEVEL2_TO_HOME)) {

        // This return already uses an exact, hand-tuned Y.
        respawnX = HOME_FROM_LEVEL2_SPAWN_X;
        respawnY = HOME_FROM_LEVEL2_SPAWN_Y - player.height;
        goToHome(1);
        return;
    }
}