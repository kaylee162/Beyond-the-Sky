#include "game_internal.h"

/* DEV NOTES FOR THIS FILE:
 *
 * This file manages how the game moves between its major modes, such as:
 *   - title screen
 *   - instructions
 *   - home level
 *   - level 1
 *   - level 2
 *   - pause
 *   - win
 *   - lose
 *
 * In the larger scope of the game:
 * This module is the "traffic controller" for the game.
 * It does not handle low-level movement, collision checks, enemy
 * behavior, or sprite drawing directly. Instead, it decides when the game
 * should enter a certain state, prepares the correct level or menu display for
 * that state, resets temporary runtime values when needed, and makes sure the
 * player is placed in a safe and predictable location.
 *
 * In short, this file controls where the player is in the overall game flow
 * and makes sure the world is configured correctly whenever that changes.
 */

// ======================================================
//                STATE / SCREEN TRANSITIONS
// ======================================================
// Removes the level-specific resource when the player dies in that level.
// This keeps progression a little stricter: if you grab the level item but
// die before successfully getting out, you have to earn that pickup again
static void dropLevelResourceOnDeath(int levelToReturnTo) {
    // Home does not need this behavior because the bean sprout is part of the
    // hub setup, not a full side-level completion reward.
    if (levelToReturnTo == LEVEL_ONE) {
        inventoryFlags &= ~INVENTORY_BONEMEAL;
    } else if (levelToReturnTo == LEVEL_TWO) {
        inventoryFlags &= ~INVENTORY_WATER;
    }
}

// Switches the game into the title screen state and prepares the menu display. It resets the visible text layer for a clean title screen
void goToStart(void) {
    // Switch to the title screen and reset the visible menu text.
    state = STATE_START;
    enableMenuDisplay();
    clearHud();
    drawTitleScreen();
    setMenuPalette();
}

// switches to the instructions menu and loads its first page. The shared menu background stays active while the text layer changes
void goToInstructions(void) {
    // Switch to the instructions screen and start on page 1.
    state = STATE_INSTRUCTIONS;
    instructionsPage = 0;
    enableMenuDisplay();
    clearHud();
    drawInstructionsPage();
    setMenuPalette();
}

// Enters the home level, loads its maps, restores the correct player state, and reapplies the home-specific visuals. This is the main setup path for returning to the hub area.
void goToHome(int respawn) {
    // Load the home map, position the player, and reset moment-to-moment movement state.
    state = STATE_HOME;
    enableGameplayDisplay();
    currentLevel = LEVEL_HOME;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();
    updateDaytimePalette();

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;

    if (!respawn) {
        // Default home spawn used for a fresh entry into the level.
        player.x = HOME_SPAWN_X;
        player.y = findStandingSpawnY(player.x, levelHeight - (8 * 8));
    } else {
        // Respawn or transition-based entry uses the stored X location.
        player.x = respawnX;

        // If no exact Y was saved, search downward for a safe standing position.
        if (respawnY < 0) {
            player.y = findStandingSpawnY(player.x, respawnPreferredY);
        } else {
            player.y = respawnY;
        }
    }

    // Reset short-lived movement state so the player starts stable.
    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;
    player.direction = DIR_RIGHT;
    player.animFrame = 0;
    player.animCounter = 0;
    player.isMoving = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    // Save this as the newest valid respawn position.
    respawnX = player.x;
    respawnY = player.y;

    // Rebuild the bean sprout each time home is loaded.
    // If the player already collected it, initBeanSprout() will keep it hidden.
    initBeanSprout();
    beeCount = 0;

    // Reapply the dynamic farmland / beanstalk visuals every time home loads.
    refreshHomeBeanstalkVisuals();

    // Start the home camera lower because the map is taller than the screen.
    hOff = 0;
    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    // Keep the cloud layer vertically fixed and horizontally animated only.
    applyBackgroundOffsets();
}

