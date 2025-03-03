# SEGA_AI_Snake
Snake Game for Sega Mega Drive using SGDK 2.00 and AI (Grok and Claude)

![AI-MAZE-ING SNAKE](/res/intro.png)

## Overview
This is an enhanced Snake game for the Sega Mega Drive, built with SGDK 2.00. Titled "AI-MAZE-ING SNAKE," the player navigates a snake through a randomly generated maze, growing by eating red food dots. The game increases in speed and difficulty as the score rises, featuring a custom tiled playfield, 8-bit chiptune music, and sound effects.

## Main Features
1. **Gameplay**: Snake moves via D-pad, grows on eating food (score +10), ends on collision with borders, maze walls, or itself.
2. **Visuals**:
   - **Head**: 32x8 sprite sheet (4 frames: down, right, up, left).
   - **Body**: 16x8 sprite sheet (2 frames: horizontal, vertical).
   - **Food**: Single 8x8 red dot.
   - **Playfield**: Custom sand tile (`sand.png`) background, custom wall tiles (`wall.png`) for borders and maze.
   - **Text**: Dark green text for score, intro, pause, and game-over screens.
   - **Intro**: Custom tilemap from `intro.png` with PAL1.
3. **Audio**: Chiptune melody with dynamic tempo (capped), "chomp" sound, game-over tune with rest, intro tune, toggleable via B button.
4. **Controls**: Start toggles states or pauses; D-pad moves snake; B toggles music in intro.
5. **Technical**: PSG audio, sprite allocation checks, free tile list for O(1) food placement, manual VRAM management for sprites and tiles.

## Updates (Latest)
- Updated title to "AI-MAZE-ING SNAKE" with simplified intro text ("START TO PLAY", "B TO TOGGLE MUSIC").
- Changed text color to dark green (PAL0 index 15) for better contrast with sand background.
- Fixed pause/unpause to restore sand tiles instead of showing plain sand color.
- Added custom wall (`wall.png`) and sand (`sand.png`) tiles, replacing monochrome tiles.
- Optimized food placement with a free tile list for O(1) performance.
- Enhanced game over with snake and food disappearance, tune, and rest before tune.
- Improved intro screen with `intro.png` as an IMAGE resource, new PAL1 palette, and music toggle.
- Removed debug direction text, capped music tempo, optimized collision detection, and fixed `prevStartState` initialization.

## Build and Run Instructions
1. Requires [SGDK 2.00](https://github.com/Stephane-D/SGDK/releases/tag/v2.00).
2. Set the `%GDK%` environment variable to point to the SGDK install directory.
3. Add `%GDK%/bin` to the `%PATH%` environment variable.
4. Build the project from the root directory: `make -f %GDK%/makefile.gen`.
5. Grab `rom.bin` from the `out/` directory and load it in your favorite Sega Mega Drive emulator (e.g., BlastEm).