# Beyond the Sky

## Overview

**Beyond the Sky** is a Game Boy Advance platformer built in **Mode 0** where the player explores a home base and progresses through sky levels by growing a beanstalk.

The gameplay loop focuses on exploration, resource collection, and vertical progression, with smooth movement, layered backgrounds, and simple but polished mechanics.

---

## Controls

* **D-Pad Left / Right** → Move
* **D-Pad Up** → Jump or Climb ladder/beanstalk (if at a climbing tile)
* **START** → Pause / Resume
* **SELECT + UP** → Toggle cheat mode

---

## Core Gameplay Loop

1. Spawn in the **home base**
2. Collect a **bean sprout**
3. Deposit it to upgrade farmland, plants the seeds in the farmland
4. Collect bonemeal in **Level 1**
5. Deposit bonemeal to grow the **beanstalk**
6. Climb upward to reach **Level 2**
7. Collect water droplet and deposit to fully grow the **beanstalk**
8. Climb to the top and win

---

## Game States

* Start Screen
* Instructions Screen
* Home Base
* Level 1 (side-scrolling sky level)
* Level 2 (side-scrolling sky level)
* Pause Screen
* Win Screen
* Lose Screen

---

## Key Features

### Player Mechanics

* Smooth horizontal movement and jumping with gravity
* Ladder / beanstalk climbing system
* Improved ladder-top behavior (no getting stuck at the top)
* Refined collision detection using multiple probe points

---

### World & Progression

* Multi-stage **beanstalk growth system**

  * Regular farmland
  * Seed farmland (after bean sprout)
  * Fully grown beanstalk (after bonemeal)
* Tilemap modification for dynamic world changes
* Multiple level transitions (top and bottom portals)

---

### Camera System

* Camera follows the player in larger levels
* Clamped scrolling within level bounds
* Stable transitions between levels and home base

---

### Background & Visuals

* **Layered background system (Mode 0)**

  * Foreground gameplay layer
  * Separate cloud background layer
* **Animated cloud system**

  * Smooth horizontal looping
  * Independent from camera movement
  * Fixed vertical position for realism
* Tile-based environments with consistent palette usage

---

### HUD & UI

* Dynamic HUD displaying:

  * Current level
  * Inventory
  * Cheat status (when enabled)
* Clean HUD clearing and redraw system
* Text rendering using tilemap font system

---

### Inventory System

* Tracks collected items (bean sprout, bonemeal)
* Displays current inventory in HUD
* Used to trigger world progression events

---

### Collision System

* Collision maps using palette indices:

  * Walkable / blocked tiles
  * Hazards
  * Climbable tiles
  * Level transitions
* Improved collision accuracy for tall player sprite
* Hazard detection and respawn handling

---

### Level Transitions

* Multiple transition types:

  * Home ↔ Level 1 (top and bottom entries)
  * Home ↔ Level 2
* Precise spawn positioning on transitions
* Seamless camera and state updates

---

### Cheats / Debug Features

* Toggleable cheat mode
* Debug tools for testing progression (e.g., instant growth behavior)
* Visual indicator when cheats are enabled

---

## Technical Highlights

* Built using **Mode 0 tilemaps and sprites**
* Efficient rendering using:

  * Hardware scrolling (`REG_BGxHOFF / VOFF`)
  * DMA for fast memory transfers
* Clean state machine architecture for game flow
* Modular systems:

  * Player
  * Camera
  * Collision
  * Rendering
  * World updates

---

## Known Challenges Solved

* Fixed ladder “stuck at top” bug
* Improved collision for tall player sprites
* Resolved tilemap palette inconsistencies
* Eliminated background flickering issues

---

## Future Improvements

* Daytime Cycle with palette modification at runtime
* Moving sun sprite to help animate the daytime cycle
* Add movement to some of the platforms in level 1 and 2
* Adding digital sound (background and action sounds)
---

## Credits

Developed as part of a **Game Boy Advance programming project** using the devkitARM toolchain and mGBA emulator.