// Enters level one, loads its data, and places the player at the correct spawn point for that transition
void goToLevelOne(int respawn) {
    // Load Level 1 and place the player at either a default or transition-based spawn.
    state = STATE_LEVEL1;
    enableGameplayDisplay();
    currentLevel = LEVEL_ONE;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();

    // Restore the animated sky palette for gameplay.
    updateDaytimePalette();

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;

    if (!respawn) {
        // Default/fallback Level 1 spawn.
        player.x = LEVEL1_FROM_HOME_BOTTOM_SPAWN_X;
        player.y = findStandingSpawnY(player.x, LEVEL1_FROM_HOME_BOTTOM_PREF_Y);
    } else {
        // Transition or respawn entry uses the saved X location.
        player.x = respawnX;

        // If no exact Y was supplied, compute a safe standing Y from the
        // preferred search height selected by the transition tile.
        if (respawnY < 0) {
            player.y = findStandingSpawnY(player.x, respawnPreferredY);
        } else {
            player.y = respawnY;
        }
    }

    // Reset temporary movement state so the player enters cleanly.
    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;
    player.direction = DIR_LEFT;
    player.animFrame = 0;
    player.animCounter = 0;
    player.isMoving = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    // Save the latest safe position for future respawns.
    respawnX = player.x;
    respawnY = player.y;

    // Rebuild the Level 1 collectible each time the level is loaded.
    // If it was already collected, initBonemeal() keeps it hidden.
    initBonemeal();
    initLevelBees(); // Recreate the bee enemies for this level.

    // Level 1 starts with the camera pushed toward the lower-right area.
    hOff = levelWidth - SCREENWIDTH;
    if (hOff < 0) hOff = 0;

    vOff = levelHeight - SCREENHEIGHT;
    if (vOff < 0) vOff = 0;

    // Keep the cloud layer vertically fixed and horizontally animated only.
    applyBackgroundOffsets();
}

// Enters level two, loads its data, and places the player at the correct spawn point for that transition
void goToLevelTwo(int respawn) {
    // Load Level 2 and place the player near its intended entry point.
    state = STATE_LEVEL2;
    enableGameplayDisplay();
    currentLevel = LEVEL_TWO;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    configureMapBackgroundsForLevel(currentLevel);
    loadLevelMaps(currentLevel);
    clearHud();

    // Restore the animated sky palette for gameplay.
    updateDaytimePalette();

    player.width = PLAYER_WIDTH;
    player.height = PLAYER_HEIGHT;

    if (!respawn) {
        // Spawn on the ground near the left side of the map.
        player.x = LEVEL2_SPAWN_X;
        player.y = findStandingSpawnY(player.x, 6 * 8);
    } else {
        // Respawn reuses the last stored player position.
        player.x = respawnX;
        player.y = respawnY;
    }

    // Reset temporary movement and animation state.
    player.xVel = 0;
    player.yVel = 0;
    player.grounded = 0;
    player.climbing = 0;
    player.direction = DIR_RIGHT;
    player.animFrame = 0;
    player.animCounter = 0;
    player.isMoving = 0;
    player.oldX = player.x;
    player.oldY = player.y;

    // Save the newest valid location as the respawn point.
    respawnX = player.x;
    respawnY = player.y;

    // Rebuild the Level 2 collectible each time the level is loaded.
    // If it was already collected, initWaterDroplet() keeps it hidden.
    initWaterDroplet();
    initLevelBees(); // Recreate the bee enemies for this level.

    // Center the vertical camera around the player, then clamp it safely.
    hOff = 0;
    vOff = player.y + (player.height / 2) - (SCREENHEIGHT / 2);

    if (vOff < 0) vOff = 0;
    if (vOff > levelHeight - SCREENHEIGHT) {
        vOff = levelHeight - SCREENHEIGHT;
    }

    // Keep the cloud layer vertically fixed and horizontally animated only.
    applyBackgroundOffsets();
}

// Switches the game into the pause state while remembering which gameplay state should resume afterward
void goToPause(void) {
    // Remember the gameplay state we came from, then open the pause screen.
    gameplayStateBeforePause = state;
    state = STATE_PAUSE;
    enableMenuDisplay();
    clearHud();
    drawPauseScreen();
    setMenuPalette();
}

// Switches the game into the win state and prepares the menu-style presentation for the victory screen
void goToWin(void) {
    // Switch to the win screen.
    state = STATE_WIN;
    enableMenuDisplay();
    clearHud();
    setMenuPalette();
}

// switches the game into the lose state and records which level the player should return to when they respawn.
void goToLose(int levelToReturnTo) {
    // If the player dies inside a resource level after grabbing its item,
    // drop that level's pickup so they have to complete the level again.
    if (levelToReturnTo == LEVEL_ONE) {
        inventoryFlags &= ~INVENTORY_BONEMEAL;
    } else if (levelToReturnTo == LEVEL_TWO) {
        inventoryFlags &= ~INVENTORY_WATER;
    }

    // Switch to the lose screen and remember which level should be reloaded on respawn.
    loseReturnLevel = levelToReturnTo;
    state = STATE_LOSE;
    enableMenuDisplay();
    clearHud();
    setMenuPalette();
}

