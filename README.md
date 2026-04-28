# Beyond the Sky  
**CS 2261 Final Project**  

A polished retro platforming / progression game built for the Game Boy Advance in **Mode 0** using tiled backgrounds, sprites, collision maps, sound, runtime tilemap changes, and palette effects.

---

# Game Overview

You begin at your home farm with a small bean sprout. To grow it into a giant beanstalk and reach the castle in the sky, you must explore two platforming levels and recover the resources needed for growth.

- **Level 1:** Retrieve **Bonemeal**
- **Level 2:** Retrieve **Water**
- Bring each item back home and deposit them (button B) on the farmland to grow your stalk
- Climb the completed beanstalk
- Reach the castle to win

The game combines exploration, movement, hazards, enemy avoidance, progression, and visual world changes.

---

# Controls

| Input | Action |
|------|--------|
| D-Pad Left / Right | Move |
| D-Pad Up | Jump / Climb Up |
| D-Pad Down | Climb Down |
| START | Pause |
| A (menus) | Confirm |
| B (menus) | Back |
| SELECT + A | Cheat: grants next needed resource |
| SELECT + B | Debug Feature: invincibility mode |

---

# Final Project Rubric Checklist

## 1. At Least 4 Unique Sprites (2 Animated)

Implemented **5 unique sprites**:

- Player  
- Bee Enemy  
- Bean Sprout  
- Bonemeal  
- Water Droplet  

### Animated Sprites
- **Player** uses walking animation frames  
- **Bee** uses flapping animation frames  

### Additional Motion
- Bonemeal and Water Droplet gently float / bob.

**Code Locations:**  
- `game_entities.c`  
- `game_gameplay.c`  
- `game_render.c`

---

## 2. Two Backgrounds That Move Independently

Implemented layered scrolling backgrounds:

- **BG1:** Moving cloud layer  
- **BG2:** Main world camera scrolling with player movement  

Creates a parallax effect.

**Code Locations:**  
- `game_render.c`  
- `levels.c`

---

## 3. Modify Tilemaps / Tiles at Runtime

The home world visually changes as progress is made.

### Runtime Changes:
- Empty farmland  
- Seed planted  
- Partial beanstalk growth  
- Full beanstalk grown  

These are written directly into VRAM tilemap memory during gameplay.

**Code Locations:**  
- `game_home.c`  
- `refreshHomeBeanstalkVisuals()`

---

## 4. Modify Palette at Runtime

The sky color changes during gameplay through a full-time-of-day cycle:

- Early Morning  
- Day  
- Sunset  
- Night  

Palette memory is directly edited at runtime.

**Code Locations:**  
- `game_render.c`  
- `updateDaytimePalette()`

---

## 5. Required States

Implemented all required states:

- START  
- INSTRUCTIONS  
- GAME  
- PAUSE  

Also includes:

- WIN  
- LOSE  

**Code Locations:**  
- `game_states.c`

---

## 6. Win / Lose State + Restart

### Win Condition
Grow the beanstalk, climb it, and reach the castle.

### Lose Condition
Touch hazards or enemy bees.

### Restart Support
Player can return to gameplay without restarting emulator.

**Code Locations:**  
- `game_states.c`

---

## 7. Two Simultaneous Sounds

Implemented GBA Direct Sound:

### Channel A
Looping background music.

### Channel B
Action sound effects:

- Damage
- Planting

Both channels can play simultaneously.

**Code Locations:**  
- `sound.c`  
- `sound.h`

---

## 8. Cheat Mechanic

Press:

**SELECT + A**

Grants the next required progression resource.

Examples:

- Gives Bonemeal first  
- Gives Water second  

This helps progression but does not instantly win.

**Code Locations:**  
- `game_gameplay.c`

---

## 9. Playable Through In-Game Instructions

Instructions state explains:

- Movement  
- Climbing  
- Goal of collecting resources  
- Returning home  
- Winning objective  

No outside explanation needed.

**Code Locations:**  
- `game_states.c`

---

## 10. Bug-Free / Polished Gameplay

Project includes:

- Smooth scrolling camera  
- Stable collisions  
- Ladder / beanstalk climbing  
- Enemy patrol logic  
- Pause system  
- Resource inventory tracking  
- Multiple levels  
- Organized modular codebase

---

# Technical Highlights

- Built entirely in **C**
- GBA **Mode 0 tiled engine**
- Collision map system
- Runtime VRAM editing
- Runtime palette transitions
- Multi-file architecture
- Direct Sound audio engine
- Custom HUD text rendering

---

# File Structure

| File | Purpose |
|------|---------|
| `main.c` | Main loop |
| `game_states.c` | State transitions |
| `game_gameplay.c` | Core gameplay loop |
| `game_entities.c` | Bees / pickups |
| `game_collision.c` | Collision logic |
| `game_home.c` | Home growth system |
| `game_render.c` | HUD / drawing |
| `sound.c` | Audio engine |
| `levels.c` | Map loading |

---

# Why This Project Stands Out

Beyond the Sky focuses on **visible progression**.  
As the player succeeds, the world physically changes, culminating in climbing a giant beanstalk into the sky.

I also created all the visual resources for the game. I made all my tilesets, sprites, and fontTiles by hand. 

---

# Credits

Created for **CS 2261 - Media Device Architecture**  
Georgia Institute of Technology

Graphics, programming, gameplay design, and implementation by Kaylee Henry.
