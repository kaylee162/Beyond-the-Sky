#include "game_internal.h"

/* DEV NOTES FOR THIS FILE:
 *
 * This file manages all the game objects that are not part of the player's direct
 * movement code and are not part of the tilemap editing system. so it focuses on
 * world entities like the bees and the collectible resources, along with the helper
 * logic needed to initialize them, update them, detect interactions with them,
 * and expose their state to the HUD.
 *
 * More specifically, this file handles:
 * - bee enemy collision sampling and movement rules
 * - bee initialization for each level
 * - bee patrol and animation updates
 * - bee-to-player collision checks
 * - collectible placement and initialization
 * - collectible pickup behavior
 * - collectible animation timing
 * - inventory bitmask helpers
 * - debug resource-grant cheat logic
 * - HUD inventory string generation
 *
 * In the larger scope of the game:
 * - gameplay code updates the player each frame
 * - this file updates the moving enemies and collectible state around that player
 * - render code then uses the data from this file to draw bees and items
 */

// Checks whether a bee would overlap solid terrain at a given position. Bee movement uses this to stay inside valid patrol space
int beeBodyHitsSolidAt(int x, int y) {
    // Sample several points on the bee body so it obeys the collision map
    // instead of clipping into platforms or walls.
    int leftX   = x;
    int centerX = x + (BEE_WIDTH / 2);
    int rightX  = x + BEE_WIDTH - 1;

    int topY    = y;
    int middleY = y + (BEE_HEIGHT / 2);
    int bottomY = y + BEE_HEIGHT - 1;

    return isBlockedPixel(currentLevel, leftX, topY)
        || isBlockedPixel(currentLevel, leftX, middleY)
        || isBlockedPixel(currentLevel, leftX, bottomY)
        || isBlockedPixel(currentLevel, rightX, topY)
        || isBlockedPixel(currentLevel, rightX, middleY)
        || isBlockedPixel(currentLevel, rightX, bottomY)
        || isBlockedPixel(currentLevel, centerX, topY)
        || isBlockedPixel(currentLevel, centerX, bottomY);
}

// returns whether a bee can move to a new position without entering blocked terrain. It wraps the bee collision checks into one movement test
int canBeeMoveTo(int newX, int newY) {
    int rightX = newX + BEE_WIDTH - 1;
    int bottomY = newY + BEE_HEIGHT - 1;

    if (newX < 0 || rightX >= levelWidth || newY < 0 || bottomY >= levelHeight) {
        return 0;
    }

    return !beeBodyHitsSolidAt(newX, newY);
}

// Initializes one bee with its starting position, patrol range, and direction. This prepares the enemy for level-specific use
void initBee(Bee* bee, int x, int y, int patrolRange, int startDir) {
    bee->x = x;
    bee->y = y;
    bee->active = canBeeMoveTo(x, y);
    bee->animFrame = 0;
    bee->animCounter = 0;
    bee->dir = (startDir >= 0) ? 1 : -1;

    // Bee patrols in a small fixed horizontal band.
    bee->minX = x - patrolRange;
    bee->maxX = x + patrolRange;

    if (bee->minX < 0) {
        bee->minX = 0;
    }
    if (bee->maxX > levelWidth - BEE_WIDTH) {
        bee->maxX = levelWidth - BEE_WIDTH;
    }
}

// spawns the bees needed for the current level and resets unused slots. Each level can define its own enemy layout through this setup step
void initLevelBees(void) {
    beeCount = 0;

    if (currentLevel == LEVEL_ONE) {
        initBee(&bees[0], LEVEL1_BEE_SPAWN_X, LEVEL1_BEE_SPAWN_Y, BEE_PATROL_RANGE, -1);
        beeCount = 1;
    } else if (currentLevel == LEVEL_TWO) {
        initBee(&bees[0], LEVEL2_BEE_A_SPAWN_X, LEVEL2_BEE_A_SPAWN_Y, BEE_PATROL_RANGE, -1);
        initBee(&bees[1], LEVEL2_BEE_B_SPAWN_X, LEVEL2_BEE_B_SPAWN_Y, BEE_PATROL_RANGE, 1);
        beeCount = 2;
    }
}

