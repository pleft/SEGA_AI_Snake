// Snake Game for Sega Mega Drive using SGDK 2.00
// Modified with new music and intro screen
//
// Overview:
// This is a classic Snake game implemented for the Sega Mega Drive using the SGDK 2.00 library.
// The player controls a snake that grows by eating red food dots, increasing in speed and difficulty.
// Features a bordered playfield, 8-bit style chiptune music, and sound effects.
//
// Main Features:
// 1. Gameplay:
//    - Snake moves in four directions (up, right, down, left) using the D-pad.
//    - Eating food increases score by 10 and lengthens the snake.
//    - Game ends on collision with borders or self.
//    - Speed increases every 50 points (frame delay decreases from 8 to 3).
// 2. Visuals:
//    - Separate sprites for snake head (`snake_head_sprite`) and body (`snake_body_sprite`).
//    - Red food dot (`food_sprite`) spawns randomly within borders.
//    - Grey borders frame the 38x26 tile playfield (total grid: 40x28).
//    - Score displayed above the top border at y=0.
//    - New intro screen with "SNAKE GAME" title and "PRESS START" prompt.
// 3. Audio:
//    - Enhanced "chomp" sound on eating (Channel 0: 1000 Hz → 400 Hz, sharp and audible).
//    - Simple 8-bit style melody loop that sounds like classic arcade games.
//    - Music tempo increases as snake speed rises; volume kept low to prioritize chomp.
// 4. Controls:
//    - Start button begins game from intro screen, pauses/unpauses during gameplay, or restarts when game over.
//    - "PAUSE" text appears centered when paused.
// 5. Technical:
//    - Uses PSG for sound: Channel 0 (eating), 1 (melody), 2 (bass).
//    - Optimized with integer math, static variables, and minimal global access.

#include <genesis.h>        // SGDK library header for Mega Drive functionality
#include "resource.h"       // Generated header for sprite resources (snake_head_sprite, snake_body_sprite, food_sprite)

// Game constants - Defining the game's grid and initial conditions
#define GRID_WIDTH 40          // Width of the game grid (in tiles), including borders
#define GRID_HEIGHT 28         // Height of the game grid (in tiles), including borders
#define SNAKE_START_X 20       // Starting X position of the snake's head (within borders)
#define SNAKE_START_Y 14       // Starting Y position of the snake's head (within borders)
#define SNAKE_START_LENGTH 3   // Initial length of the snake
#define SNAKE_MAX_LENGTH 80    // Maximum snake length (matches VDP sprite limit)
#define INITIAL_DELAY 8        // Initial frame delay for game speed (higher = slower)
#define MIN_DELAY 3            // Minimum frame delay (fastest speed)
#define SNAKE_TILE_SIZE 8      // Size of each sprite tile (8x8 pixels)

// Snake directions - Constants for movement directions
#define DIR_UP 0               // Up direction
#define DIR_RIGHT 1            // Right direction
#define DIR_DOWN 2             // Down direction
#define DIR_LEFT 3             // Left direction

// Game states
#define STATE_INTRO 0          // Intro/title screen
#define STATE_PLAYING 1        // Gameplay
#define STATE_GAMEOVER 2       // Game over

// Music constants - PSG frequencies for notes (approx. values for chiptune feel)
// Simple 8-bit melody notes
#define NOTE_C4  262           // C4 (~262 Hz)
#define NOTE_D4  294           // D4 (~294 Hz)
#define NOTE_E4  330           // E4 (~330 Hz)
#define NOTE_F4  349           // F4 (~349 Hz)
#define NOTE_G4  392           // G4 (~392 Hz)
#define NOTE_A4  440           // A4 (~440 Hz)
#define NOTE_B4  494           // B4 (~494 Hz)
#define NOTE_C5  523           // C5 (~523 Hz)
#define NOTE_E5  659           // E5 (~659 Hz)
#define NOTE_G5  784           // G5 (~784 Hz)
#define NOTE_REST 0            // Rest (silence)
#define MELODY_SIZE 16         // Number of notes in melody loop
#define BASS_SIZE 8            // Number of notes in bass loop

// Game objects - Structure for representing positions
typedef struct {
    s16 x;                     // X coordinate (signed 16-bit integer)
    s16 y;                     // Y coordinate (signed 16-bit integer)
} Point;

