# SEGA_AI_Snake
Snake Game for Sega Mega Drive using SGDK 2.00 and AI (Grok and Claude)

Overview:
This is a classic Snake game implemented for the Sega Mega Drive using the SGDK 2.00 library.
The player controls a snake that grows by eating red food dots, increasing in speed and difficulty.
Features a bordered playfield, 8-bit style chiptune music, and sound effects.

Main Features:
1. Gameplay:
- Snake moves in four directions (up, right, down, left) using the D-pad.
- Eating food increases score by 10 and lengthens the snake.
- Game ends on collision with borders or self.
- Speed increases every 50 points (frame delay decreases from 8 to 3).
2. Visuals:
- Separate sprites for snake head (`snake_head_sprite`) and body (`snake_body_sprite`).
- Red food dot (`food_sprite`) spawns randomly within borders.
- Grey borders frame the 38x26 tile playfield (total grid: 40x28).
- Score displayed above the top border at y=0.
- New intro screen with "SNAKE GAME" title and "PRESS START" prompt.
3. Audio:
- Enhanced "chomp" sound on eating (Channel 0: 1000 Hz → 400 Hz, sharp and audible).
- Simple 8-bit style melody loop that sounds like classic arcade games.
- Music tempo increases as snake speed rises; volume kept low to prioritize chomp.
4. Controls:
- Start button begins game from intro screen, pauses/unpauses during gameplay, or restarts when game over.
- "PAUSE" text appears centered when paused.
5. Technical:
- Uses PSG for sound: Channel 0 (eating), 1 (melody), 2 (bass).
- Optimized with integer math, static variables, and minimal global access.

## Build and run instructions

1. Requires [SGDK 2.00](https://github.com/Stephane-D/SGDK/releases/tag/v2.00)
2. Setup the `%GDK%` env variable to point the install directory of SGDK
3. Modify the `%path%` env var to include `%GDK%/bin`
4. Build the project from root dir: `make -f %GDK%/makefile.gen`
5. Grab the `rom.bin` from the `out/` dir and load it in your favorite Sega Mega Drive emulator
