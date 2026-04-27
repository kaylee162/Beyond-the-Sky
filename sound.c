#include "gba.h"
#include "sound.h"

Sound soundA;
Sound soundB;

void initSound(void) {
    REG_SOUNDCNT_X = SND_ENABLED;

    REG_SOUNDCNT_H =
        SND_OUTPUT_RATIO_100 |
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

void playSoundA(const signed char* data, int length, int frequency, int loops) {
    // stop the dma first 
    DMA[1].ctrl = 0;

    int ticks = PROCESSOR_CYCLES_PER_SECOND / frequency;

    REG_TM0CNT = 0;
    REG_TM0D = -ticks;
    REG_TM0CNT = TIMER_ON;

    DMA[1].src = data;
    DMA[1].dest = &REG_FIFO_A;
    DMA[1].ctrl = DMA_DESTINATION_FIXED | DMA_AT_REFRESH | DMA_REPEAT | DMA_32 | DMA_ON;

    // sound state
    soundA.data = data;
    soundA.length = length;
    soundA.frequency = frequency;
    soundA.loops = loops;
    soundA.isPlaying = 1;
    soundA.vBlankCount = 0;
    soundA.duration = ((length * 60) / frequency);
}

void playSoundB(const signed char* data, int length, int frequency, int loops) {
    // Stop channel B completely before starting the new effect.
    DMA[2].ctrl = 0;
    REG_TM1CNT = 0;

    // Flush FIFO B so old action sound data does not play first.
    REG_SOUNDCNT_H |= DSB_FIFO_RESET;

    int ticks = PROCESSOR_CYCLES_PER_SECOND / frequency;
    REG_TM1D = -ticks;

    soundB.data = data;
    soundB.length = length;
    soundB.frequency = frequency;
    soundB.loops = loops;
    soundB.isPlaying = 1;
    soundB.vBlankCount = 0;
    soundB.duration = ((length * 60) / frequency);

    // Prime FIFO B immediately so short sound effects start right away.
    // This helps action sounds feel synced to deaths, deposits, and hits.
    const unsigned int* soundData = (const unsigned int*) data;

    if (length >= 16) {
        REG_FIFO_B = soundData[0];
        REG_FIFO_B = soundData[1];
        REG_FIFO_B = soundData[2];
        REG_FIFO_B = soundData[3];

        DMA[2].src = data + 16;
    } else {
        DMA[2].src = data;
    }

    DMA[2].dest = &REG_FIFO_B;
    DMA[2].ctrl = DMA_DESTINATION_FIXED | DMA_AT_REFRESH | DMA_REPEAT | DMA_32 | DMA_ON;

    REG_TM1CNT = TIMER_ON;
}

void setupSounds(void) {
    // sound A is the looping background sound
    if (soundA.isPlaying) {
        soundA.vBlankCount++;

        if (soundA.vBlankCount > soundA.duration) {
            if (soundA.loops) {
                playSoundA(soundA.data, soundA.length, soundA.frequency, soundA.loops);
            } else {
                DMA[1].ctrl = 0;
                soundA.isPlaying = 0;
            }
        }
    }

    // channel b has the action sounds (for damage and planing)
    if (soundB.isPlaying) {
        soundB.vBlankCount++;

        if (soundB.vBlankCount > soundB.duration) {
            if (soundB.loops) {
                playSoundB(soundB.data, soundB.length, soundB.frequency, soundB.loops);
            } else {
                DMA[2].ctrl = 0;
                soundB.isPlaying = 0;
            }
        }
    }
}

void pauseSound(void) {
    REG_TM0CNT = 0;
    REG_TM1CNT = 0;
}

void unpauseSound(void) {
    REG_TM0CNT = TIMER_ON;
    REG_TM1CNT = TIMER_ON;
}

void stopSound(void) {
    DMA[1].ctrl = 0;
    DMA[2].ctrl = 0;

    REG_TM0CNT = 0;
    REG_TM1CNT = 0;

    soundA.isPlaying = 0;
    soundB.isPlaying = 0;
}