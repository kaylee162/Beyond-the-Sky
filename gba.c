#include "gba.h"

// Pointer to the start of video memory
unsigned volatile short *videoBuffer = (unsigned short *)0x6000000;

// Checks for collision between two rectangles
int collision(int x1, int y1, int width1, int height1, int x2, int y2, int width2, int height2) {
    return y1 < y2 + height2 && y1 + height1 > y2 && x1 < x2 + width2 && x1 + width1 > x2;
}

// Waits until drawing pixel 0 of line 160
void waitForVBlank() {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

// Immediately begins a DMA transfer using parameters
void DMANow(int channel, volatile void* src, volatile void* dest, unsigned int ctrl) {
    DMA[channel].ctrl = 0;
    DMA[channel].src = src;
    DMA[channel].dest = dest;
    DMA[channel].ctrl = ctrl | DMA_ON;
}