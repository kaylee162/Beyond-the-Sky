#ifndef FONT_H
#define FONT_H

#include <stddef.h>
#include "gba.h"

#define FONT_PALROW 14

#define FONT_COLOR RGB(0, 0, 0)

// The array of character data (256 chars * 6 * 8 = 12288)
extern const unsigned char fontdata[12288]; // :contentReference[oaicite:1]{index=1}

// Initializes the font tiles + palette into the given BG charblock.
// - charblock: 0..3
// - tileBase: tile index inside that charblock to start writing at (pick a safe range)
void fontInit(int charblock, int tileBase);

// Clears a screenblock tilemap to a specific tile index (usually 0).
void fontClearMap(int screenblock, unsigned short fillTile);

// Writes one character to a tilemap (screenblock) at tile coordinates (col,row).
// The character is mapped to tile (tileBase + (unsigned char)c).
void fontPutChar(int screenblock, int col, int row, char c);

// Writes a null-terminated string starting at (col,row).
void fontDrawString(int screenblock, int col, int row, const char* str);

#endif