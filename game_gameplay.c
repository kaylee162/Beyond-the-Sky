#include "game_internal.h"

/* DEV NOTES FOR THIS FILE:
 * 
 * This file contains the core frame-by-frame gameplay systems that run while
 * the player is inside a playable level. its job is to coordinate movement,
 * camera behavior, background scrolling, and the high-level gameplay checks
 * that need to happen every frame.
 *
 * More specifically, this file handles:
 * - the shared gameplay loop used by all playable states
 * - player movement input and movement resolution
 * - jump, gravity, and climb behavior
 * - the ladder / beanstalk top-exit anti-stuck logic
 * - player animation timing
 * - enemy update calls
 * - item pickup and resource deposit checks
 * - level transitions
 * - camera tracking and scrolling
 * - parallax cloud movement
 * - win and lose condition checks
 *
 * In the larger scope of the game:
 * - state-management code decides which screen or mode the game is in
 * - collision/world helper code answers whether movement is allowed
 * - rendering code draws the world after these gameplay systems update state
 * - this file is the main "live gameplay" controller that connects all of that :)
 */

// Runs the shared gameplay loop used by the home level and both platforming levels. It handles pause input, movement, pickups, animation, transitions, camera updates, and win or death checks
void updateGameplayCommon(void) {
    // Handle the shared gameplay loop used by all playable levels.
    //
    // This function is the top-level coordinator for one gameplay frame.
    // It does not perform every low-level task itself, but it calls the major
    // gameplay systems in the order that keeps movement, collisions, pickups,
    // transitions, and win/lose checks consistent.

    // START pauses the game immediately before the rest of the frame continues.
    if (BUTTON_PRESSED(BUTTON_START)) {
        goToPause();
        return;
    }

    // SELECT + A gives the next resource needed for progression.
    //
    // Order:
    //   1) bonemeal
    //   2) water droplet
    //
    // If bonemeal has already been obtained or deposited, this gives water.
    // Both resources can exist in the inventory at the same time.
    //
    // In the larger scope of the game, this is a debug shortcut that lets you
    // test the beanstalk progression loop without replaying the full levels.
    if (BUTTON_HELD(BUTTON_SELECT) && BUTTON_PRESSED(BUTTON_A)) {
        giveNextResourceCheat();
    }

    // Toggle cheat mode on/off with SELECT + B.
    //
    // This is a latched toggle, so the player only has to press it once rather
    // than hold the buttons continuously.
    if (BUTTON_HELD(BUTTON_SELECT) && BUTTON_PRESSED(BUTTON_B)) {
        cheatModeEnabled = !cheatModeEnabled;

        // Sync individual cheats to the master cheat mode toggle so hazard and
        // death behavior update immediately.
        invincibilityCheat = cheatModeEnabled;
    }

    // Move the player first so the rest of the frame uses the newest position.
    updatePlayerMovement();

    // Update enemy movement after the player moves.
    updateBees();

    // check death as soon as movement/enemy updates are done
    // this keeps the damanage sound tightly synced to actual hit frame
    if (invincibilityCheat) {
        if (fellOutOfLevel()) {
            recoverFromInvincibleFall();
            return;
        } 
    } else if (touchesHazard() || fellOutOfLevel() || playerTouchesBee()) {
        // damanage sound effect on channel b, triggered before nay state/menu work
        playSoundB(ouch_data, ouch_length, ouch_sampleRate, 0);
        goToLose(currentLevel);
        return;
    }

    // check item pickups after movement so a new overlap this frame counts
    updateCollectibles();

    // allow b to deposit the next required resource while standing on the farmland
    tryDepositResource();

    // Animate any active world collectibles.
    updateCollectibleAnimations();

    // Update the player animation after movement is fully resolved.
    updatePlayerAnimation();

    // Check for transition tiles after movement.
    handleLevelTransitions();

    // Update camera after movement and possible transitions.
    updateCamera();

    // Win check happens after movement and transitions have settled.
    if (touchesWinTile()) {
        goToWin();
        return;
    }
}

// Chooses the correct animation row and frame for the player based on movement and state. This keeps the sprite visuals matched to the latest gameplay logic
void updatePlayerAnimation(void) {
    // Advance or reset the player animation based on actual movement this frame.
    //
    // When the player is not actively walking or climbing, return to the idle
    // frame so the sprite does not keep cycling unnecessarily.
    if (!player.isMoving) {
        player.animFrame = 0;
        player.animCounter = 0;
        return;
    }

    // Use a slower delay for walking, but keep climbing at the faster speed.
    player.animCounter++;

    int animDelay = player.climbing ? 6 : 12;

    if (player.animCounter >= animDelay) {
        player.animCounter = 0;
        player.animFrame = (player.animFrame + 1) % PLAYER_ANIM_FRAMES;
    }
}

