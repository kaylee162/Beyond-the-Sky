#include "game_internal.h"

/* DEV NOTES FOR THIS FILE:
 *
 * This file is in charge of the visual side of the game once the game state
 * has already been updated elsewhere. this module does NOT decide
 * gameplay outcomes like movement, collisions, pickups, or transitions,
 * instead, it takes the current game state and turns it into what the player
 * sees on screen.
 *
 * More specifically, this file handles:
 * - drawing the gameplay frame each update
 * - restoring and updating the runtime daytime sky palette
 * - drawing all visible sprites from world space into screen space
 * - hiding sprites that are inactive or off-screen
 * - drawing HUD text such as level names, inventory, and cheat indicators
 * - drawing menu overlay text for the title, instructions, and pause screens
 *
 * In the larger scope of the game:
 * - update modules decide what the game state should be
 * - this render module displays that state cleanly every frame
 */

// Draws one gameplay frame using the current world, HUD, and sprite state. It also keeps the active palette and background offsets visually in sync with gameplay
void drawGameplay(void) {
    // Restore and animate the sky palette during gameplay.
    updateDaytimePalette();

    // Refresh HUD text, scroll the animated cloud layer plus the foreground,
    // and then draw all visible sprites.
    clearHud();
    drawHudText();

    applyBackgroundOffsets();

    drawSprites();
}