// Music note structure - Defines a note's pitch and duration
typedef struct {
    u16 frequency;             // PSG frequency for the note
    u16 baseDuration;          // Base duration in frames at INITIAL_DELAY (scaled by speed)
} Note;

// Game state variables - Global variables tracking game state
static Point snakeBody[SNAKE_MAX_LENGTH]; // Array of snake body segments (position data)
static u16 snakeLength;                   // Current length of the snake
static u16 direction;                     // Current movement direction of the snake
static u16 nextDirection;                 // Next direction (buffered from input)
static Point food;                        // Position of the food dot
static u16 score;                         // Player's score
static u16 gameState;                     // Current game state (intro, playing, game over)
static u16 frameDelay;                    // Delay between updates (controls game speed)
static u16 frameCount;                    // Frame counter for timing updates
static u16 paused;                        // Pause flag (TRUE = game is paused)
static u16 prevStartState;                // Previous state of the Start button (for edge detection)
static u16 introAnimFrame;                // Frame counter for intro screen animations

// Music state variables - Tracking chiptune playback
static Note melody[MELODY_SIZE] = {       // Simple 8-bit style melody (16 notes)
    {NOTE_C4, 8}, {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_C5, 16},  // First phrase
    {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_C5, 16}, {NOTE_REST, 8}, // Second phrase
    {NOTE_A4, 8}, {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_G4, 16},  // Third phrase
    {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_A4, 8}, {NOTE_G4, 16}   // Final phrase
};
static Note bass[BASS_SIZE] = {           // Simple bass line (8 notes)
    {NOTE_C4, 16}, {NOTE_G4/2, 16},       // First measure
    {NOTE_C4, 16}, {NOTE_G4/2, 16},       // Second measure
    {NOTE_A4/2, 16}, {NOTE_E4/2, 16},     // Third measure
    {NOTE_F4/2, 16}, {NOTE_G4/2, 16}      // Fourth measure
};
static u16 melodyIndex = 0;               // Current note index in melody
static u16 bassIndex = 0;                 // Current note index in bass
static u16 melodyCounter = 0;             // Frames remaining for current melody note
static u16 bassCounter = 0;               // Frames remaining for current bass note

// Sprite engine objects - Pointers to sprite instances
static Sprite* spriteHead = NULL;               // Sprite for the snake's head (uses snake_head_sprite)
static Sprite* spriteBody[SNAKE_MAX_LENGTH - 1]; // Array of sprites for body segments (uses snake_body_sprite)
static Sprite* spriteFood = NULL;               // Sprite for the food dot (uses food_sprite)

// Function prototypes - Forward declarations for all static functions
static void initGame(void);         // Initializes or resets the game state
static void showIntroScreen(void);  // Displays and manages the intro screen
static void updateIntroScreen(void); // Updates intro screen animations
static void startGame(void);        // Transitions from intro to gameplay
static void handleInput(void);      // Processes player input from the controller
static void updateGame(void);       // Updates game logic (movement, collisions, food)
static void drawGame(void);         // Renders sprites to their current positions
static void generateFood(void);     // Places a new food dot on the grid
static u16 checkCollision(s16 x, s16 y); // Checks if a position collides with the snake
static void showGameOver(void);     // Displays the game over message
static void playEatSound(void);     // Plays an enhanced sound when food is eaten
static void togglePause(void);      // Toggles pause state and displays/clears "PAUSE" text
static void updateMusic(void);      // Updates chiptune music playback with dynamic tempo