// Updates the player position for one frame using input, climbing rules, gravity, jumping, and collision checks. This is one of the core gameplay functions because it ties together most movement systems
void updatePlayerMovement(void) {
    // Resolve player input, climbing, jumping, gravity, and collision one pixel at a time.
    //
    // Movement is not applied in one big jump. Horizontal and vertical motion
    // are both resolved one pixel at a time, which makes collision much more
    // reliable around tile edges, short platforms, ladders, and ledges.
    int dx = 0;
    int step;
    int remaining;
    int wantsClimbUp;
    int wantsClimbDown;
    int wasActivelyMoving = 0;
    int onClimbNow;

    // Save the previous position so animation and movement comparison logic can
    // tell what actually changed this frame.
    player.oldX = player.x;
    player.oldY = player.y;

    // Check whether the player starts this frame standing on solid ground.
    player.grounded = isStandingOnSolid();

    // Read horizontal movement input.
    if (BUTTON_HELD(BUTTON_LEFT)) {
        dx -= MOVE_SPEED;
        player.direction = DIR_LEFT;
    }

    if (BUTTON_HELD(BUTTON_RIGHT)) {
        dx += MOVE_SPEED;
        player.direction = DIR_RIGHT;
    }

    // Read climb intent separately from jump behavior.
    wantsClimbUp = BUTTON_HELD(BUTTON_UP);
    wantsClimbDown = BUTTON_HELD(BUTTON_DOWN);
    onClimbNow = onClimbTile();

    // Climbing logic
    //
    // The player enters climb mode only when their body overlaps a climb tile
    // and the player is actively pressing up or down. This avoids "sticky"
    // ladder behavior where merely touching a ladder would steal control away
    // from normal platforming.
    if (onClimbNow && (wantsClimbUp || wantsClimbDown)) {
        player.climbing = 1;
        player.yVel = 0;
        player.grounded = 0;
    } else if (!onClimbNow) {
        // If the player is no longer overlapping climbable space, leave climb mode.
        player.climbing = 0;
    }

    // Jump logic
    //
    // Normal jump only happens when the player is not climbing and is grounded.
    // Ladder-top exit is handled separately below because it is a special anti-
    // stuck case rather than a normal jump.
    if (!player.climbing && BUTTON_PRESSED(BUTTON_UP) && player.grounded) {
        player.yVel = JUMP_VEL;
        player.grounded = 0;
    }

    // Gravity / falling
    //
    // While not climbing:
    // - upward velocity gradually slows due to gravity
    // - downward velocity increases until capped
    // - grounded players stay at zero vertical velocity
    if (!player.climbing) {
        if (!player.grounded || player.yVel < 0) {
            player.yVel += GRAVITY;
            if (player.yVel > MAX_FALL_SPEED) {
                player.yVel = MAX_FALL_SPEED;
            }
        } else {
            player.yVel = 0;
        }
    }

    // Horizontal movement resolution, 1 pixel at a time.
    //
    // This prevents the player from skipping through thin barriers or clipping
    // into walls when moving multiple pixels in a single frame.
    if (dx != 0) {
        step = (dx > 0) ? 1 : -1;
        remaining = (dx > 0) ? dx : -dx;

        while (remaining > 0) {
            if (canMoveTo(player.x + step, player.y)) {
                player.x += step;
            } else {
                break;
            }
            remaining--;
        }
    }

    // Vertical movement while climbing.
    if (player.climbing) {
        int climbDy = 0;

        if (wantsClimbUp) {
            climbDy -= CLIMB_SPEED;
        }
        if (wantsClimbDown) {
            climbDy += CLIMB_SPEED;
        }

        if (climbDy != 0) {
            step = (climbDy > 0) ? 1 : -1;
            remaining = (climbDy > 0) ? climbDy : -climbDy;

            while (remaining > 0) {
                int nextY = player.y + step;

                // Normal climb movement:
                // keep moving only if the next position is open and still
                // overlaps climb tiles.
                if (canMoveTo(player.x, nextY) &&
                    onClimbTileAt(player.x, nextY)) {
                    player.y = nextY;
                }
                // Special case: climbing upward at the top of the ladder / vine.
                // Leave climb mode and give the player a small upward launch.
                //
                // This is what lets the player get on top of the ladder instead
                // of getting trapped. Once the code detects that only the lower
                // body is still overlapping climb pixels, it pops the player up
                // to a clean non-climb position and gives a small upward boost
                // so the sprite lands naturally on the platform above.
                else if (step < 0 && wantsClimbUp && isAtTopOfClimb(player.x, player.y)) {
                    tryStartLadderTopExit();
                    wasActivelyMoving = 1;
                    break;
                } else {
                    break;
                }

                remaining--;
            }
        }

        // If the body no longer overlaps the climb tiles, leave climb mode.
        if (!onClimbTile()) {
            player.climbing = 0;
            player.grounded = isStandingOnSolid();
        }
    } else {
        // Vertical movement from jump / gravity.
        if (player.yVel != 0) {
            step = (player.yVel > 0) ? 1 : -1;
            remaining = (player.yVel > 0) ? player.yVel : -player.yVel;

            while (remaining > 0) {
                if (canMoveTo(player.x, player.y + step)) {
                    player.y += step;
                } else {
                    // Hitting the ground ends downward movement and marks the
                    // player as grounded.
                    if (step > 0) {
                        player.grounded = 1;
                    }

                    // Any vertical collision cancels the current vertical speed.
                    player.yVel = 0;
                    break;
                }
                remaining--;
            }
        }
    }

    // Clamp to world bounds so the player cannot leave the playable map space.
    if (player.x < 0) {
        player.x = 0;
    }
    if (player.y < 0) {
        player.y = 0;
    }
    if (player.x > levelWidth - player.width) {
        player.x = levelWidth - player.width;
    }

    // Final grounded check
    //
    // This is recomputed after movement so the rest of the frame uses the
    // player's final position rather than the earlier starting position.
    if (!player.climbing) {
        player.grounded = isStandingOnSolid();
        if (player.grounded && player.yVel > 0) {
            player.yVel = 0;
        }
    }

    // Decide which animation row / facing direction to use.
    //
    // Horizontal movement chooses left/right.
    if (dx < 0) {
        player.direction = DIR_LEFT;
    } else if (dx > 0) {
        player.direction = DIR_RIGHT;
    }

    // While climbing, choose up/down based on actual vertical motion.
    if (player.climbing) {
        if (wantsClimbUp && player.y < player.oldY) {
            player.direction = DIR_UP;
        } else if (wantsClimbDown && player.y > player.oldY) {
            player.direction = DIR_DOWN;
        }
    } else if (!player.grounded) {
        // While airborne, use the down/fall row.
        player.direction = DIR_DOWN;
    }

    // Decide whether the player should be considered "moving" for animation.
    //
    // This checks actual position changes, not just button presses, so animation
    // only advances when motion truly happened.
    if ((dx != 0 && player.x != player.oldX) ||
        (player.climbing && wantsClimbUp && player.y < player.oldY) ||
        (player.climbing && wantsClimbDown && player.y > player.oldY) ||
        (!player.climbing && player.y < player.oldY)) {
        wasActivelyMoving = 1;
    }

    player.isMoving = wasActivelyMoving;
}

