// Snake Game for Sega Mega Drive using SGDK 2.00
// Modified with new music, intro screen, multi-frame sprites, and enhancements as of March 02, 2025
//
// Overview:
// Classic Snake game where the player controls a snake that grows by eating food, featuring a bordered
// playfield, chiptune music, and sound effects. Updated with multi-frame sprites, pause fixes, and optimizations.
//
// Main Features:
// 1. Gameplay: Snake moves via D-pad, grows on eating food (score +10), ends on collision.
// 2. Visuals:
//    - Head: 32x8 sprite sheet (4 frames: down, right, up, left).
//    - Body: 16x8 sprite sheet (2 frames: horizontal, vertical).
//    - Food: Single 8x8 red dot.
// 3. Audio: Chiptune melody with dynamic tempo (capped), "chomp" sound, game-over tune, stops when paused.
// 4. Controls: Start toggles states; D-pad moves snake.
// 5. Technical: PSG audio, optimized math, manual VRAM management for sprites.
//
// Updates (Latest):
// - Removed debug direction text from upper right corner (previously at 30, 0).
// - Pause now stops music completely (volume decrease was ineffective).
// - Added sprite allocation checks to prevent VDP overflow.
// - Optimized food placement with free tile list.
// - Capped music tempo for better playback at high speeds.
// - Optimized collision detection by skipping head self-check.
// - Enhanced game over with snake disappearance, food disappearance, and tune.
// - Fixed prevStartState initialization bug.

#include <genesis.h>        // SGDK library header
#include "resource.h"       // Generated sprite resources

// Game constants
#define GRID_WIDTH 40          // Total grid width in tiles (including borders)
#define GRID_HEIGHT 28         // Total grid height in tiles (including borders)
#define SNAKE_START_X 20       // Snake head’s starting X
#define SNAKE_START_Y 14       // Snake head’s starting Y
#define SNAKE_START_LENGTH 3   // Initial snake length
#define SNAKE_MAX_LENGTH 80    // Max snake length (VDP sprite limit)
#define INITIAL_DELAY 8        // Initial frame delay (slower)
#define MIN_DELAY 3            // Minimum frame delay (faster)
#define SNAKE_TILE_SIZE 8      // Sprite tile size (8x8 pixels)
#define MAX_TEMPO_FACTOR 6     // Minimum tempo factor to cap music speed

// Snake directions
#define DIR_UP 0               // Up (head frame 2)
#define DIR_RIGHT 1            // Right (head frame 1)
#define DIR_DOWN 2             // Down (head frame 0)
#define DIR_LEFT 3             // Left (head frame 3)

// Game states
#define STATE_INTRO 0          // Intro screen
#define STATE_PLAYING 1        // Gameplay
#define STATE_GAMEOVER 2       // Game over

// Music constants
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
#define NOTE_G3  196           // G3 (~196 Hz, for game-over tune)
#define NOTE_REST 0            // Silence
#define MELODY_SIZE 16         // Melody loop length
#define BASS_SIZE 8            // Bass loop length

// Game objects
typedef struct {
    s16 x;                     // X position in tiles
    s16 y;                     // Y position in tiles
} Point;

// Music note structure
typedef struct {
    u16 frequency;             // PSG frequency (Hz)
    u16 baseDuration;          // Base duration in frames
} Note;

// Game state variables
static Point snakeBody[SNAKE_MAX_LENGTH]; // Snake segments (head at [0])
static u16 snakeLength;                   // Current length
static u16 direction;                     // Current direction
static u16 nextDirection;                 // Buffered direction
static Point food;                        // Food position
static u16 score;                         // Player score
static u16 gameState;                     // Current state
static u16 frameDelay;                    // Frames between updates
static u16 frameCount;                    // Frame counter
static u16 paused;                        // Pause flag
static u16 prevStartState;                // Start button state
static u16 introAnimFrame;                // Intro animation counter

