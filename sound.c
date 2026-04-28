#include "gba.h"
#include "sound.h"

SOUND soundA;
SOUND soundB;

void setupSounds() {

    REG_SOUNDCNT_X = SND_ENABLED;

	REG_SOUNDCNT_H = SND_OUTPUT_RATIO_100 |
                     DSA_OUTPUT_RATIO_100 |
                     DSA_OUTPUT_TO_BOTH |
                     DSA_TIMER0 |
                     DSA_FIFO_RESET |
                     DSB_OUTPUT_RATIO_100 |
                     DSB_OUTPUT_TO_BOTH |
                     DSB_TIMER1 |
                     DSB_FIFO_RESET;

	REG_SOUNDCNT_L = 0;

}

void playSoundA(const signed char* data, int dataLength, int looping) {
    
    // Set DMA channel to 1
    DMANow(1, (volatile void*) data, (volatile void*) REG_FIFO_A, DMA_DESTINATION_FIXED | DMA_AT_REFRESH | DMA_REPEAT | DMA_32);

    // Set up timer 0
    REG_TM0CNT = 0;
    int cyclesPerSecond = PROCESSOR_CYCLES_PER_SECOND / SOUND_FREQ;
    REG_TM0D = 65536 - cyclesPerSecond;
    REG_TM0CNT = TIMER_ON;

    // Initialize struct members of soundA
    soundA.data = data;
    soundA.dataLength = dataLength;
    soundA.looping = looping;
    soundA.isPlaying = 1;
    soundA.durationInVBlanks = (VBLANK_FREQ * dataLength) / SOUND_FREQ;
    soundA.vBlankCount = 0;

}

void playSoundB(const signed char* data, int dataLength, int looping) {

    // Set DMA channel to 2
    DMA[2].ctrl = 0;
    DMANow(2, (volatile void*) data, (volatile void*) REG_FIFO_B, DMA_DESTINATION_FIXED | DMA_AT_REFRESH | DMA_REPEAT | DMA_32);

    // Set up timer 1
    REG_TM1CNT = 0;
    int cyclesPerSecond = PROCESSOR_CYCLES_PER_SECOND / SOUND_FREQ;
    REG_TM1D = 65536 - cyclesPerSecond;
    REG_TM1CNT = TIMER_ON;

    // Initialize struct members of soundB
    soundB.data = data;
    soundB.dataLength = dataLength;
    soundB.looping = looping;
    soundB.isPlaying = 1;
    soundB.durationInVBlanks = (VBLANK_FREQ * dataLength) / SOUND_FREQ;
    soundB.vBlankCount = 0;

}

void updateSounds() {
    if (soundA.isPlaying) {
        soundA.vBlankCount++;

        if (soundA.vBlankCount >= soundA.durationInVBlanks) {
            if (soundA.looping) {
                playSoundA(soundA.data, soundA.dataLength, soundA.looping);
            } else {
                soundA.isPlaying = 0;
                REG_TM0CNT = 0;
                DMA[1].ctrl = 0;
            }
        }
    }

    if (soundB.isPlaying) {
        soundB.vBlankCount++;

        if (soundB.vBlankCount >= soundB.durationInVBlanks) {
            if (soundB.looping) {
                playSoundB(soundB.data, soundB.dataLength, soundB.looping);
            } else {
                soundB.isPlaying = 0;
                REG_TM1CNT = 0;
                DMA[2].ctrl = 0;
            }
        }
    }
}

void pauseSounds() {
    soundA.isPlaying = 0;
    REG_TM0CNT = 0;
    soundB.isPlaying = 0;
    REG_TM1CNT = 0;

}

void unpauseSounds() {
    soundA.isPlaying = 1;
    REG_TM0CNT = TIMER_ON;
    soundB.isPlaying = 1;
    REG_TM1CNT = TIMER_ON;

}

void stopSounds() {
    soundA.isPlaying = 0;
    REG_TM0CNT = 0;
    DMA[1].ctrl = 0;
    soundB.isPlaying = 0;
    REG_TM1CNT = 0;
    DMA[2].ctrl = 0;

}