// Main function - Entry point of the program
int main() {
    JOY_init();                     // Initialize joystick input system
    SPR_init();                     // Initialize the sprite engine for rendering
    
    // Set up palette for PAL0 (used by all sprites, text, and borders)
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000)); // Index 0: Black (background)
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0x008000)); // Index 1: Dark green (snake head and body)
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0xFF0000)); // Index 2: Red (food)
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xC0C0C0)); // Index 3: Grey (border)
    PAL_setColor(4, RGB24_TO_VDPCOLOR(0x800000)); // Index 4: Dark red (snake's tongue)
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF)); // Index 15: White (text)
    
    VDP_setTextPalette(PAL0);       // Assign PAL0 to text rendering
    VDP_setTextPriority(1);         // Ensure text renders above sprites
    
    PSG_reset();                    // Reset PSG sound channels to a silent state
    
    showIntroScreen();              // Start with the intro screen
    
    // Main game loop - Runs indefinitely
    while (1) {
        handleInput();              // Process player input

        if (gameState == STATE_INTRO) {
            updateIntroScreen();    // Update intro screen animations
        } else if (gameState == STATE_PLAYING) {
            frameCount++;           // Increment frame counter for timing
            
            if (frameCount >= frameDelay) { // Update game logic at set intervals
                frameCount = 0;     // Reset counter after update
                if (!paused) {      // Only update if game isn't paused
                    updateGame();
                }
            }
            
            drawGame();             // Update sprite positions on screen
        }
        
        updateMusic();              // Update chiptune music playback each frame
        SPR_update();               // Refresh sprite engine to display changes
        SYS_doVBlankProcess();      // Handle VBlank processing (syncs with display refresh)
    }
    
    return 0;                       // Exit code (never reached due to infinite loop)
}

// Displays and manages the intro screen
static void showIntroScreen(void) {
    VDP_clearPlane(BG_A, TRUE);     // Clear the background plane
    
    // Draw the title and instructions
    VDP_setTextPalette(PAL0);
    VDP_drawText("AI SNAKE", 15, 8);
    
    VDP_drawText("MEGA DRIVE EDITION", 11, 10);
    
    VDP_drawText("PRESS START TO PLAY", 11, 16);
    VDP_drawText("USE D-PAD TO MOVE", 12, 18);
    
    // Set up variables for intro animation
    introAnimFrame = 0;
    gameState = STATE_INTRO;
    
    // Initialize music to play at intro tempo
    melodyIndex = 0;
    bassIndex = 0;
    melodyCounter = 0;
    bassCounter = 0;
    
    // Reset game variables
    score = 0;
    paused = FALSE;
    prevStartState = TRUE;          // Set to TRUE to avoid immediate start
}

// Updates intro screen animations
static void updateIntroScreen(void) {
    introAnimFrame++;
    
    // Animate "PRESS START" text by flashing (every 30 frames)
    if (introAnimFrame % 60 < 30) {
        VDP_drawText("PRESS START TO PLAY", 11, 16);
    } else {
        VDP_clearText(11, 16, 19);  // Clear text
    }
    
    // Add a simple snake animation in the background (every 15 frames)
    if (introAnimFrame % 15 == 0) {
        // Clear previous animation frame
        VDP_clearText(10 + (introAnimFrame / 15) % 20, 22, 2);
        
        // Draw new snake segment
        VDP_drawText("O", 11 + (introAnimFrame / 15) % 20, 22);
    }
}

// Transitions from intro to gameplay
static void startGame(void) {
    initGame();
    gameState = STATE_PLAYING;
}