// builds the OAM entries for the player, collectibles, and enemies, then uploads them for the current frame. This is the main sprite rendering pass for gameplay
void drawSprites(void) {
    // Write the current frame's sprite attributes into shadow OAM.
    //
    // The sprites are drawn in screen space, not world space, so every sprite
    // position is converted by subtracting the camera offsets first. The tile
    // index chooses the correct frame, while the palette row chooses the color
    // bank. That is why the player and world items can share OBJ memory cleanly
    // but still use different colors.
    int screenX = player.x - hOff;
    int screenY = player.y - vOff;

    // --------------------------------------------------
    // Draw player
    // --------------------------------------------------
    //
    // The player is the main controllable sprite. This block:
    // - checks whether the player is visible on screen
    // - chooses the correct animation row based on movement direction
    // - selects the current animation frame within that row
    // - writes the correct OBJ attributes for a 16x32 sprite
    //
    // In the larger game, this is what makes walking, climbing, and falling
    // animations appear correctly to the player.
    if (screenX < -player.width || screenX >= SCREENWIDTH ||
        screenY < -player.height || screenY >= SCREENHEIGHT) {
        hideSprite(OBJ_INDEX_PLAYER);
    } else {
        int tileBase;

        // Pick the correct directional animation row.
        if (player.direction == DIR_LEFT) {
            tileBase = OBJ_TILE_PLAYER_LEFT + player.animFrame * PLAYER_TILES_PER_FRAME;
        } else if (player.direction == DIR_UP) {
            tileBase = OBJ_TILE_PLAYER_UP + player.animFrame * PLAYER_TILES_PER_FRAME;
        } else if (player.direction == DIR_DOWN) {
            tileBase = OBJ_TILE_PLAYER_DOWN + player.animFrame * PLAYER_TILES_PER_FRAME;
        } else {
            tileBase = OBJ_TILE_PLAYER_RIGHT + player.animFrame * PLAYER_TILES_PER_FRAME;
        }

        // 16x32 sprite = TALL + MEDIUM
        shadowOAM[OBJ_INDEX_PLAYER].attr0 = ATTR0_Y(screenY) | ATTR0_TALL | ATTR0_4BPP;
        shadowOAM[OBJ_INDEX_PLAYER].attr1 = ATTR1_X(screenX) | ATTR1_MEDIUM;
        shadowOAM[OBJ_INDEX_PLAYER].attr2 = ATTR2_TILEID(tileBase)
            | ATTR2_PRIORITY(0)
            | ATTR2_PALROW(PLAYER_PALROW);
    }

    // --------------------------------------------------
    // Draw bean sprout
    // --------------------------------------------------
    //
    // The bean sprout only exists in the home level while it is still active.
    //
    // Since a 16x24 sprite is not a native GBA sprite size, it is split into
    // two OBJ entries:
    // - a 16x16 top piece
    // - a 16x8 bottom piece
    //
    // In the larger scope of the game, this collectible starts the beanstalk
    // progression loop. Rendering it correctly helps communicate that it can be
    // collected and brought back to the farmland.
    if (currentLevel == LEVEL_HOME && beanSprout.active) {
        int beanScreenX = beanSprout.x - hOff;
        int beanScreenY = beanSprout.y - vOff;

        if (beanScreenX < -BEAN_SPROUT_WIDTH || beanScreenX >= SCREENWIDTH ||
            beanScreenY < -BEAN_SPROUT_HEIGHT || beanScreenY >= SCREENHEIGHT) {
            hideSprite(OBJ_INDEX_BEAN_TOP);
            hideSprite(OBJ_INDEX_BEAN_BOTTOM);
        } else {
            // Top 16x16 portion
            shadowOAM[OBJ_INDEX_BEAN_TOP].attr0 =
                ATTR0_Y(beanScreenY) | ATTR0_SQUARE | ATTR0_4BPP;
            shadowOAM[OBJ_INDEX_BEAN_TOP].attr1 =
                ATTR1_X(beanScreenX) | ATTR1_SMALL;
            shadowOAM[OBJ_INDEX_BEAN_TOP].attr2 =
                ATTR2_TILEID(OBJ_TILE_BEAN_SPROUT)
                | ATTR2_PRIORITY(0)
                | ATTR2_PALROW(WORLD_SPRITE_PALROW);

            // Bottom 16x8 portion starts at tile offset +4
            shadowOAM[OBJ_INDEX_BEAN_BOTTOM].attr0 =
                ATTR0_Y(beanScreenY + 16) | ATTR0_WIDE | ATTR0_4BPP;
            shadowOAM[OBJ_INDEX_BEAN_BOTTOM].attr1 =
                ATTR1_X(beanScreenX) | ATTR1_TINY;
            shadowOAM[OBJ_INDEX_BEAN_BOTTOM].attr2 =
                ATTR2_TILEID(OBJ_TILE_BEAN_SPROUT + 4)
                | ATTR2_PRIORITY(0)
                | ATTR2_PALROW(WORLD_SPRITE_PALROW);
        }
    } else {
        hideSprite(OBJ_INDEX_BEAN_TOP);
        hideSprite(OBJ_INDEX_BEAN_BOTTOM);
    }

    // --------------------------------------------------
    // Draw bonemeal in Level 1
    // --------------------------------------------------
    //
    // This collectible is only shown in Level 1 while it has not been picked up.
    // The animation frame is chosen using the bonemeal object's animFrame field.
    //
    // In the larger game loop, collecting this is part of the second growth
    // step of the beanstalk.
    if (currentLevel == LEVEL_ONE && bonemeal.active) {
        int itemScreenX = bonemeal.x - hOff;
        int itemScreenY = bonemeal.y - vOff;
        int tileBase = OBJ_TILE_BONEMEAL + bonemeal.animFrame * RESOURCE_TILES_PER_FRAME;

        if (itemScreenX < -RESOURCE_WIDTH || itemScreenX >= SCREENWIDTH ||
            itemScreenY < -RESOURCE_HEIGHT || itemScreenY >= SCREENHEIGHT) {
            hideSprite(OBJ_INDEX_BONEMEAL);
        } else {
            shadowOAM[OBJ_INDEX_BONEMEAL].attr0 =
                ATTR0_Y(itemScreenY) | ATTR0_SQUARE | ATTR0_4BPP;
            shadowOAM[OBJ_INDEX_BONEMEAL].attr1 =
                ATTR1_X(itemScreenX) | ATTR1_SMALL;
            shadowOAM[OBJ_INDEX_BONEMEAL].attr2 =
                ATTR2_TILEID(tileBase)
                | ATTR2_PRIORITY(0)
                | ATTR2_PALROW(WORLD_SPRITE_PALROW);
        }
    } else {
        hideSprite(OBJ_INDEX_BONEMEAL);
    }

    // --------------------------------------------------
    // Draw water droplet in Level 2
    // --------------------------------------------------
    //
    // This collectible behaves similarly to bonemeal, but only appears in
    // Level 2. It uses the same general resource rendering approach:
    // visible check, animation frame selection, and one OBJ entry.
    //
    // In the larger game loop, this is the final resource needed to fully grow
    // the beanstalk.
    if (currentLevel == LEVEL_TWO && waterDroplet.active) {
        int itemScreenX = waterDroplet.x - hOff;
        int itemScreenY = waterDroplet.y - vOff;
        int tileBase = OBJ_TILE_WATER + waterDroplet.animFrame * RESOURCE_TILES_PER_FRAME;

        if (itemScreenX < -RESOURCE_WIDTH || itemScreenX >= SCREENWIDTH ||
            itemScreenY < -RESOURCE_HEIGHT || itemScreenY >= SCREENHEIGHT) {
            hideSprite(OBJ_INDEX_WATER);
        } else {
            shadowOAM[OBJ_INDEX_WATER].attr0 =
                ATTR0_Y(itemScreenY) | ATTR0_SQUARE | ATTR0_4BPP;
            shadowOAM[OBJ_INDEX_WATER].attr1 =
                ATTR1_X(itemScreenX) | ATTR1_SMALL;
            shadowOAM[OBJ_INDEX_WATER].attr2 =
                ATTR2_TILEID(tileBase)
                | ATTR2_PRIORITY(0)
                | ATTR2_PALROW(WORLD_SPRITE_PALROW);
        }
    } else {
        hideSprite(OBJ_INDEX_WATER);
    }

    // --------------------------------------------------
    // Draw bees
    // --------------------------------------------------
    //
    // Each bee enemy is 16x24, so each one is split into:
    // - a 16x16 top sprite
    // - a 16x8 bottom sprite
    //
    // The sprite sheet only includes one facing direction, so horizontal flip
    // is used to visually reverse the bee depending on travel direction.
    //
    // In the larger scope of the game, bees are moving hazards. Their movement
    // is handled elsewhere, but this function is what makes them appear alive
    // and animated on screen.
    for (int i = 0; i < 3; i++) {
        int topIndex = OBJ_INDEX_BEE0_TOP + (i * 2);
        int bottomIndex = OBJ_INDEX_BEE0_BOTTOM + (i * 2);

        // If this bee slot is unused or inactive, hide both sprite pieces.
        if (i >= beeCount || !bees[i].active) {
            hideSprite(topIndex);
            hideSprite(bottomIndex);
            continue;
        }

        {
            int beeScreenX = bees[i].x - hOff;
            int beeScreenY = bees[i].y - vOff;
            int tileBase = OBJ_TILE_BEE + bees[i].animFrame * BEE_TILES_PER_FRAME;
            
            // Bee art in the sprite sheet already faces left.
            // Only flip it when the bee is moving right.
            int flip = (bees[i].dir > 0) ? ATTR1_HFLIP : 0;

            if (beeScreenX < -BEE_WIDTH || beeScreenX >= SCREENWIDTH ||
                beeScreenY < -BEE_HEIGHT || beeScreenY >= SCREENHEIGHT) {
                hideSprite(topIndex);
                hideSprite(bottomIndex);
            } else {
                // Top 16x16 piece
                shadowOAM[topIndex].attr0 =
                    ATTR0_Y(beeScreenY) | ATTR0_SQUARE | ATTR0_4BPP;
                shadowOAM[topIndex].attr1 =
                    ATTR1_X(beeScreenX) | ATTR1_SMALL | flip;
                shadowOAM[topIndex].attr2 =
                    ATTR2_TILEID(tileBase)
                    | ATTR2_PRIORITY(0)
                    | ATTR2_PALROW(WORLD_SPRITE_PALROW);

                // Bottom 16x8 piece
                shadowOAM[bottomIndex].attr0 =
                    ATTR0_Y(beeScreenY + 16) | ATTR0_WIDE | ATTR0_4BPP;
                shadowOAM[bottomIndex].attr1 =
                    ATTR1_X(beeScreenX) | ATTR1_TINY | flip;
                shadowOAM[bottomIndex].attr2 =
                    ATTR2_TILEID(tileBase + 4)
                    | ATTR2_PRIORITY(0)
                    | ATTR2_PALROW(WORLD_SPRITE_PALROW);
            }
        }
    }

    // Hide every remaining OBJ slot that this game does not actively use.
    //
    // This prevents stale sprite data from previous frames or states from
    // appearing unexpectedly on screen.
    for (int i = 11; i < 128; i++) {
        hideSprite(i);
    }
}

