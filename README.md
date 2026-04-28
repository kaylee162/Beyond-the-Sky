# Beyond the Sky

## Overview

**Beyond the Sky** is a Game Boy Advance platformer built in **Mode 0** using tilemaps, sprites, DMA, collision maps, and Direct Sound.

The player explores a home base, gathers resources across two sky levels, upgrades a magical beanstalk, and climbs toward the castle above the clouds.

---

## Controls

- **D-Pad Left / Right** → Move  
- **D-Pad Up** → Jump  
- **A Button** → Climb ladders / beanstalk  
- **START** → Pause / Resume  
- **LEFT / RIGHT** (Instructions) → Change pages  
- **SELECT + B** → Toggle cheat mode  
- **SELECT + A** → Grant next required resource (debug)

---

## Core Gameplay Loop

1. Start in the **Home Base**
2. Collect the **Bean Sprout**
3. Plant it in the farmland
4. Travel to **Level 1** and collect **Bonemeal**
5. Return home and upgrade the beanstalk
6. Travel to **Level 2** and collect **Water**
7. Return home and fully grow the beanstalk
8. Climb to the castle and win

---

## Game States

- Start Screen  
- Instructions Screen  
- Home Base  
- Level 1  
- Level 2  
- Pause Screen  
- Win Screen  
- Lose Screen  

State transitions are managed through a modular state system in `game_states.c`.

---

## Major Features

## Player Movement

- Smooth left / right movement
- Gravity and jumping
- Ladder / beanstalk climbing
- Improved ladder-top exit behavior
- Safe respawn handling after hazards

## World Progression

Multi-stage beanstalk growth using runtime tilemap edits:

1. Empty farmland  
2. Seed planted  
3. Partial beanstalk growth  
4. Full beanstalk growth  

## Camera System

- Side-scrolling camera in Level 1 and Level 2
- Camera follows player position
- Scroll clamped to map bounds
- Stable transitions between maps

## Background Effects

- Multi-layer tile backgrounds
- Animated clouds with parallax movement
- Runtime palette day/night cycle:
  - Morning
  - Day
  - Sunset
  - Night

## HUD / UI

Top HUD displays:

- Current area
- Inventory
- Cheat mode status

Uses a dedicated font layer and HUD panel for readability.

## Enemies

- Animated bee enemy
- Patrol movement
- Collision damage

## Audio

Implemented with GBA Direct Sound:

- Looping background music
- Planting / deposit sound effect
- Damage sound effect

Files handled through:

- `sound.c`
- `sound.h`
- `background_music.*`
- `planting.*`
- `ouch.*`

---

## Technical Highlights

- **Mode 0 tiled rendering**
- Multiple background layers
- Sprite animation system
- DMA memory transfers
- Collision maps
- Palette editing at runtime
- Modular game architecture
- Direct Sound using timers + FIFO + DMA

---

## Updated File Structure

## Core Game Logic

- `main.c` – program entry point  
- `game.c` – main game loop / state dispatch  
- `game.h` – shared structs, constants, globals  
- `game_internal.h` – internal prototypes  

## Gameplay Modules

- `game_states.c` – state transitions  
- `game_gameplay.c` – runtime gameplay logic  
- `game_home.c` – home base progression logic  
- `game_entities.c` – player / enemies / items  
- `game_collision.c` – movement + collision checks  
- `game_render.c` – HUD / sprites / visual rendering  
- `game_system.c` – shared systems / helpers  

## Assets

- Tilemaps, collision maps, palettes, spritesheets
- Music and sound effect data converted to C arrays

---

## Progress & Milestones

### Milestone One COMPLETE

* Core gameplay loop implemented
* Player movement and collision system working
* Tileset and Tilemaps working in game
* Player sprite properly displayed, and animated (although animation frames have not been completed)
* Level transitions somewhat functional

---

### Milestone Two COMPLETE

* Resource items (sprites: bonemeal & droplet) properly added in game
* Deposit logic for the bonemeal and water droplet working perfectly with tile modification
* Tile modification for beanstalk at runtime implement correctly and smoothly
* Updated collison map logic to go with the tile modification
* Parallax background with animated clouds
* Updated collision checking for player & platforms
* Cleaner Instructions pages and logic
* Smooth animated daylight cycle (palette modification at runtime)
* Get instant resource cheat
* Updated Spritesheet with player animations
* Added bee sprite to the game with animations
* Enemy game logic with the bee sprite

---

### Milestone Three COMPLETE

* Directional signs in levels for guidance for the player
* Themed UI screens for Start, Instructions, Pause & Win / Lose  
* Refactored the codebase to clean up game.c & organize the code better
* Added a castle at the top of the level
* If you collect a resource, and die inside a level, you lose the resource and have to complete the level again. 
* Added a cozy looping background music
* Added some action sounds for planting and damage effects

---

## Known Issues

- Sound effects on channel B are a bit out of sync with actual actions. 

-- 

## Credits

Developed as part of a **Game Boy Advance programming project** using:

- mGBA Emulator  
- Tiled
- Usenti  
- Audacity  

Music / sound sources used from royalty-free and converted assets