// Advances all active bees by updating their patrol movement, facing, and animation. This keeps enemy behavior consistent across gameplay states
void updateBees(void) {
    for (int i = 0; i < beeCount; i++) {
        Bee* bee = &bees[i];
        int nextX;

        if (!bee->active) {
            continue;
        }

        // Animate wings
        bee->animCounter++;
        if (bee->animCounter >= 8) {
            bee->animCounter = 0;
            bee->animFrame = (bee->animFrame + 1) % BEE_ANIM_FRAMES;
        }

        // Horizontal patrol
        nextX = bee->x + bee->dir * BEE_PATROL_SPEED;

        // Reverse if the bee would leave its patrol zone or hit level collision.
        if (nextX < bee->minX || nextX > bee->maxX || !canBeeMoveTo(nextX, bee->y)) {
            bee->dir *= -1;
            nextX = bee->x + bee->dir * BEE_PATROL_SPEED;

            // If both directions are blocked, stay put.
            if (nextX < bee->minX || nextX > bee->maxX || !canBeeMoveTo(nextX, bee->y)) {
                continue;
            }
        }

        bee->x = nextX;
    }
}

// Checks whether the player overlaps any active bee. Gameplay uses this to trigger damage or loss behavior
int playerTouchesBee(void) {
    // Collision checks for all active bees.
    for (int i = 0; i < beeCount; i++) {
        if (bees[i].active && rectsOverlap(
                player.x, player.y, player.width, player.height,
                bees[i].x, bees[i].y, BEE_WIDTH, BEE_HEIGHT)) {
            return 1;
        }
    }

    return 0;
}

// ======================================================
//                COLLECTIBLES / INVENTORY
// ======================================================

// Places the bean sprout collectible in the home level and resets its state. This is the starting collectible that begins the beanstalk progression
void initBeanSprout(void) {
    // Place the bean sprout collectible in home and hide it if it was already
    // collected or already deposited into the farmland.
    int playerGroundY;

    // Put the bean sprout about 5 tiles in front of the player spawn.
    beanSprout.x = HOME_SPAWN_X + (5 * 8);

    // findStandingSpawnY() returns a TOP-Y for a player-sized body.
    // Convert that into a ground Y, then place the bean sprout so its feet
    // sit on that same ground.
    playerGroundY = findStandingSpawnY(beanSprout.x, levelHeight - (8 * 8)) + PLAYER_HEIGHT;
    beanSprout.y = playerGroundY - BEAN_SPROUT_HEIGHT;

    // The bean sprout should only exist before the first deposit stage.
    // Once the player has picked it up OR deposited it, do not let it respawn.
    beanSprout.active =
        ((inventoryFlags & INVENTORY_BEAN_SPROUT) == 0) &&
        (beanstalkGrowthStage == 0);

    beanSprout.bob = 0;
}

// Places the bonemeal collectible in level one and resets its state for a new run
void initBonemeal(void) {
    // Place bonemeal near the left side of Level 1, sitting on the ground.
    int playerGroundY;

    bonemeal.x = LEVEL1_BONEMEAL_SPAWN_X;

    // Use the same ground-placement idea as the bean sprout:
    // find the top Y where a player could stand, then convert that to ground Y.
    playerGroundY = findStandingSpawnY(bonemeal.x, levelHeight - (8 * 8)) + PLAYER_HEIGHT;
    bonemeal.y = playerGroundY - RESOURCE_HEIGHT;

    // Hide if already collected.
    bonemeal.active = ((inventoryFlags & INVENTORY_BONEMEAL) == 0);

    // Start animation on frame 0.
    bonemeal.animFrame = 0;
    bonemeal.animCounter = 0;
    bonemeal.bob = 0;
}

// places the water droplet collectible in level two and resets its state for a new run
void initWaterDroplet(void) {
    // Place the water droplet near the right side of Level 2.
    waterDroplet.x = LEVEL2_WATER_SPAWN_X;
    waterDroplet.y = LEVEL2_WATER_SPAWN_Y;

    // Hide if already collected.
    waterDroplet.active = ((inventoryFlags & INVENTORY_WATER) == 0);

    // Start animation on frame 0.
    waterDroplet.animFrame = 0;
    waterDroplet.animCounter = 0;
    waterDroplet.bob = 0;
}

// Returns whether two rectangles overlap. Several collision checks use this as a shared helper for simple bounding-box tests
int rectsOverlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    // Simple axis-aligned bounding box overlap test.
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

