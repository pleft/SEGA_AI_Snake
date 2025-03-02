# SEGA_AI_Snake
Snake Game for Sega Mega Drive using SGDK 2.00 and AI (Grok and Claude)

![AI Snake](/res/intro.png)

Overview:
This is a classic Snake game implemented for the Sega Mega Drive using the SGDK 2.00 library.
The player controls a snake that grows by eating red food dots, increasing in speed and difficulty.
Features a bordered playfield, 8-bit style chiptune music, and sound effects.

Main Features:
1. Gameplay: Snake moves via D-pad, grows on eating food (score +10), ends on collision.
2. Visuals:
   - Head: 32x8 sprite sheet (4 frames: down, right, up, left).
   - Body: 16x8 sprite sheet (2 frames: horizontal, vertical).
   - Food: Single 8x8 red dot.
   - Intro: Custom tilemap from intro.png with text overlay using PAL1.
3. Audio: Chiptune melody with dynamic tempo (capped), "chomp" sound, game-over tune with rest, intro tune, toggleable.
4. Controls: Start toggles states; D-pad moves snake; B toggles music in intro.
5. Technical: PSG audio, optimized math, manual VRAM management for sprites.

Updates (Latest):
- Removed debug direction text from upper right corner (previously at 30, 0).
- Pause now stops music completely (volume decrease was ineffective).
- Added sprite allocation checks to prevent VDP overflow.
- Optimized food placement with free tile list.
- Capped music tempo for better playback at high speeds.
- Optimized collision detection by skipping head self-check.
- Enhanced game over with snake disappearance, food disappearance, tune, and rest before tune.
- Improved intro screen with custom intro.png as IMAGE resource, new PAL1 palette, new tune, music toggle, and removed animation.
- Fixed prevStartState initialization bug.

## Build and run instructions

1. Requires [SGDK 2.00](https://github.com/Stephane-D/SGDK/releases/tag/v2.00)
2. Setup the `%GDK%` env variable to point the install directory of SGDK
3. Modify the `%path%` env var to include `%GDK%/bin`
4. Build the project from root dir: `make -f %GDK%/makefile.gen`
5. Grab the `rom.bin` from the `out/` dir and load it in your favorite Sega Mega Drive emulator