// Music state variables
static Note melody[MELODY_SIZE] = {
    {NOTE_C4, 8}, {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_C5, 16},
    {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_C5, 16}, {NOTE_REST, 8},
    {NOTE_A4, 8}, {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_G4, 16},
    {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_A4, 8}, {NOTE_G4, 16}
};
static Note bass[BASS_SIZE] = {
    {NOTE_C4, 16}, {NOTE_G4/2, 16},
    {NOTE_C4, 16}, {NOTE_G4/2, 16},
    {NOTE_A4/2, 16}, {NOTE_E4/2, 16},
    {NOTE_F4/2, 16}, {NOTE_G4/2, 16}
};
static u16 melodyIndex = 0;
static u16 bassIndex = 0;
static u16 melodyCounter = 0;
static u16 bassCounter = 0;

// Sprite engine objects
static Sprite* spriteHead = NULL;               // Head sprite (4 frames)
static Sprite* spriteBody[SNAKE_MAX_LENGTH - 1]; // Body sprites (2 frames)
static Sprite* spriteFood = NULL;               // Food sprite
static u16 headVramIndexes[4];                  // VRAM indices for head frames
static u16 bodyVramIndexes[2];                  // VRAM indices for body frames

// Function prototypes
static void initGame(void);
static void showIntroScreen(void);
static void updateIntroScreen(void);
static void startGame(void);
static void handleInput(void);
static void updateGame(void);
static void drawGame(void);
static void generateFood(void);
static u16 checkCollision(s16 x, s16 y);
static void showGameOver(void);
static void playEatSound(void);
static void togglePause(void);
static void updateMusic(void);

// Main function: Runs the game loop
int main() {
    JOY_init();
    SPR_init();
    
    // Set up palette
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000)); // Black
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0x008000)); // Dark green
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0xFF0000)); // Red
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xC0C0C0)); // Grey
    PAL_setColor(4, RGB24_TO_VDPCOLOR(0x800000)); // Dark red
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF)); // White
    
    VDP_setTextPalette(PAL0);
    VDP_setTextPriority(1);
    
    PSG_reset();
    
    showIntroScreen();
    
    while (1) {
        handleInput();
        if (gameState == STATE_INTRO) {
            updateIntroScreen();
            SPR_update(); // Still update sprites for intro animation if any
        }
        else if (gameState == STATE_PLAYING) {
            frameCount++;
            if (frameCount >= frameDelay && !paused) {
                frameCount = 0;
                updateGame();
            }
            drawGame();    // Only draw during playing state
            SPR_update();  // Only update sprites during playing state
        }
        // In STATE_GAMEOVER, sprites are managed by showGameOver(), so skip drawGame/SPR_update
        updateMusic();
        SYS_doVBlankProcess();
    }
    
    return 0;
}

