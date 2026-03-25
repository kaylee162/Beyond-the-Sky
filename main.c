#include "gba.h"
#include "sprites.h"
#include "game.h"

unsigned short oldButtons;
unsigned short buttons;
OBJ_ATTR shadowOAM[128];

int main(void) {
    buttons = REG_BUTTONS;
    oldButtons = buttons;

    initGame();

    while (1) {
        oldButtons = buttons;
        buttons = REG_BUTTONS;

        updateGame();

        waitForVBlank();
        drawGame();
        DMANow(3, shadowOAM, OAM, 128 * 4);
    }

    return 0;
}