// Initializes or resets the game to starting conditions
static void initGame(void) {
    // Clean up any existing sprites to prevent memory leaks
    if (spriteHead) SPR_releaseSprite(spriteHead);
    for (u16 i = 0; i < SNAKE_MAX_LENGTH - 1; i++) {
        if (spriteBody[i]) SPR_releaseSprite(spriteBody[i]);
        spriteBody[i] = NULL;       // Clear body sprite pointers
    }
    if (spriteFood) SPR_releaseSprite(spriteFood);
    
    VDP_clearPlane(BG_A, TRUE);    // Clear the background plane (removes old graphics)
    
    // Define and load a solid grey tile for the border (palette index 3)
    static const u32 borderTile[8] = { 
        0x33333333, 0x33333333, 0x33333333, 0x33333333,
        0x33333333, 0x33333333, 0x33333333, 0x33333333
    };
    VDP_loadTileData(borderTile, TILE_USER_INDEX, 1, DMA); // Load tile into VRAM
    
    // Draw borders on BG_A using grey (index 3), shifted down one row
    const u16 borderTileAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX);
    for (u16 i = 0; i < GRID_WIDTH; i++) {
        VDP_setTileMapXY(BG_A, borderTileAttr, i, 1);         // Top border (row 1)
        VDP_setTileMapXY(BG_A, borderTileAttr, i, GRID_HEIGHT - 1); // Bottom border (row 27)
    }
    for (u16 i = 2; i < GRID_HEIGHT - 1; i++) {
        VDP_setTileMapXY(BG_A, borderTileAttr, 0, i);         // Left border (column 0)
        VDP_setTileMapXY(BG_A, borderTileAttr, GRID_WIDTH - 1, i); // Right border (column 39)
    }
    
    // Set initial snake position (head at START_X, START_Y, body trailing left)
    snakeLength = SNAKE_START_LENGTH;
    for (u16 i = 0; i < snakeLength; i++) {
        snakeBody[i].x = SNAKE_START_X - i; // Place segments left of head
        snakeBody[i].y = SNAKE_START_Y;
    }
    
    direction = DIR_RIGHT;          // Initial movement direction is right
    nextDirection = DIR_RIGHT;      // Buffer matches current direction
    
    // Create snake head sprite using snake_head_sprite
    spriteHead = SPR_addSprite(&snake_head_sprite, 
                             SNAKE_START_X * SNAKE_TILE_SIZE, 
                             SNAKE_START_Y * SNAKE_TILE_SIZE,
                             TILE_ATTR(PAL0, TRUE, FALSE, FALSE)); // Use PAL0, high priority
    
    // Create initial body sprites using snake_body_sprite
    for (u16 i = 1; i < snakeLength; i++) {
        spriteBody[i-1] = SPR_addSprite(&snake_body_sprite, 
                                      snakeBody[i].x * SNAKE_TILE_SIZE, 
                                      snakeBody[i].y * SNAKE_TILE_SIZE,
                                      TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
    }
    
    // Ensure remaining body slots are NULL (no dangling pointers)
    for (u16 i = snakeLength - 1; i < SNAKE_MAX_LENGTH - 1; i++) {
        spriteBody[i] = NULL;
    }
    
    generateFood();                 // Place the first food dot
    spriteFood = SPR_addSprite(&food_sprite, 
                             food.x * SNAKE_TILE_SIZE, 
                             food.y * SNAKE_TILE_SIZE,
                             TILE_ATTR(PAL0, TRUE, FALSE, FALSE)); // Create food sprite
    
    // Reset game state variables
    score = 0;                      // Starting score
    paused = FALSE;                 // Game starts unpaused
    prevStartState = TRUE;          // Start button is initially pressed (avoids immediate pause)
    frameDelay = INITIAL_DELAY;     // Set initial speed
    frameCount = 0;                 // Reset frame counter
    
    // Reset music state
    melodyIndex = 0;                // Start at first melody note
    bassIndex = 0;                  // Start at first bass note
    melodyCounter = 0;              // Reset melody note duration
    bassCounter = 0;                // Reset bass note duration
    
    // Display initial score at y=0 (above border at y=1)
    char scoreText[20];
    sprintf(scoreText, "SCORE: %d", score);
    VDP_drawText(scoreText, 1, 0);  // Place score at top row
}

// Handles player input from the controller
static void handleInput(void) {
    const u16 joy = JOY_readJoypad(JOY_1); // Read state of Joypad 1
    const u16 startPressed = joy & BUTTON_START; // Check current state of Start button
    
    // Detect Start button press (transition from not pressed to pressed)
    if (startPressed && !prevStartState) {
        if (gameState == STATE_INTRO) {
            startGame();            // Start the game from intro screen
        } else if (gameState == STATE_PLAYING) {
            togglePause();          // Toggle pause during gameplay
        } else if (gameState == STATE_GAMEOVER) {
            showIntroScreen();      // Return to intro screen after game over
        }
    }
    prevStartState = startPressed;  // Update previous state for next frame
    
    // Process movement only if game is active and not paused
    if (gameState == STATE_PLAYING && !paused) {
        if (joy & BUTTON_UP && direction != DIR_DOWN) nextDirection = DIR_UP;
        else if (joy & BUTTON_RIGHT && direction != DIR_LEFT) nextDirection = DIR_RIGHT;
        else if (joy & BUTTON_DOWN && direction != DIR_UP) nextDirection = DIR_DOWN;
        else if (joy & BUTTON_LEFT && direction != DIR_RIGHT) nextDirection = DIR_LEFT;
    }
}