// checks for player pickups and updates collectible state when an item is collected. This is where world items move into the inventory system
void updateCollectibles(void) {
    // Handle item pickup logic for all active collectibles in the world.
    //
    // Pickups use simple AABB overlap checks because they are forgiving and
    // easier for the player to collect cleanly during movement.
    // Once picked up, the corresponding inventory bit is enabled and the world
    // object is hidden.

    // --------------------------------------------------
    // Bean sprout pickup in home
    // --------------------------------------------------
    if (currentLevel == LEVEL_HOME && beanSprout.active) {
        if (rectsOverlap(
                player.x, player.y, player.width, player.height,
                beanSprout.x, beanSprout.y, BEAN_SPROUT_WIDTH, BEAN_SPROUT_HEIGHT)) {

            // Mark bean sprout as collected and hide it.
            inventoryFlags |= INVENTORY_BEAN_SPROUT;
            beanSprout.active = 0;
        }
    }

    // --------------------------------------------------
    // Bonemeal pickup in Level 1
    // --------------------------------------------------
    if (currentLevel == LEVEL_ONE && bonemeal.active) {
        if (rectsOverlap(
                player.x, player.y, player.width, player.height,
                bonemeal.x, bonemeal.y, RESOURCE_WIDTH, RESOURCE_HEIGHT)) {

            // Mark bonemeal as collected and hide it.
            inventoryFlags |= INVENTORY_BONEMEAL;
            bonemeal.active = 0;
        }
    }

    // --------------------------------------------------
    // Water droplet pickup in Level 2
    // --------------------------------------------------
    if (currentLevel == LEVEL_TWO && waterDroplet.active) {
        if (rectsOverlap(
                player.x, player.y, player.width, player.height,
                waterDroplet.x, waterDroplet.y, RESOURCE_WIDTH, RESOURCE_HEIGHT)) {

            // Mark water as collected and hide it.
            inventoryFlags |= INVENTORY_WATER;
            waterDroplet.active = 0;
        }
    }
}

// advances the simple animation timers for active collectibles. This gives the pickup items some visual motion without changing their gameplay state
void updateCollectibleAnimations(void) {
    // Slowly toggle each active collectible between its animation frames.
    // A higher threshold means slower animation.
    if (currentLevel == LEVEL_ONE && bonemeal.active) {
        bonemeal.animCounter++;
        if (bonemeal.animCounter >= 20) {
            bonemeal.animCounter = 0;
            bonemeal.animFrame = (bonemeal.animFrame + 1) % RESOURCE_ANIM_FRAMES;
        }
    }

    if (currentLevel == LEVEL_TWO && waterDroplet.active) {
        waterDroplet.animCounter++;
        if (waterDroplet.animCounter >= 20) {
            waterDroplet.animCounter = 0;
            waterDroplet.animFrame = (waterDroplet.animFrame + 1) % RESOURCE_ANIM_FRAMES;
        }
    }
}

// Grants the next progression item needed for the beanstalk sequence. It is used by the cheat system to speed up testing
void giveNextResourceCheat(void) {
    // Bonemeal is the next needed item until it has either:
    // - been collected into inventory, or
    // - already been deposited into the beanstalk progression
    if ((beanstalkGrowthStage < 2) && ((inventoryFlags & INVENTORY_BONEMEAL) == 0)) {
        inventoryFlags |= INVENTORY_BONEMEAL;

        // If the bonemeal pickup is currently visible in Level 1,
        // remove it from the world so the cheat doesn't duplicate it.
        bonemeal.active = 0;
        return;
    }

    // Otherwise, water is the next needed item.
    // This can be added even if bonemeal is still sitting in inventory,
    // so both items can be held at the same time.
    if ((inventoryFlags & INVENTORY_WATER) == 0) {
        inventoryFlags |= INVENTORY_WATER;

        // Hide the world pickup if it is still active in Level 2.
        waterDroplet.active = 0;
    }
}

// Returns the HUD string that describes the player's current inventory.
// The text updates dynamically as progression items are collected or used.
const char* getInventoryText(void) {
    // Static buffer so the returned pointer remains valid after the function exits.
    static char buffer[64];

    // Start fresh each call.
    buffer[0] = '\0';

    // No items at all.
    if (inventoryFlags == 0) {
        strcpy(buffer, "EMPTY");
        return buffer;
    }

    // Bean sprout (home progression item).
    if (inventoryFlags & INVENTORY_BEAN_SPROUT) {
        strcat(buffer, "BEAN SPROUT");
    }

    // Bonemeal reward from level one.
    if (inventoryFlags & INVENTORY_BONEMEAL) {
        // Add separator only if something is already written.
        if (buffer[0] != '\0') {
            strcat(buffer, ", ");
        }

        strcat(buffer, "BONEMEAL");
    }

    // Water reward from level two.
    if (inventoryFlags & INVENTORY_WATER) {
        // Add separator only if another item is already listed.
        if (buffer[0] != '\0') {
            strcat(buffer, ", ");
        }

        strcat(buffer, "WATER");
    }

    return buffer;
}