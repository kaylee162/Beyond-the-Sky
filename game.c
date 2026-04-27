#include "game_internal.h"

/* DEV NOTES FOR THIS FILE:
 *
 * This file acts as the main gameplay hub that the rest of the program talks to.
 * It owns the shared global state for the game, including:
 *   - the current screen/state the player is on
 *   - which level is currently loaded
 *   - player, enemy, and collectible runtime data
 *   - camera offsets and scrolling values
 *   - inventory and beanstalk progression state
 *   - cheat flags and respawn information
 *
 * In the larger scope of the game:
 * This file is the bridge between main.c and the rest of the game systems. 
 * main.c only needs to call initGame(), updateGame(),
 * and drawGame(), and this file delegates the work out to the
 * appropriate systems depending on what state the game is currently in.
 */

// -----------------------------------------------------------------------------
// Global state shared across the refactored game modules.
// These variables represent the live state of the current play session.
// -----------------------------------------------------------------------------
GameState state;
GameState gameplayStateBeforePause;
int currentLevel;
int levelWidth;
int levelHeight;

Player player;

ResourceItem beanSprout;
ResourceItem bonemeal;
ResourceItem waterDroplet;

Bee bees[3];
int beeCount;

int hOff;
int vOff;
int cloudScrollX;
int frameCounter;
int inventoryFlags;
int beanstalkGrowthStage;
int respawnX;
int respawnY;
int respawnPreferredY;
int loseReturnLevel;
int cheatModeEnabled;
int invincibilityCheat;

// ======================================================
//                GAME LIFECYCLE ENTRY POINTS
// ======================================================


// Initializes a fresh play session by resetting shared state, loading the home level, and preparing the graphics setup before sending the player to the title screen.
void initGame(void) {
    // Set up VRAM, backgrounds, sprite systems, palettes, and other display data.
    initGraphics();

    // Initalize sound
    initSound();
    // set up sound A as the background music
    playSoundA(background_music_data, background_music_length, background_music_sampleRate, 1);

    // Reset frame-based systems and progression tracking for a fresh playthrough.
    frameCounter = 0;
    inventoryFlags = 0;
    beanstalkGrowthStage = 0;
    invincibilityCheat = 0;
    cheatModeEnabled = 0;
    beeCount = 0;

    // Start the game in the home level before the player begins actual play.
    currentLevel = LEVEL_HOME;
    levelWidth = getLevelPixelWidth(currentLevel);
    levelHeight = getLevelPixelHeight(currentLevel);

    // Load the starting level's background, foreground, and collision data.
    loadLevelMaps(currentLevel);

    // Make sure all background layers begin aligned at their origin.
    setBackgroundOffset(0, 0, 0);
    setBackgroundOffset(1, 0, 0);
    setBackgroundOffset(2, 0, 0);

    // Reset cloud parallax tracking so the background animation starts cleanly.
    cloudScrollX = 0;

    // Transfer control to the title screen state.
    goToStart();
}

// Updates the game based on the current state (menu, gameplay, etc).
// Routes logic to the appropriate update handler.
void updateGame(void) {
    // Count frames globally so timed systems can reference one shared clock.
    frameCounter++;

    // setup sound as looping and non-looping sounds sounds restart / stop correctly
    setupSounds();

    // Run only the logic for the currently active game state.
    switch (state) {
        case STATE_START:
            updateStart();
            break;
        case STATE_INSTRUCTIONS:
            updateInstructions();
            break;
        case STATE_HOME:
        case STATE_LEVEL1:
        case STATE_LEVEL2:
            updateGameplayCommon();
            break;
        case STATE_PAUSE:
            updatePause();
            break;
        case STATE_WIN:
            updateWin();
            break;
        case STATE_LOSE:
            updateLose();
            break;
    }
}

// Draws the current frame based on the active game state.
// Delegates rendering to state-specific draw functions.
void drawGame(void) {
    // Render only the visuals that belong to the active game state.
    switch (state) {
        case STATE_START:
            drawTitleScreen();
            break;

        case STATE_INSTRUCTIONS:
            drawInstructionsPage();
            break;

        case STATE_HOME:
        case STATE_LEVEL1:
        case STATE_LEVEL2:
            drawGameplay();
            break;

        case STATE_PAUSE:
            drawPauseScreen();
            break;

        case STATE_WIN:
            // The win screen is text-based, so clear the HUD layer first.
            clearHud();
            putText(11, 8, "YOU WIN");
            putText(2, 12, "PRESS START TO PLAY AGAIN");
            break;

        case STATE_LOSE:
            // The lose screen is also text-based and prompts the player to respawn.
            clearHud();
            putText(12, 8, "YOU DIED");
            putText(5, 12, "PRESS START TO RESPAWN");
            break;
    }
}