// Sends the player back into the appropriate gameplay level after a loss. It uses the stored return target so the respawn behavior matches where the player died
void respawnIntoCurrentLevel(void) {
    // Reload the level stored by the lose screen and respawn the player there.
    if (loseReturnLevel == LEVEL_HOME) {
        goToHome(1);
    } else if (loseReturnLevel == LEVEL_ONE) {
        goToLevelOne(1);
    } else {
        goToLevelTwo(1);
    }
}

// ======================================================
//                PER-STATE UPDATE FUNCTIONS
// ======================================================

// handles title screen input such as starting the game or opening the instructions screen.
void updateStart(void) {
    // Title screen input: START opens the instructions screen.
    if (BUTTON_PRESSED(BUTTON_START)) {
        goToInstructions();
    }
}

// handles input for moving between instruction pages or returning to the title screen
// handles input for moving between instruction pages or returning to the title screen
void updateInstructions(void) {
    // Instructions flow:
    // LEFT  = previous / back to title from page 1
    // RIGHT = next page
    // START = begin the game

    if (BUTTON_PRESSED(BUTTON_LEFT)) {
        if (instructionsPage == 0) {
            // From page 1, go back to the title screen.
            goToStart();
        } else {
            // From page 2, go back to page 1.
            instructionsPage = 0;
            drawInstructionsPage();
            setMenuPalette();
        }
        return;
    }

    if (BUTTON_PRESSED(BUTTON_RIGHT)) {
        if (instructionsPage == 0) {
            // From page 1, go to page 2.
            instructionsPage = 1;
            drawInstructionsPage();
            setMenuPalette();
        }
        return;
    }

    if (BUTTON_PRESSED(BUTTON_START)) {
        // Start a completely fresh run from the home level.
        inventoryFlags = 0;
        beanstalkGrowthStage = 0;
        invincibilityCheat = 0;
        cheatModeEnabled = 0;

        // Restart the daytime cycle so a brand new game always begins at
        // early morning. This only happens on fresh game start, not on
        // normal level transitions.
        frameCounter = 0;

        goToHome(0);
    }
}

// handles pause menu input for resuming the game or returning to the title screen
void updatePause(void) {
    // Pause input: resume gameplay or return to the title screen.
    if (BUTTON_PRESSED(BUTTON_START)) {
        // Go back to the gameplay state we were in before pausing.
        state = gameplayStateBeforePause;

        // Rebuild BG1/BG2 for the active level.
        // This is important because the pause screen reused BG1 for the
        // title/menu tilemap, so we must restore the real gameplay maps.
        configureMapBackgroundsForLevel(currentLevel);
        loadLevelMaps(currentLevel);

        // Homebase uses runtime tilemap edits for the farmland / beanstalk.
        // Reloading the raw exported home map brings back the original full
        // beanstalk art, so reapply the live growth-state edits immediately.
        if (currentLevel == LEVEL_HOME) {
            refreshHomeBeanstalkVisuals();
        }

        // Turn gameplay layers back on.
        enableGameplayDisplay();

        // Restore the gameplay palette now that we are leaving the menu.
        updateDaytimePalette();

        // Reapply the current scroll offsets so the camera resumes exactly
        // where it was before pausing.
        applyBackgroundOffsets();

        // Redraw HUD text cleanly on BG0.
        clearHud();
    }

    if (BUTTON_PRESSED(BUTTON_SELECT)) {
        goToStart();
    }
}

// handles the lose screen input that triggers a respawn into gameplay
void updateLose(void) {
    // Lose screen input: respawn into the stored level.
    if (BUTTON_PRESSED(BUTTON_START) || BUTTON_PRESSED(BUTTON_A)) {
        respawnIntoCurrentLevel();
    }
}

// handles the win screen input for starting a new run from the beginning
void updateWin(void) {
    if (BUTTON_PRESSED(BUTTON_START)) {
        // Fresh run after winning.
        inventoryFlags = 0;
        beanstalkGrowthStage = 0;
        invincibilityCheat = 0;
        cheatModeEnabled = 0;

        // Reset the daytime cycle so the next new run starts at early morning.
        frameCounter = 0;

        goToStart();
    }
}