// Hides a sprite by moving its OAM entry offscreen. This is used for inactive or temporarily invisible objects
void hideSprite(int index) {
    // Hide one sprite entry in shadow OAM.
    shadowOAM[index].attr0 = ATTR0_HIDE;
    shadowOAM[index].attr1 = 0;
    shadowOAM[index].attr2 = 0;
}

// ======================================================
//                HUD / TEXT DRAWING
// ======================================================

// draws the HUD labels for the current level, inventory, and cheat status. It refreshes the text layer after gameplay state changes
void drawHudText(void) {
    clearHud();

    if (currentLevel == LEVEL_HOME) {
        putText(1, 1, "HOMEBASE");
    } else if (currentLevel == LEVEL_ONE) {
        putText(1, 1, "LEVEL 1");
    } else {
        putText(1, 1, "LEVEL 2");
    }

    if (cheatModeEnabled) {
        putText(16, 1, "DEBUG ENABLED");
    }

    putText(1, 3, "INVENTORY:");
    putText(1, 5, getInventoryText());
}

// draws the title screen text on top of the shared menu background. The background itself is loaded elsewhere
void drawTitleScreen(void) {
    // The title artwork itself is the BG1 tilemap.
    // This function only draws the overlay text on BG0.
    clearHud();
    putText(9, 6, "BEYOND THE SKY");
    putText(9, 8, "COZY FARM GAME");
    putText(10, 14, "PRESS START"); //9,14
}

