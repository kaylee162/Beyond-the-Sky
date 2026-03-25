#include "sprites.h"

// Hides all sprites in the shadowOAM
void hideSprites() {
    int i;

    // Hide every sprite entry
    for (i = 0; i < 128; i++) {
        shadowOAM[i].attr0 = ATTR0_HIDE;
        shadowOAM[i].attr1 = 0;
        shadowOAM[i].attr2 = 0;
        shadowOAM[i].fill = 0;
    }
}