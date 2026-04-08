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
3. Deposit it to upgrade farmland (plants the seed)
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
  * 2/3 grown beanstalk (after bonemeal)
    The beanstalk base is full grown, but the top is still hidden and a custom beanstalk top is drawn
  * Fully grown beanstalk (after water droplet)  
* Tilemap modification for dynamic world changes
* Multiple level transitions (top and bottom portals)

---

### Camera System

* Camera follows the player
* Clamped scrolling within level bounds
* Stable transitions between levels and home base

---

### Background & Visuals

* **Layered background system (Mode 0)**

  * Foreground gameplay layer  
  * Separate cloud background layer  

* **Animated cloud system**

  * Smooth horizontal looping  
  * Independent from camera movement (work in progress)

* Tile-based environments with consistent palette usage

---

### HUD & UI

* Dynamic HUD displaying:

  * Current level/map
  * Inventory  
  * Cheat status (when enabled)  

* Clean HUD clearing and redraw system
* Text rendering using tilemap font system

---

### Inventory System

* Tracks collected items (bean sprout, bonemeal, water)
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
* **Invincibility cheat** (player cannot die)  
* Debug tools for testing progression (instant growth behavior)
  Note: not implemented yet, but will be
* Visual indicator when cheats are enabled in the HUD

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

## Progress & Milestones

### Milestone One COMPLETE

* Core gameplay loop implemented
* Player movement and collision system working
* Tileset and Tilemaps working in game
* Player sprite properly displayed, and animated (although animation frames have not been completed)
* Level transitions somewhat functional

---

### Milestone Two IN PROGRESS

* Resource items (sprites) properly added in game
* Tile modification for beanstalk at runtime implement correctly and smoothly
* Parallax background with animated clouds
* Updated collision checking

#### Need to Fix

* Prevent cloud background from vertically following camera

#### TODO

**Sprites / Animation**
* Complete player sprite sheet animations
* Add enemy bee sprite (animation already created)

**Daytime Cycle**
* Add sun sprite
* Move sun across screen on timer
* (Stretch) Smooth animated daylight cycle
* Palette changes for sky color

---

### Milestone Three TBD

---

## Final Requirements / Planned Features

### Sprites & Animation

* Finalize player animation
* Add and animate enemy bee
* Add sun sprite and animation synced with daytime cycle

---

### Levels

* Add moving platforms in Level 1 and Level 2

---

### Daytime Cycle

* Full palette transitions:
  * Early day → Day → Sunset → Night  
* Timer-based cycle system

---

### Sound

* Background music (digital sound)
* Action sound effects (digital sound)

---

### Cheats

* Add resource cheat:
  * Instantly grant next required resource
  * Enables full game completion in seconds for testing

---

### Polish / Nice-to-Haves

* Themed “sign-style” UI screens:
  * Start  
  * Instructions  
  * Pause  
  * Win / Lose  

* Directional signs in levels for guidance

---

## Known Issues

* Cloud background incorrectly follows camera vertically (fix in progress)

---

## Credits

Developed as part of a **Game Boy Advance programming project** using the devkitARM toolchain and mGBA emulator.