// repositions the camera so it follows the player within the level bounds. It also updates the scrolling values used by the background layers
void updateCamera(void) {
    // Center the camera on the player, then clamp it to the map bounds.
    hOff = player.x + (player.width / 2) - (SCREENWIDTH / 2);
    vOff = player.y + (player.height / 2) - (SCREENHEIGHT / 2);

    if (hOff < 0) {
        hOff = 0;
    }
    if (vOff < 0) {
        vOff = 0;
    }
    if (hOff > levelWidth - SCREENWIDTH) {
        hOff = levelWidth - SCREENWIDTH;
    }
    if (vOff > levelHeight - SCREENHEIGHT) {
        vOff = levelHeight - SCREENHEIGHT;
    }

    // If the map is smaller than the screen, do not scroll that axis at all.
    if (levelWidth < SCREENWIDTH) {
        hOff = 0;
    }
    if (levelHeight < SCREENHEIGHT) {
        vOff = 0;
    }
}

// Returns the width of the cloud background map in pixels
int getCloudMapWidth(void) {
    if (currentLevel == LEVEL_HOME) {
        return HOME_PIXEL_W;
    }

    // Level 1 and Level 2 both use the shared level background map.
    return LEVEL1_PIXEL_W;
}


// Pushes the latest gameplay scroll values into the active background registers. This keeps the tilemap layers aligned with the camera and cloud motion
void applyBackgroundOffsets(void) {
    // This is where the camera and parallax meet. BG2 follows the player as
    // the main gameplay layer, while BG1 behaves like a drifting cloud layer.
    // The cloud layer scrolls on a timer, so the world feels more alive even
    // if the player stands still.
    int cloudMapWidth = getCloudMapWidth();

    // Animate the cloud layer using only a horizontal timer-based scroll.
    // This keeps the clouds drifting across the screen over time.
    cloudScrollX = (frameCounter / CLOUD_SCROLL_DELAY) % cloudMapWidth;

    // BG1 = clouds
    // Auto-scroll horizontally while still respecting the current vertical view.
    setBackgroundOffset(1, cloudScrollX, vOff);

    // BG2 = main gameplay layer
    // Foreground follows the camera normally.
    setBackgroundOffset(2, hOff, vOff);
}