// Updates game logic: movement, collisions, and food consumption
static void updateGame(void) {
    s16 newHeadX = snakeBody[0].x;  // Calculate new head X position
    s16 newHeadY = snakeBody[0].y;  // Calculate new head Y position
    
    direction = nextDirection;      // Apply the buffered direction
    switch (direction) {            // Move head based on current direction
        case DIR_UP:    newHeadY--; break;
        case DIR_RIGHT: newHeadX++; break;
        case DIR_DOWN:  newHeadY++; break;
        case DIR_LEFT:  newHeadX--; break;
    }
    
    // Check for wall collisions (against borders at 0, 39, 1, 26)
    if (newHeadX <= 0 || newHeadX >= GRID_WIDTH - 1 || newHeadY <= 1 || newHeadY >= GRID_HEIGHT - 1) {
        gameState = STATE_GAMEOVER;
        showGameOver();
        return;
    }
    
    // Check for self-collision (head hits body)
    if (checkCollision(newHeadX, newHeadY)) {
        gameState = STATE_GAMEOVER;
        showGameOver();
        return;
    }
    
    // Check if snake eats food (head overlaps food position)
    if (newHeadX == food.x && newHeadY == food.y) {
        if (snakeLength < SNAKE_MAX_LENGTH) { // Grow snake if not at max length
            for (u16 i = snakeLength; i > 0; i--) {
                snakeBody[i] = snakeBody[i - 1]; // Shift body segments forward
            }
            if (snakeLength > 1 && !spriteBody[snakeLength-2]) {
                // Add new body sprite off-screen initially
                spriteBody[snakeLength-2] = SPR_addSprite(&snake_body_sprite, 
                                                        -16, -16,
                                                        TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
            }
            snakeLength++;         // Increase snake length
        } else {                   // At max length, just shift body (no growth)
            for (u16 i = snakeLength - 1; i > 0; i--) {
                snakeBody[i] = snakeBody[i - 1];
            }
        }
        
        playEatSound();            // Play enhanced eating sound
        
        score += 10;               // Add 10 points to score
        char scoreText[20];
        sprintf(scoreText, "SCORE: %d", score);
        VDP_clearText(1, 0, 20);   // Clear previous score text at y=0
        VDP_drawText(scoreText, 1, 0); // Display updated score above border
        
        generateFood();            // Place new food dot
        SPR_setPosition(spriteFood, 
                       food.x * SNAKE_TILE_SIZE, 
                       food.y * SNAKE_TILE_SIZE); // Update food sprite position
        
        // Increase game speed every 50 points (decrease delay)
        if (score % 50 == 0 && frameDelay > MIN_DELAY) {
            frameDelay--;
        }
    } else {                       // No food eaten, just move snake forward
        for (u16 i = snakeLength - 1; i > 0; i--) {
            snakeBody[i] = snakeBody[i - 1]; // Shift body segments
        }
    }
    
    // Update head's new position
    snakeBody[0].x = newHeadX;
    snakeBody[0].y = newHeadY;
}

// Updates sprite positions on the screen
static void drawGame(void) {
    // Update head sprite position
    SPR_setPosition(spriteHead, 
                   snakeBody[0].x * SNAKE_TILE_SIZE, 
                   snakeBody[0].y * SNAKE_TILE_SIZE);
    
    // Update body sprite positions
    for (u16 i = 1; i < snakeLength; i++) {
        if (spriteBody[i-1]) {      // Check if sprite exists
            SPR_setPosition(spriteBody[i-1], 
                           snakeBody[i].x * SNAKE_TILE_SIZE, 
                           snakeBody[i].y * SNAKE_TILE_SIZE);
        }
    }
}

// Generates a new food position within the bordered playfield
static void generateFood(void) {
    u16 validPosition = FALSE;      // Flag to track if position is valid
    u16 attempts = 0;               // Counter to limit random attempts
    
    // Try to place food randomly inside borders (1 to 38 for x, 2 to 26 for y)
    while (!validPosition && attempts < 100) {
        food.x = (random() % (GRID_WIDTH - 2)) + 1;  // X from 1 to 38
        food.y = (random() % (GRID_HEIGHT - 3)) + 2; // Y from 2 to 26 (below border at y=1)
        validPosition = !checkCollision(food.x, food.y); // Check if position is free
        attempts++;
    }
    
    // Fallback: Place food near center if random placement fails
    if (!validPosition) {
        food.x = GRID_WIDTH / 2;
        food.y = GRID_HEIGHT / 2;
        if (checkCollision(food.x, food.y)) { // If center is occupied
            // Scan playfield for an empty spot inside borders
            for (food.x = 1; food.x < GRID_WIDTH - 1; food.x++) {
                for (food.y = 2; food.y < GRID_HEIGHT - 1; food.y++) {
                    if (!checkCollision(food.x, food.y)) {
                        return; // Found a spot, exit
                    }
                }
            }
        }
    }
}

// Checks if a position collides with the snake's body
static u16 checkCollision(s16 x, s16 y) {
    for (u16 i = 0; i < snakeLength; i++) {
        if (snakeBody[i].x == x && snakeBody[i].y == y) {
            return TRUE;            // Collision detected
        }
    }
    return FALSE;                   // No collision
}

// Displays game over message and silences music
static void showGameOver(void) {
    VDP_drawText("GAME OVER", 15, 10);
    VDP_drawText("PRESS START TO PLAY AGAIN", 8, 12);
    VDP_drawText("FINAL SCORE:", 14, 14);
    
    char scoreText[5];
    sprintf(scoreText, "%d", score);
    VDP_drawText(scoreText, 19 - (score >= 10 ? (score >= 100 ? (score >= 1000 ? 3 : 2) : 1) : 0), 16);
    
    // Silence music channels when game ends
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
    PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
}

// Plays an enhanced "chomp" sound when the snake eats food (Channel 0)
static void playEatSound(void) {
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX); // Full volume for sharp "bite"
    PSG_setFrequency(0, 1000);            // Higher pitch (~1000 Hz) for clarity
    waitMs(20);                           // Play for 20ms
    
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX / 2); // Half volume for "gulp"
    PSG_setFrequency(0, 400);             // Lower pitch (~400 Hz)
    waitMs(30);                           // Play for 30ms
    
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN); // Mute to end the sound
}