// Initializes game state and sprites
static void initGame(void) {
    // Clean up existing sprites
    if (spriteHead) SPR_releaseSprite(spriteHead);
    for (u16 i = 0; i < SNAKE_MAX_LENGTH - 1; i++) {
        if (spriteBody[i]) SPR_releaseSprite(spriteBody[i]);
        spriteBody[i] = NULL;
    }
    if (spriteFood) SPR_releaseSprite(spriteFood);
    
    VDP_clearPlane(BG_A, TRUE);
    
    // Draw border
    static const u32 borderTile[8] = { 
        0x33333333, 0x33333333, 0x33333333, 0x33333333,
        0x33333333, 0x33333333, 0x33333333, 0x33333333
    };
    VDP_loadTileData(borderTile, TILE_USER_INDEX, 1, DMA);
    u16 vramIndex = TILE_USER_INDEX + 1;
    
    const u16 borderTileAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX);
    for (u16 i = 0; i < GRID_WIDTH; i++) {
        VDP_setTileMapXY(BG_A, borderTileAttr, i, 1);
        VDP_setTileMapXY(BG_A, borderTileAttr, i, GRID_HEIGHT - 1);
    }
    for (u16 i = 2; i < GRID_HEIGHT - 1; i++) {
        VDP_setTileMapXY(BG_A, borderTileAttr, 0, i);
        VDP_setTileMapXY(BG_A, borderTileAttr, GRID_WIDTH - 1, i);
    }
    
    // Load head frames (32x8, 4x 8x8)
    Animation* headAnim = snake_head_sprite.animations[0];
    for (u16 i = 0; i < 4; i++) {
        TileSet* tileset = headAnim->frames[i]->tileset;
        VDP_loadTileSet(tileset, vramIndex, DMA);
        headVramIndexes[i] = vramIndex;
        vramIndex += tileset->numTile;
    }
    
    // Load body frames (16x8, 2x 8x8)
    Animation* bodyAnim = snake_body_sprite.animations[0];
    for (u16 i = 0; i < 2; i++) { // 0=horizontal, 1=vertical
        TileSet* tileset = bodyAnim->frames[i]->tileset;
        VDP_loadTileSet(tileset, vramIndex, DMA);
        bodyVramIndexes[i] = vramIndex;
        vramIndex += tileset->numTile;
    }
    
    // Initialize snake
    snakeLength = SNAKE_START_LENGTH;
    for (u16 i = 0; i < snakeLength; i++) {
        snakeBody[i].x = SNAKE_START_X - i;
        snakeBody[i].y = SNAKE_START_Y;
    }
    
    direction = DIR_RIGHT;
    nextDirection = DIR_RIGHT;
    
    // Create head sprite
    spriteHead = SPR_addSprite(&snake_head_sprite, 
                              SNAKE_START_X * SNAKE_TILE_SIZE, 
                              SNAKE_START_Y * SNAKE_TILE_SIZE,
                              TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
    SPR_setAutoTileUpload(spriteHead, FALSE);
    SPR_setFrame(spriteHead, 1); // Right
    SPR_setVRAMTileIndex(spriteHead, headVramIndexes[1]);
    
    // Create initial body sprites
    for (u16 i = 1; i < snakeLength; i++) {
        spriteBody[i-1] = SPR_addSprite(&snake_body_sprite, 
                                       snakeBody[i].x * SNAKE_TILE_SIZE, 
                                       snakeBody[i].y * SNAKE_TILE_SIZE,
                                       TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
        if (!spriteBody[i-1]) {
            snakeLength = i; // Cap length if sprite allocation fails
            break;
        }
        SPR_setAutoTileUpload(spriteBody[i-1], FALSE);
        SPR_setFrame(spriteBody[i-1], 0); // Horizontal initially
        SPR_setVRAMTileIndex(spriteBody[i-1], bodyVramIndexes[0]);
    }
    for (u16 i = snakeLength - 1; i < SNAKE_MAX_LENGTH - 1; i++) {
        spriteBody[i] = NULL;
    }
    
    // Initialize food and other game state
    generateFood();
    spriteFood = SPR_addSprite(&food_sprite, 
                              food.x * SNAKE_TILE_SIZE, 
                              food.y * SNAKE_TILE_SIZE,
                              TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
    
    score = 0;
    paused = FALSE;
    prevStartState = FALSE; // [FIX] Start unpressed to avoid skipping first toggle
    frameDelay = INITIAL_DELAY;
    frameCount = 0;
    
    melodyIndex = 0;
    bassIndex = 0;
    melodyCounter = 0;
    bassCounter = 0;
    
    char scoreText[20];
    sprintf(scoreText, "SCORE: %4d", score);
    VDP_drawText(scoreText, 1, 0);
}

// Displays intro screen
static void showIntroScreen(void) {
    VDP_clearPlane(BG_A, TRUE);
    VDP_drawText("AI SNAKE", 15, 8);
    VDP_drawText("MEGA DRIVE EDITION", 11, 10);
    VDP_drawText("PRESS START TO PLAY", 11, 16);
    VDP_drawText("USE D-PAD TO MOVE", 12, 18);
    
    introAnimFrame = 0;
    gameState = STATE_INTRO;
    
    melodyIndex = 0;
    bassIndex = 0;
    melodyCounter = 0;
    bassCounter = 0;
    
    score = 0;
    paused = FALSE;
    prevStartState = FALSE; // [FIX] Consistent with initGame
}

// Updates intro animations
static void updateIntroScreen(void) {
    introAnimFrame++;
    if (introAnimFrame % 60 < 30) {
        VDP_drawText("PRESS START TO PLAY", 11, 16);
    } else {
        VDP_clearText(11, 16, 19);
    }
    
    if (introAnimFrame % 15 == 0) {
        u16 x = (introAnimFrame / 15) % 28;
        u16 y = 22 + ((x % 4) < 2 ? 0 : 1);
        VDP_clearTextArea(10, 22, 20, 2);
        VDP_drawText("O-", 11 + x, y);
    }
}

// Starts gameplay
static void startGame(void) {
    initGame();
    gameState = STATE_PLAYING;
}

// Handles input
static void handleInput(void) {
    const u16 joy = JOY_readJoypad(JOY_1);
    const u16 startPressed = joy & BUTTON_START;
    
    if (startPressed && !prevStartState) {
        if (gameState == STATE_INTRO) startGame();
        else if (gameState == STATE_PLAYING) togglePause();
        else if (gameState == STATE_GAMEOVER) showIntroScreen();
    }
    prevStartState = startPressed;
    
    if (gameState == STATE_PLAYING && !paused) {
        if (joy & BUTTON_UP && direction != DIR_DOWN) nextDirection = DIR_UP;
        else if (joy & BUTTON_RIGHT && direction != DIR_LEFT) nextDirection = DIR_RIGHT;
        else if (joy & BUTTON_DOWN && direction != DIR_UP) nextDirection = DIR_DOWN;
        else if (joy & BUTTON_LEFT && direction != DIR_RIGHT) nextDirection = DIR_LEFT;
    }
}

// Updates game logic
static void updateGame(void) {
    s16 newHeadX = snakeBody[0].x;
    s16 newHeadY = snakeBody[0].y;
    
    direction = nextDirection;
    switch (direction) {
        case DIR_UP:    newHeadY--; break;
        case DIR_RIGHT: newHeadX++; break;
        case DIR_DOWN:  newHeadY++; break;
        case DIR_LEFT:  newHeadX--; break;
    }
    
    // Check wall collision
    if (newHeadX <= 0 || newHeadX >= GRID_WIDTH - 1 || newHeadY <= 1 || newHeadY >= GRID_HEIGHT - 1) {
        gameState = STATE_GAMEOVER;
        showGameOver();
        return;
    }
    
    // Check self-collision
    if (checkCollision(newHeadX, newHeadY)) {
        gameState = STATE_GAMEOVER;
        showGameOver();
        return;
    }
    
    // Handle food collision
    if (newHeadX == food.x && newHeadY == food.y) {
        if (snakeLength < SNAKE_MAX_LENGTH) {
            for (u16 i = snakeLength; i > 0; i--) {
                snakeBody[i] = snakeBody[i - 1];
            }
            if (snakeLength > 1 && !spriteBody[snakeLength-2]) {
                spriteBody[snakeLength-2] = SPR_addSprite(&snake_body_sprite, 
                                                         -16, -16,
                                                         TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
                if (!spriteBody[snakeLength-2]) {
                    snakeLength--; // [NEW] Cap length if allocation fails
                    VDP_drawText("SPRITE LIMIT!", 14, 10);
                    return;
                }
                SPR_setAutoTileUpload(spriteBody[snakeLength-2], FALSE);
                SPR_setFrame(spriteBody[snakeLength-2], 0); // Default horizontal
                SPR_setVRAMTileIndex(spriteBody[snakeLength-2], bodyVramIndexes[0]);
            }
            snakeLength++;
        } else {
            for (u16 i = snakeLength - 1; i > 0; i--) {
                snakeBody[i] = snakeBody[i - 1];
            }
        }
        
        playEatSound();
        score += 10;
        char scoreText[20];
        sprintf(scoreText, "SCORE: %4d", score);
        VDP_clearText(1, 0, 20);
        VDP_drawText(scoreText, 1, 0);
        
        generateFood();
        SPR_setPosition(spriteFood, food.x * SNAKE_TILE_SIZE, food.y * SNAKE_TILE_SIZE);
        
        if (score % 50 == 0 && frameDelay > MIN_DELAY) frameDelay--;
    } else {
        for (u16 i = snakeLength - 1; i > 0; i--) {
            snakeBody[i] = snakeBody[i - 1];
        }
    }
    
    snakeBody[0].x = newHeadX;
    snakeBody[0].y = newHeadY;
}

// Renders sprites with frame selection
static void drawGame(void) {
    SPR_setPosition(spriteHead, snakeBody[0].x * SNAKE_TILE_SIZE, snakeBody[0].y * SNAKE_TILE_SIZE);
    
    // Head frames: 0=down, 1=right, 2=up, 3=left
    switch (direction) {
        case DIR_DOWN:  
            SPR_setFrame(spriteHead, 0); 
            SPR_setVRAMTileIndex(spriteHead, headVramIndexes[0]);
            break;
        case DIR_RIGHT: 
            SPR_setFrame(spriteHead, 1); 
            SPR_setVRAMTileIndex(spriteHead, headVramIndexes[1]);
            break;
        case DIR_UP:    
            SPR_setFrame(spriteHead, 2); 
            SPR_setVRAMTileIndex(spriteHead, headVramIndexes[2]);
            break;
        case DIR_LEFT:  
            SPR_setFrame(spriteHead, 3); 
            SPR_setVRAMTileIndex(spriteHead, headVramIndexes[3]);
            break;
    }
    
    // Body frames: 0=horizontal (left/right), 1=vertical (up/down)
    for (u16 i = 1; i < snakeLength; i++) {
        if (spriteBody[i-1]) {
            SPR_setPosition(spriteBody[i-1], 
                           snakeBody[i].x * SNAKE_TILE_SIZE, 
                           snakeBody[i].y * SNAKE_TILE_SIZE);
            s16 dx = snakeBody[i-1].x - snakeBody[i].x;
            s16 dy = snakeBody[i-1].y - snakeBody[i].y;
            if (dx != 0) { // Horizontal movement
                SPR_setFrame(spriteBody[i-1], 0);
                SPR_setVRAMTileIndex(spriteBody[i-1], bodyVramIndexes[0]);
            } else if (dy != 0) { // Vertical movement
                SPR_setFrame(spriteBody[i-1], 1);
                SPR_setVRAMTileIndex(spriteBody[i-1], bodyVramIndexes[1]);
            }
        }
    }
}

// Generates new food position using free tile list
static void generateFood(void) {
    Point freeTiles[(GRID_WIDTH - 2) * (GRID_HEIGHT - 3)]; // [NEW] Array of free positions
    u16 freeCount = 0;
    
    // Collect all free positions
    for (u16 y = 2; y < GRID_HEIGHT - 1; y++) {
        for (u16 x = 1; x < GRID_WIDTH - 1; x++) {
            if (!checkCollision(x, y)) {
                freeTiles[freeCount].x = x;
                freeTiles[freeCount].y = y;
                freeCount++;
            }
        }
    }
    
    if (freeCount == 0) {
        gameState = STATE_GAMEOVER;
        VDP_drawText("YOU WIN!", 16, 10);
        return;
    }
    
    u16 pick = random() % freeCount;
    food.x = freeTiles[pick].x;
    food.y = freeTiles[pick].y;
}

// Checks collision with snake (optimized to skip head)
static u16 checkCollision(s16 x, s16 y) {
    for (u16 i = 1; i < snakeLength; i++) { // [NEW] Skip i=0 (head)
        if (snakeBody[i].x == x && snakeBody[i].y == y) return TRUE;
    }
    return FALSE;
}

// Displays game over screen with snake disappearance, food disappearance, and tune with rest
static void showGameOver(void) {
    VDP_drawText("GAME OVER", 15, 10);
    VDP_drawText("PRESS START TO PLAY AGAIN", 8, 12);
    VDP_drawText("FINAL SCORE:", 14, 14);
    
    char scoreText[5];
    sprintf(scoreText, "%d", score);
    VDP_drawText(scoreText, 19 - (score >= 10 ? (score >= 100 ? (score >= 1000 ? 3 : 2) : 1) : 0), 16);
    
    // Silence gameplay music immediately
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
    PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
    
    // Add a brief rest before the game-over tune
    waitMs(200); // 200ms silence for transition
    
    // Define a simple game-over tune (descending notes)
    Note gameOverTune[] = {
        {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_C4, 8}, {NOTE_G3, 12}, {NOTE_REST, 8}
    };
    const u16 tuneSize = 5;
    u16 tuneIndex = 0;
    u16 tuneCounter = 0;
    
    // Animate snake disappearance from tail to head
    for (u16 i = snakeLength - 1; i > 0; i--) { // Start from tail (excluding head)
        if (spriteBody[i-1]) {
            SPR_releaseSprite(spriteBody[i-1]);
            spriteBody[i-1] = NULL;
            SPR_update();
            SYS_doVBlankProcess();
            waitMs(50);
        }
        
        // Play tune during animation
        if (tuneCounter == 0 && tuneIndex < tuneSize) {
            PSG_setFrequency(1, gameOverTune[tuneIndex].frequency);
            PSG_setEnvelope(1, gameOverTune[tuneIndex].frequency != NOTE_REST ? PSG_ENVELOPE_MAX / 4 : PSG_ENVELOPE_MIN);
            tuneCounter = gameOverTune[tuneIndex].baseDuration * 2; // Slower tempo
            tuneIndex++;
        }
        if (tuneCounter > 0) tuneCounter--;
    }
    
    // Release the head sprite
    if (spriteHead) {
        SPR_releaseSprite(spriteHead);
        spriteHead = NULL;
        SPR_update();
        SYS_doVBlankProcess();
        waitMs(50);
    }
    
    // Release the food sprite after the snake
    if (spriteFood) {
        SPR_releaseSprite(spriteFood);
        spriteFood = NULL;
        SPR_update();
        SYS_doVBlankProcess();
        waitMs(50); // Brief delay to emphasize food disappearance
    }
    
    // Finish the tune if any notes remain
    while (tuneIndex < tuneSize) {
        if (tuneCounter == 0) {
            PSG_setFrequency(1, gameOverTune[tuneIndex].frequency);
            PSG_setEnvelope(1, gameOverTune[tuneIndex].frequency != NOTE_REST ? PSG_ENVELOPE_MAX / 4 : PSG_ENVELOPE_MIN);
            tuneCounter = gameOverTune[tuneIndex].baseDuration * 2;
            tuneIndex++;
        }
        tuneCounter--;
        SYS_doVBlankProcess();
    }
    
    // Ensure silence after tune
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
    PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
}

// Plays "chomp" sound
static void playEatSound(void) {
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1000);
    waitMs(20);
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX / 2);
    PSG_setFrequency(0, 400);
    waitMs(30);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
}

// Updates chiptune music with tempo cap
static void updateMusic(void) {
    if (gameState == STATE_GAMEOVER || (gameState == STATE_PLAYING && paused)) {
        PSG_setEnvelope(1, PSG_ENVELOPE_MIN); // Silence melody
        PSG_setEnvelope(2, PSG_ENVELOPE_MIN); // Silence bass
        return;
    }
    
    // Normal playback when unpaused
    u16 melodyVolume = PSG_ENVELOPE_MAX / 8; // Full volume when playing
    u16 bassVolume = PSG_ENVELOPE_MAX / 16;
    
    // [NEW] Cap tempo to prevent chaotic speedup
    u16 tempoFactor = (gameState == STATE_INTRO) ? 12 : max((frameDelay * 10) / INITIAL_DELAY, MAX_TEMPO_FACTOR);
    const u16 melodyDuration = (melody[melodyIndex].baseDuration * tempoFactor) / 10;
    const u16 bassDuration = (bass[bassIndex].baseDuration * tempoFactor) / 10;
    
    if (melodyCounter == 0) {
        PSG_setFrequency(1, melody[melodyIndex].frequency);
        PSG_setEnvelope(1, melody[melodyIndex].frequency != NOTE_REST ? melodyVolume : PSG_ENVELOPE_MIN);
        melodyCounter = melodyDuration;
        melodyIndex = (melodyIndex + 1) % MELODY_SIZE;
    }
    melodyCounter--;
    
    if (bassCounter == 0) {
        PSG_setFrequency(2, bass[bassIndex].frequency);
        PSG_setEnvelope(2, bass[bassIndex].frequency != NOTE_REST ? bassVolume : PSG_ENVELOPE_MIN);
        bassCounter = bassDuration;
        bassIndex = (bassIndex + 1) % BASS_SIZE;
    }
    bassCounter--;
}

// Toggles pause state
static void togglePause(void) {
    paused = !paused;
    if (paused) VDP_drawText("PAUSE", 17, 14);
    else VDP_clearText(17, 14, 5);
}