// draws the instructions text for the current page. It reuses the menu background and only updates the text layer
void drawInstructionsPage(void) {
    // Draw the current instructions page.
    clearHud();

    // Page 1 focuses on movement controls.
    if (instructionsPage == 0) {
        clearHud();
        putText(7, 4, "INSTRUCTIONS 1/2");
        putText(8, 8,"LEFT/RIGHT MOVE");
        putText(4, 10, "UP/DOWN JUMP OR CLIMB");
        putText(8, 15, "< BACK    NEXT >");
    } else {
        // Page 2 explains the main progression loop of the game.
        clearHud();
        putText(7, 4, "INSTRUCTIONS 2/2");
        putText(7, 8, "FINISH LEVELS TO");
        putText(4, 10, "GET RESOURCES, RETURN");
        putText(3, 12, "HOME & PRESS B ON SOIL");
        putText(6, 16, "< PREV   START PLAY");
    }
}

// Draws the pause menu text over the menu background. This keeps pause presentation separate from gameplay rendering
void drawPauseScreen(void) {
    // Draw pause text and hide gameplay sprites while paused.
    clearHud();
    putText(10, 7, "PAUSED GAME");
    putText(7, 11, "START TO RESUME"); 
    putText(2, 14, "SELECT TO RETURN TO TITLE"); 
    hideSprites();
}

// Clears the HUD text map so a new menu or gameplay overlay can be drawn cleanly
void clearHud(void) {
    // Clear the HUD text map so the next screen can redraw cleanly.
    fontClearMap(HUD_SCREENBLOCK, 0);
}

// Writes a string to the HUD text layer at the given tile position. This is the basic helper used for menus and on-screen labels
void putText(int col, int row, const char* str) {
    fontDrawString(HUD_SCREENBLOCK, col, row, str);
}