// Updates chiptune music playback with tempo tied to game speed (Channels 1 and 2)
static void updateMusic(void) {
    if (gameState == STATE_GAMEOVER) { // Stop music if game is over
        PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
        PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
        return;
    }
    
    if (gameState == STATE_PLAYING && paused) { // Quieter music if paused
        PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
        PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
        return;
    }
    
    // Calculate tempo factor - different for intro and gameplay
    u16 tempoFactor;
    if (gameState == STATE_INTRO) {
        tempoFactor = 12;  // Slower on intro screen
    } else {
        // Calculate based on frameDelay (scales from 8 to 3)
        const u16 delay = frameDelay;
        tempoFactor = (delay * 10) / INITIAL_DELAY; // Integer scaling (10-3.75)
    }
    
    const u16 melodyDuration = (melody[melodyIndex].baseDuration * tempoFactor) / 10;
    const u16 bassDuration = (bass[bassIndex].baseDuration * tempoFactor) / 10;
    
    // Update melody (Channel 1)
    if (melodyCounter == 0) {       // Time to play the next note
        PSG_setFrequency(1, melody[melodyIndex].frequency);
        PSG_setEnvelope(1, melody[melodyIndex].frequency != NOTE_REST ? 
                        PSG_ENVELOPE_MAX / 8 : PSG_ENVELOPE_MIN); // Low volume
        melodyCounter = melodyDuration; // Set dynamic duration
        melodyIndex = (melodyIndex + 1) % MELODY_SIZE; // Loop through melody
    }
    melodyCounter--;                // Decrease counter each frame
    
    // Update bass (Channel 2)
    if (bassCounter == 0) {         // Time to play the next note
        PSG_setFrequency(2, bass[bassIndex].frequency);
        PSG_setEnvelope(2, bass[bassIndex].frequency != NOTE_REST ? 
                        PSG_ENVELOPE_MAX / 16 : PSG_ENVELOPE_MIN); // Even lower volume
        bassCounter = bassDuration;   // Set dynamic duration
        bassIndex = (bassIndex + 1) % BASS_SIZE; // Loop through bass
    }
    bassCounter--;                  // Decrease counter each frame
}

// Toggles pause state and displays/clears "PAUSE" text
static void togglePause(void) {
    paused = !paused;               // Toggle pause state (TRUE <-> FALSE)
    if (paused) {
        VDP_drawText("PAUSE", 17, 14); // Display "PAUSE" in center (40/2 - 5/2 = 17, 28/2 = 14)
    } else {
        VDP_clearText(17, 14, 5);      // Clear "PAUSE" text (5 characters wide)
    }
}