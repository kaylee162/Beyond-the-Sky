# Beyond the Sky

## Overview
Beyond the Sky is a Game Boy Advance platformer built using Mode 0. The game blends a calm, grounded home environment with a vertical, side-scrolling sky level where players collect resources to grow a magical beanstalk.

The core experience focuses on smooth movement, light platforming, and a relaxing progression loop. As the player gathers resources and returns them home, the beanstalk grows, unlocking new vertical areas and challenges.

---

## Controls

- **D-Pad Left/Right** → Move
- **A** → Jump
- **UP / DOWN** → Climb beanstalk (ladder mechanic)
- **START** → Pause / Resume
- **SELECT** → Toggle debug cheats

---

## Features

### Game States
- Start Screen
- Instructions Screen
- Home Level (hub area)
- Sky Level (scrolling platforming level)
- Pause Screen
- Win Screen
- Lose Screen

---

### Core Gameplay Mechanics
- Smooth player movement with gravity and jump physics
- Ladder climbing system using the beanstalk
- Resource collection in the sky level
- Deposit system at the home soil patch
- Dynamic beanstalk growth tied to player progression
- Hazard interactions that reset player state
- Win/Lose conditions based on progression and hazards

---

### Level Design
- **Home Level**
  - Central hub for depositing resources
  - Beanstalk growth visualization
  - Safe exploration space

- **Game Levels**
  - Side-scrolling camera system
  - Floating platforms and collectibles
  - Increasing vertical progression
  - Hazard placement for light challenge

---

### Camera & Movement Systems
- Smooth horizontal camera tracking
- World bounds and edge constraints
- Player-centered scrolling in sky level

---

### Visual Systems
- Mode 0 tile-based rendering
- Multiple background layers:
  - Gameplay layer
  - HUD layer
  - Parallax background (clouds)
- Animated or dynamic visual elements
- Custom font rendering for HUD/text

---

### UI / HUD
- On-screen text rendering system
- Resource tracking display
- Clear feedback for game states (pause, win, lose)

---

### Debug / Cheat Features To Come
- Toggleable debug mode
- Instant beanstalk growth
- Used for rapid testing and progression validation

---

## Gameplay Loop

1. Start in the **Home Level**
2. Travel into **Level One**
3. Collect resources
4. Avoid hazards while platforming
5. Return to the **Home Level**
6. Deposit resources into the soil
7. Grow the beanstalk
9. Climb higher to Level Two and progress to the top
10. Win the Game by getting to the top of the beanstalk

---

## Technical Details

- Built using **Mode 0** (tile-based rendering)
- Multiple background layers for parallax and HUD
- Tilemap + collision map system
- Camera system with smooth scrolling and bounds
- Sprite handling via OAM
- Modular state machine for game flow
- Written in **C** using the **devkitARM toolchain**
- Based on and extended from course engine architecture

---

## Project Structure

- `main.c`  
  Main game loop and state machine

- `game.c / game.h`  
  Core gameplay systems and logic

- `levels.c / levels.h`  
  Level data and initialization

- `mode0.c / mode0.h`  
  Background and tile rendering helpers

- `sprites.c / sprites.h`  
  Sprite/OAM management

- `gba.c / gba.h`  
  Hardware abstraction layer

- `font.c / font.h`  
  Text rendering system

---

## Highlights / Improvements

- Expanded gameplay beyond initial milestone
- Fully integrated progression system (beanstalk growth)
- Improved camera and movement feel
- More complete UI and player feedback
- Cleaner separation of systems (rendering, logic, states)

---

## Future Improvements

- Additional resource types with unique effects
- More hazards (enemies, wind zones, falling objects)
- Expanded vertical level design
- Enhanced animations (player, environment, collectibles)
- Sound effects and music integration
- More polished art and tilemaps

---

## Notes

- Developed as part of Georgia Tech CS 2261
- Focused on balancing simplicity with polish
- Designed to demonstrate core GBA systems:
  - Rendering
  - Input handling
  - State management
  - Collision and physics

---

## Credits

**Developed by:**  
Kaylee & Henry  

Georgia Institute of Technology  
CS 2261 - Media Device Architecture
