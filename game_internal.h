#ifndef GAME_INTERNAL_H
#define GAME_INTERNAL_H

/* QUICK NOTE:
 * this is the internal gameplay coordination header 
 *
 * This file shares the game-wide state and internal helper prototypes across
 * all the gameplay modules.
 */

#include <string.h>
#include "game.h"
#include "tileset.h"
#include "spriteSheet.h"
#include "homebase_foreground.h"
#include "title.h"
#include "sound.h"
#include "background_music.h"
#include "damage.h"
#include "ouch.h"
#include "planting.h"

extern GameState state;
extern GameState gameplayStateBeforePause;
extern int currentLevel;
extern int levelWidth;
extern int levelHeight;

extern Player player;
extern ResourceItem beanSprout;
extern ResourceItem bonemeal;
extern ResourceItem waterDroplet;
extern Bee bees[3];
extern int beeCount;

extern int hOff;
extern int vOff;
extern int cloudScrollX;
extern int frameCounter;
extern int inventoryFlags;
extern int beanstalkGrowthStage;
extern int respawnX;
extern int respawnY;
extern int respawnPreferredY;
extern int loseReturnLevel;
extern int cheatModeEnabled;
extern int invincibilityCheat;
extern int instructionsPage;

unsigned short lerpSkyColor(unsigned short startColor, unsigned short endColor,
    int step, int totalSteps);
void updateDaytimePalette(void);
void setMenuPalette(void);
void initGraphics(void);
void initBgAssets(void);
void initObjAssets(void);
void copySpriteBlockFromSheet(const unsigned short* sheetTiles,
    int sheetWidthTiles, int srcTileX, int srcTileY, int blockWidthTiles,
    int blockHeightTiles, int dstTileIndex);
void loadMenuBackground(void);
void enableGameplayDisplay(void);
void enableMenuDisplay(void);

void goToStart(void);
void goToInstructions(void);
void goToHome(int respawn);
void goToLevelOne(int respawn);
void goToLevelTwo(int respawn);
void goToPause(void);
void goToWin(void);
void goToLose(int levelToReturnTo);
void respawnIntoCurrentLevel(void);
void updateStart(void);
void updateInstructions(void);
void updatePause(void);
void updateLose(void);
void updateWin(void);

void updateGameplayCommon(void);
void updatePlayerAnimation(void);
void updatePlayerMovement(void);
void updateCamera(void);
int getCloudMapWidth(void);
void applyBackgroundOffsets(void);

unsigned short getHomeForegroundSourceEntry(int tileX, int tileY);
void setHomeForegroundTileEntry(int tileX, int tileY, unsigned short entry);
void drawHomeTileBlockFromTileset(int mapTileX, int mapTileY, int srcTileX,
    int srcTileY, int widthTiles, int heightTiles);
void refreshHomeBeanstalkVisuals(void);
int isPlayerAtFarmland(void);
void tryDepositResource(void);
int canUseCurrentClimbPixel(int x, int y);

int bodyHitsSolidAt(int x, int y);
int canMoveTo(int newX, int newY);
int findStandingSpawnY(int spawnX, int preferredTopY);
int onClimbTile(void);
int onClimbTileAt(int x, int y);
int isAtTopOfClimb(int x, int y);
int tryStartLadderTopExit(void);
int isStandingOnSolid(void);
int touchesHazard(void);
int playerTouchesBee(void);
int fellOutOfLevel(void);
void recoverFromInvincibleFall(void);
int touchesWinTile(void);
void handleLevelTransitions(void);

int beeBodyHitsSolidAt(int x, int y);
int canBeeMoveTo(int newX, int newY);
void initBee(Bee* bee, int x, int y, int patrolRange, int startDir);
void initLevelBees(void);
void updateBees(void);

void initBeanSprout(void);
void initBonemeal(void);
void initWaterDroplet(void);
int rectsOverlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh);
void updateCollectibles(void);
void updateCollectibleAnimations(void);
void giveNextResourceCheat(void);
const char* getInventoryText(void);

void drawGameplay(void);
void drawSprites(void);
void hideSprite(int index);
void drawHudText(void);
void drawTitleScreen(void);
void drawInstructionsPage(void);
void drawPauseScreen(void);
void clearHud(void);
void putText(int col, int row, const char* str);

#define LOSE_DELAY_FRAMES 22 

extern int loseDelayCounter;
extern int loseTransitionActive;

void startLoseDelay(void);
void updateLoseDelay(void);

#endif
