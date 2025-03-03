// Snake Game for Sega Mega Drive using SGDK 2.00
// Enhanced with custom wall/sand tiles, dark green text, and updated title as of March 03, 2025
//
// Overview:
// A classic Snake game with a maze playfield, custom PNG tiles for walls and sand, and optimized food placement.
// The snake grows by eating food, navigates a maze, and ends on collision with walls or itself.
//
// Main Features:
// 1. Gameplay: D-pad controls snake movement; grows on food (score +10); ends on collision with borders, maze walls, or self.
// 2. Visuals:
//    - Head: 32x8 sprite sheet (4 frames: down, right, up, left).
//    - Body: 16x8 sprite sheet (2 frames: horizontal, vertical).
//    - Food: 8x8 red dot.
//    - Playfield: Custom sand tile (sand.png) background, custom wall tiles (wall.png) for borders and maze.
//    - Text: Dark green (PAL0 index 15) for score, intro, pause, and game-over screens.
//    - Intro: Custom tilemap from intro.png with PAL1.
// 3. Audio: Chiptune melody with capped tempo, intro tune, "chomp" sound, game-over tune with rest, toggleable.
// 4. Controls: Start toggles states or pauses; D-pad moves snake; B toggles music in intro.
// 5. Technical: PSG audio, sprite allocation checks, free tile list for O(1) food placement, manual VRAM management.

#include <genesis.h>
#include "resource.h"

// Game constants
#define GRID_WIDTH 40          // Total grid width in tiles (including borders)
#define GRID_HEIGHT 28         // Total grid height in tiles (including borders)
#define SNAKE_START_X 20       // Snake head’s starting X position
#define SNAKE_START_Y 14       // Snake head’s starting Y position
#define SNAKE_START_LENGTH 3   // Initial snake length
#define SNAKE_MAX_LENGTH 80    // Maximum snake length (limited by VDP sprite capacity: 80 sprites)
#define INITIAL_DELAY 8        // Initial frame delay between updates (slower speed)
#define MIN_DELAY 3            // Minimum frame delay (faster speed as score increases)
#define SNAKE_TILE_SIZE 8      // Sprite tile size (8x8 pixels)
#define MAX_TEMPO_FACTOR 6     // Minimum tempo factor to cap music speed
#define MAX_WALLS 50           // Maximum number of maze wall segments (each up to 5 tiles)
#define MAX_FREE_TILES ((GRID_WIDTH - 2) * (GRID_HEIGHT - 3)) // Maximum free tiles: 38x25 = 950

// Snake directions (correspond to head sprite frames)
#define DIR_UP 0               // Up direction (frame 2)
#define DIR_RIGHT 1            // Right direction (frame 1)
#define DIR_DOWN 2             // Down direction (frame 0)
#define DIR_LEFT 3             // Left direction (frame 3)

// Game states
#define STATE_INTRO 0          // Intro screen state
#define STATE_PLAYING 1        // Active gameplay state
#define STATE_GAMEOVER 2       // Game over state

// Music constants (PSG frequencies)
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
#define NOTE_REST 0            // Silence (no frequency)
#define MELODY_SIZE 16         // Length of melody loop
#define BASS_SIZE 8            // Length of bass loop

// Data structures
typedef struct {
    s16 x;                     // X position in tiles
    s16 y;                     // Y position in tiles
} Point;

typedef struct {
    u16 frequency;             // PSG frequency in Hz
    u16 baseDuration;          // Base duration in frames
} Note;

// Game state variables
static Point snakeBody[SNAKE_MAX_LENGTH]; // Snake segments (head at index 0)
static u16 snakeLength;                   // Current length of the snake
static u16 direction;                     // Current movement direction
static u16 nextDirection;                 // Buffered next direction from input
static Point food;                        // Current food position
static u16 score;                         // Player score
static u16 gameState;                     // Current game state
static u16 frameDelay;                    // Frames between snake updates
static u16 frameCount;                    // Frame counter for timing updates
static u16 paused;                        // Pause flag (TRUE/FALSE)
static u16 prevStartState;                // Previous Start button state for edge detection
static u16 introAnimFrame;                // Frame counter for intro animation
static u16 musicEnabled = TRUE;           // Music toggle state (TRUE = on)
static Point mazeWalls[MAX_WALLS * 5];    // Maze wall positions (up to 50 segments, 5 tiles each)
static u16 wallCount;                     // Total number of maze wall tiles
static Point freeTiles[MAX_FREE_TILES];   // List of free tile positions for food placement
static u16 freeTileCount;                 // Number of free tiles available

// Music state variables
static Note melody[MELODY_SIZE] = {       // Main gameplay melody
    {NOTE_C4, 8}, {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_C5, 16},
    {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_C5, 16}, {NOTE_REST, 8},
    {NOTE_A4, 8}, {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_G4, 16},
    {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_A4, 8}, {NOTE_G4, 16}
};
static Note bass[BASS_SIZE] = {           // Bassline accompaniment
    {NOTE_C4, 16}, {NOTE_G4/2, 16},
    {NOTE_C4, 16}, {NOTE_G4/2, 16},
    {NOTE_A4/2, 16}, {NOTE_E4/2, 16},
    {NOTE_F4/2, 16}, {NOTE_G4/2, 16}
};
static u16 melodyIndex = 0;               // Current melody note index
static u16 bassIndex = 0;                 // Current bass note index
static u16 melodyCounter = 0;             // Frames remaining for current melody note
static u16 bassCounter = 0;               // Frames remaining for current bass note

// Sprite and tile objects
static Sprite* spriteHead = NULL;         // Head sprite (4 animation frames)
static Sprite* spriteBody[SNAKE_MAX_LENGTH - 1]; // Body sprites (2 frames: horizontal, vertical)
static Sprite* spriteFood = NULL;         // Food sprite (single frame)
static u16 headVramIndexes[4];            // VRAM tile indices for head frames
static u16 bodyVramIndexes[2];            // VRAM tile indices for body frames
static u16 wallVramIndex;                 // VRAM index for wall tile
static u16 sandVramIndex;                 // VRAM index for sand tile

// Function prototypes
static void initGame(void);               // Initializes game state, sprites, and tiles
static void showIntroScreen(void);        // Displays intro screen with updated title
static void updateIntroScreen(void);      // Updates intro screen animation
static void startGame(void);              // Transitions to gameplay state
static void handleInput(void);            // Processes player input from joypad
static void updateGame(void);             // Updates game logic (movement, collisions, food)
static void drawGame(void);               // Renders game sprites
static void generateFood(void);           // Places new food using free tile list
static u16 checkCollision(s16 x, s16 y);  // Checks collisions with snake body or walls
static void showGameOver(void);           // Displays game over screen with animation
static void playEatSound(void);           // Plays food-eating sound effect
static void togglePause(void);            // Toggles pause state with tile restoration
static void updateMusic(void);            // Updates background music playback

// Main function: Entry point and game loop
int main() {
    JOY_init();                           // Initialize joypad input system
    SPR_init();                           // Initialize sprite engine
    
    // Set up PAL0 for gameplay (black intro background initially)
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));  // Black (intro background)
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0x008000));  // Dark Green (snake)
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0xFF0000));  // Red (food)
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xC0C0C0));  // Grey (unused)
    PAL_setColor(4, RGB24_TO_VDPCOLOR(0x800000));  // Dark Red (unused)
    PAL_setColor(5, RGB24_TO_VDPCOLOR(0x000080));  // Dark Blue (unused)
    PAL_setColor(6, RGB24_TO_VDPCOLOR(0x00A000));  // Medium Green (unused)
    PAL_setColor(7, RGB24_TO_VDPCOLOR(0xDEB887));  // Sand (gameplay base)
    PAL_setColor(8, RGB24_TO_VDPCOLOR(0xA52A2A));  // Brown (wall base)
    PAL_setColor(9, RGB24_TO_VDPCOLOR(0xFFD700));  // Gold (unused)
    PAL_setColor(10, RGB24_TO_VDPCOLOR(0xCD7F32)); // Bronze (unused)
    PAL_setColor(11, RGB24_TO_VDPCOLOR(0xFFFFAA)); // Pale Yellow (unused)
    PAL_setColor(12, RGB24_TO_VDPCOLOR(0xD2B48C)); // Light Brown (unused)
    PAL_setColor(13, RGB24_TO_VDPCOLOR(0xF5DEB3)); // Tan (unused)
    PAL_setColor(14, RGB24_TO_VDPCOLOR(0xFFFF00)); // Yellow (unused)
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0x008000)); // Dark Green (text)
    
    // Set up PAL1 for intro screen
    PAL_setColor(16, RGB24_TO_VDPCOLOR(0x000083));
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0x260081));
    PAL_setColor(18, RGB24_TO_VDPCOLOR(0x3e1179));
    PAL_setColor(19, RGB24_TO_VDPCOLOR(0x641a69));
    PAL_setColor(20, RGB24_TO_VDPCOLOR(0xfe0000));
    PAL_setColor(21, RGB24_TO_VDPCOLOR(0x3b329c));
    PAL_setColor(22, RGB24_TO_VDPCOLOR(0xa12c28));
    PAL_setColor(23, RGB24_TO_VDPCOLOR(0x1f5ba7));
    PAL_setColor(24, RGB24_TO_VDPCOLOR(0x027a00));
    PAL_setColor(25, RGB24_TO_VDPCOLOR(0x1a9a0f));
    PAL_setColor(26, RGB24_TO_VDPCOLOR(0xce7e33));
    PAL_setColor(27, RGB24_TO_VDPCOLOR(0xd8b228));
    PAL_setColor(28, RGB24_TO_VDPCOLOR(0xd0b18f));
    PAL_setColor(29, RGB24_TO_VDPCOLOR(0xe0b889));
    PAL_setColor(30, RGB24_TO_VDPCOLOR(0xfdd800));
    PAL_setColor(31, RGB24_TO_VDPCOLOR(0xf6ddb4));
    
    VDP_setTextPalette(PAL0);         // Set text to use PAL0 (dark green at index 15)
    VDP_setTextPriority(1);           // Text renders above sprites and background
    PSG_reset();                      // Reset PSG audio channels
    
    showIntroScreen();                // Display intro screen on startup
    
    while (1) {                       // Infinite game loop
        handleInput();                // Process player input
        if (gameState == STATE_INTRO) {
            updateIntroScreen();      // Update intro screen animation
            SPR_update();             // Update sprite engine
        }
        else if (gameState == STATE_PLAYING) {
            frameCount++;
            if (frameCount >= frameDelay && !paused) { // Update game logic at frameDelay intervals
                frameCount = 0;
                updateGame();
            }
            drawGame();               // Render game state
            SPR_update();             // Update sprite engine
        }
        updateMusic();                // Update music playback
        SYS_doVBlankProcess();        // Sync to V-blank (60 FPS)
    }
    
    return 0;                         // Never reached due to infinite loop
}

// Initializes game state, sprites, and tiled playfield
static void initGame(void) {
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0xDEB887)); // Set gameplay background to sand color
    
    // Clean up any existing sprites
    if (spriteHead) SPR_releaseSprite(spriteHead);
    for (u16 i = 0; i < SNAKE_MAX_LENGTH - 1; i++) {
        if (spriteBody[i]) SPR_releaseSprite(spriteBody[i]);
        spriteBody[i] = NULL;
    }
    if (spriteFood) SPR_releaseSprite(spriteFood);
    
    VDP_clearPlane(BG_A, TRUE);       // Clear background planes to sand color
    VDP_clearPlane(BG_B, TRUE);
    
    // Load wall tile from wall.png
    u16 vramIndex = TILE_USER_INDEX + intro.tileset->numTile;
    VDP_loadTileSet(&wall_tileset, vramIndex, DMA);
    wallVramIndex = vramIndex;
    const u16 wallTileAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, wallVramIndex);
    vramIndex += wall_tileset.numTile;
    
    // Load sand tile from sand.png and tile the playfield
    VDP_loadTileSet(&sand_tileset, vramIndex, DMA);
    sandVramIndex = vramIndex;
    const u16 sandTileAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, sandVramIndex);
    vramIndex += sand_tileset.numTile;
    for (u16 y = 2; y < GRID_HEIGHT - 1; y++) {
        for (u16 x = 1; x < GRID_WIDTH - 1; x++) {
            VDP_setTileMapXY(BG_A, sandTileAttr, x, y); // Tile playfield with sand
        }
    }
    
    // Draw outer borders with wall tiles
    for (u16 i = 0; i < GRID_WIDTH; i++) {
        VDP_setTileMapXY(BG_A, wallTileAttr, i, 1);
        VDP_setTileMapXY(BG_A, wallTileAttr, i, GRID_HEIGHT - 1);
    }
    for (u16 i = 2; i < GRID_HEIGHT - 1; i++) {
        VDP_setTileMapXY(BG_A, wallTileAttr, 0, i);
        VDP_setTileMapXY(BG_A, wallTileAttr, GRID_WIDTH - 1, i);
    }
    
    // Generate random maze walls with wall tiles
    wallCount = 0;
    u16 numWalls = 10;                // Number of maze segments
    for (u16 w = 0; w < numWalls && wallCount < MAX_WALLS * 5; w++) {
        u16 isVertical = random() % 2; // 0=horizontal, 1=vertical
        u16 length = 3 + (random() % 3); // 3-5 tiles per segment
        u16 x, y;
        if (isVertical) {
            x = 2 + (random() % (GRID_WIDTH - 4)); // Avoid borders
            y = 3 + (random() % (GRID_HEIGHT - length - 4)); // Leave gaps
            for (u16 i = 0; i < length && y + i < GRID_HEIGHT - 1 && wallCount < MAX_WALLS * 5; i++) {
                if (x != SNAKE_START_X || y + i != SNAKE_START_Y) { // Avoid snake start position
                    VDP_setTileMapXY(BG_A, wallTileAttr, x, y + i);
                    mazeWalls[wallCount].x = x;
                    mazeWalls[wallCount].y = y + i;
                    wallCount++;
                }
            }
        } else {
            x = 2 + (random() % (GRID_WIDTH - length - 3)); // Avoid borders
            y = 3 + (random() % (GRID_HEIGHT - 5)); // Leave gaps
            for (u16 i = 0; i < length && x + i < GRID_WIDTH - 1 && wallCount < MAX_WALLS * 5; i++) {
                if (x + i != SNAKE_START_X || y != SNAKE_START_Y) { // Avoid snake start position
                    VDP_setTileMapXY(BG_A, wallTileAttr, x + i, y);
                    mazeWalls[wallCount].x = x + i;
                    mazeWalls[wallCount].y = y;
                    wallCount++;
                }
            }
        }
    }
    
    // Initialize free tile list (playable area minus walls and snake)
    freeTileCount = 0;
    for (u16 y = 2; y < GRID_HEIGHT - 1; y++) {
        for (u16 x = 1; x < GRID_WIDTH - 1; x++) {
            u16 isWall = FALSE;
            for (u16 i = 0; i < wallCount; i++) {
                if (mazeWalls[i].x == x && mazeWalls[i].y == y) {
                    isWall = TRUE;
                    break;
                }
            }
            if (!isWall) {
                freeTiles[freeTileCount].x = x;
                freeTiles[freeTileCount].y = y;
                freeTileCount++;
            }
        }
    }
    
    // Initialize snake (head at index 0)
    snakeLength = SNAKE_START_LENGTH;
    for (u16 i = 0; i < snakeLength; i++) {
        snakeBody[i].x = SNAKE_START_X - i;
        snakeBody[i].y = SNAKE_START_Y;
        // Remove snake positions from free tiles
        for (u16 j = 0; j < freeTileCount; j++) {
            if (freeTiles[j].x == snakeBody[i].x && freeTiles[j].y == snakeBody[i].y) {
                freeTiles[j] = freeTiles[freeTileCount - 1];
                freeTileCount--;
                break;
            }
        }
    }
    direction = DIR_RIGHT;            // Initial direction is right
    nextDirection = DIR_RIGHT;        // Buffer matches initial direction
    
    // Load head sprite frames (32x8, 4 directions)
    Animation* headAnim = snake_head_sprite.animations[0];
    for (u16 i = 0; i < 4; i++) {
        TileSet* tileset = headAnim->frames[i]->tileset;
        VDP_loadTileSet(tileset, vramIndex, DMA);
        headVramIndexes[i] = vramIndex;
        vramIndex += tileset->numTile;
    }
    
    // Load body sprite frames (16x8, horizontal/vertical)
    Animation* bodyAnim = snake_body_sprite.animations[0];
    for (u16 i = 0; i < 2; i++) {
        TileSet* tileset = bodyAnim->frames[i]->tileset;
        VDP_loadTileSet(tileset, vramIndex, DMA);
        bodyVramIndexes[i] = vramIndex;
        vramIndex += tileset->numTile;
    }
    
    // Create head sprite
    spriteHead = SPR_addSprite(&snake_head_sprite,
                              SNAKE_START_X * SNAKE_TILE_SIZE,
                              SNAKE_START_Y * SNAKE_TILE_SIZE,
                              TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
    SPR_setAutoTileUpload(spriteHead, FALSE);
    SPR_setFrame(spriteHead, 1);  // Right-facing frame
    SPR_setVRAMTileIndex(spriteHead, headVramIndexes[1]);
    
    // Create initial body sprites
    for (u16 i = 1; i < snakeLength; i++) {
        spriteBody[i-1] = SPR_addSprite(&snake_body_sprite,
                                       snakeBody[i].x * SNAKE_TILE_SIZE,
                                       snakeBody[i].y * SNAKE_TILE_SIZE,
                                       TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
        if (!spriteBody[i-1]) {
            snakeLength = i;      // Cap length if sprite allocation fails
            break;
        }
        SPR_setAutoTileUpload(spriteBody[i-1], FALSE);
        SPR_setFrame(spriteBody[i-1], 0); // Horizontal frame
        SPR_setVRAMTileIndex(spriteBody[i-1], bodyVramIndexes[0]);
    }
    for (u16 i = snakeLength - 1; i < SNAKE_MAX_LENGTH - 1; i++) {
        spriteBody[i] = NULL;     // Clear unused sprite slots
    }
    
    // Place initial food
    generateFood();
    spriteFood = SPR_addSprite(&food_sprite,
                              food.x * SNAKE_TILE_SIZE,
                              food.y * SNAKE_TILE_SIZE,
                              TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
    
    // Reset game state variables
    score = 0;
    paused = FALSE;
    prevStartState = FALSE;
    frameDelay = INITIAL_DELAY;
    frameCount = 0;
    melodyIndex = 0;
    bassIndex = 0;
    melodyCounter = 0;
    bassCounter = 0;
    
    // Display initial score
    char scoreText[20];
    sprintf(scoreText, "SCORE: %4d", score);
    VDP_drawText(scoreText, 1, 0);
}

// Displays intro screen with updated title "AI-MAZE-ING SNAKE"
static void showIntroScreen(void) {
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000)); // Set intro background to black
    VDP_clearPlane(BG_A, TRUE);       // Clear planes to black
    VDP_clearPlane(BG_B, TRUE);
    
    // Load and set intro image on BG_B using PAL1
    VDP_loadTileSet(intro.tileset, TILE_USER_INDEX, DMA);
    VDP_setMapEx(BG_B, intro.tilemap, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                 0, 0, 0, 0, 40, 28);
    
    // Draw intro text on BG_A using PAL0 (dark green)
    VDP_drawText("AI-MAZE-ING SNAKE", 12, 2); // Updated title, centered
    VDP_drawText("START TO PLAY", 14, 6);    // "PRESS" removed
    VDP_drawText("B TO TOGGLE MUSIC", 12, 10); // "PRESS" removed
    
    // Initialize intro state
    introAnimFrame = 0;
    gameState = STATE_INTRO;
    melodyIndex = 0;
    bassIndex = 0;
    melodyCounter = 0;
    bassCounter = 0;
    score = 0;
    paused = FALSE;
    prevStartState = FALSE;
    musicEnabled = TRUE;
}

// Updates intro screen (blinking "START TO PLAY" text)
static void updateIntroScreen(void) {
    introAnimFrame++;
    if (introAnimFrame % 60 < 30) {   // Blink every 60 frames (1 second at 60 FPS)
        VDP_drawText("START TO PLAY", 14, 6);
    } else {
        VDP_clearText(14, 6, 13);
    }
}

// Transitions from intro to gameplay
static void startGame(void) {
    initGame();
    gameState = STATE_PLAYING;
}

// Handles player input from joypad
static void handleInput(void) {
    const u16 joy = JOY_readJoypad(JOY_1); // Read joypad 1 state
    const u16 startPressed = joy & BUTTON_START;
    const u16 bPressed = joy & BUTTON_B;
    
    // Start button: toggle game states or pause
    if (startPressed && !prevStartState) {
        if (gameState == STATE_INTRO) startGame();
        else if (gameState == STATE_PLAYING) togglePause();
        else if (gameState == STATE_GAMEOVER) showIntroScreen();
    }
    prevStartState = startPressed;
    
    // B button: toggle music during intro
    static u16 prevBState = FALSE;
    if (gameState == STATE_INTRO && bPressed && !prevBState) {
        musicEnabled = !musicEnabled;
        if (!musicEnabled) {
            PSG_setEnvelope(1, PSG_ENVELOPE_MIN); // Silence melody
            PSG_setEnvelope(2, PSG_ENVELOPE_MIN); // Silence bass
        }
    }
    prevBState = bPressed;
    
    // D-pad: update snake direction (only when playing and not paused)
    if (gameState == STATE_PLAYING && !paused) {
        if (joy & BUTTON_UP && direction != DIR_DOWN) nextDirection = DIR_UP;
        else if (joy & BUTTON_RIGHT && direction != DIR_LEFT) nextDirection = DIR_RIGHT;
        else if (joy & BUTTON_DOWN && direction != DIR_UP) nextDirection = DIR_DOWN;
        else if (joy & BUTTON_LEFT && direction != DIR_RIGHT) nextDirection = DIR_LEFT;
    }
}

// Updates game logic (snake movement, collisions, food handling)
static void updateGame(void) {
    s16 newHeadX = snakeBody[0].x; // Current head X position
    s16 newHeadY = snakeBody[0].y; // Current head Y position
    
    direction = nextDirection;     // Apply buffered direction
    switch (direction) {           // Update head position based on direction
        case DIR_UP:    newHeadY--; break;
        case DIR_RIGHT: newHeadX++; break;
        case DIR_DOWN:  newHeadY++; break;
        case DIR_LEFT:  newHeadX--; break;
    }
    
    // Check collision with borders or maze walls
    if (newHeadX <= 0 || newHeadX >= GRID_WIDTH - 1 || newHeadY <= 1 || newHeadY >= GRID_HEIGHT - 1 || checkCollision(newHeadX, newHeadY)) {
        gameState = STATE_GAMEOVER;
        showGameOver();
        return;
    }
    
    // Handle food collision
    if (newHeadX == food.x && newHeadY == food.y) {
        if (snakeLength < SNAKE_MAX_LENGTH) { // Grow snake if under max length
            for (u16 i = snakeLength; i > 0; i--) {
                snakeBody[i] = snakeBody[i - 1]; // Shift segments forward
            }
            if (snakeLength > 1 && !spriteBody[snakeLength-2]) { // Add new body sprite
                spriteBody[snakeLength-2] = SPR_addSprite(&snake_body_sprite,
                                                         -16, -16, // Offscreen initially
                                                         TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
                if (!spriteBody[snakeLength-2]) {
                    snakeLength--; // Cap length if sprite allocation fails
                    VDP_drawText("SPRITE LIMIT!", 14, 10);
                    return;
                }
                SPR_setAutoTileUpload(spriteBody[snakeLength-2], FALSE);
                SPR_setFrame(spriteBody[snakeLength-2], 0);
                SPR_setVRAMTileIndex(spriteBody[snakeLength-2], bodyVramIndexes[0]);
            }
            snakeLength++;
        } else { // Move without growing if at max length
            for (u16 i = snakeLength - 1; i > 0; i--) {
                snakeBody[i] = snakeBody[i - 1];
            }
        }
        
        playEatSound();           // Play eating sound effect
        score += 10;              // Increase score by 10
        char scoreText[20];
        sprintf(scoreText, "SCORE: %4d", score);
        VDP_clearText(1, 0, 20);
        VDP_drawText(scoreText, 1, 0); // Update score display
        
        generateFood();           // Place new food
        SPR_setPosition(spriteFood, food.x * SNAKE_TILE_SIZE, food.y * SNAKE_TILE_SIZE);
        
        if (score % 50 == 0 && frameDelay > MIN_DELAY) frameDelay--; // Increase speed every 50 points
    } else { // Move without eating
        for (u16 i = snakeLength - 1; i > 0; i--) {
            snakeBody[i] = snakeBody[i - 1]; // Shift segments forward
        }
        // Update free tile list: remove old tail, add previous head position
        for (u16 i = 0; i < freeTileCount; i++) {
            if (freeTiles[i].x == snakeBody[snakeLength - 1].x && freeTiles[i].y == snakeBody[snakeLength - 1].y) {
                freeTiles[i] = freeTiles[freeTileCount - 1];
                freeTileCount--;
                break;
            }
        }
        freeTiles[freeTileCount].x = snakeBody[0].x;
        freeTiles[freeTileCount].y = snakeBody[0].y;
        freeTileCount++;
    }
    
    // Update head position
    snakeBody[0].x = newHeadX;
    snakeBody[0].y = newHeadY;
}

// Renders game sprites (head, body, food) to the screen
static void drawGame(void) {
    // Position and animate head sprite
    SPR_setPosition(spriteHead, snakeBody[0].x * SNAKE_TILE_SIZE, snakeBody[0].y * SNAKE_TILE_SIZE);
    switch (direction) {
        case DIR_DOWN:  SPR_setFrame(spriteHead, 0); SPR_setVRAMTileIndex(spriteHead, headVramIndexes[0]); break;
        case DIR_RIGHT: SPR_setFrame(spriteHead, 1); SPR_setVRAMTileIndex(spriteHead, headVramIndexes[1]); break;
        case DIR_UP:    SPR_setFrame(spriteHead, 2); SPR_setVRAMTileIndex(spriteHead, headVramIndexes[2]); break;
        case DIR_LEFT:  SPR_setFrame(spriteHead, 3); SPR_setVRAMTileIndex(spriteHead, headVramIndexes[3]); break;
    }
    
    // Position and animate body sprites
    for (u16 i = 1; i < snakeLength; i++) {
        if (spriteBody[i-1]) {
            SPR_setPosition(spriteBody[i-1], snakeBody[i].x * SNAKE_TILE_SIZE, snakeBody[i].y * SNAKE_TILE_SIZE);
            s16 dx = snakeBody[i-1].x - snakeBody[i].x;
            s16 dy = snakeBody[i-1].y - snakeBody[i].y;
            if (dx != 0) { // Horizontal orientation
                SPR_setFrame(spriteBody[i-1], 0);
                SPR_setVRAMTileIndex(spriteBody[i-1], bodyVramIndexes[0]);
            } else if (dy != 0) { // Vertical orientation
                SPR_setFrame(spriteBody[i-1], 1);
                SPR_setVRAMTileIndex(spriteBody[i-1], bodyVramIndexes[1]);
            }
        }
    }
}

// Generates a new food position using the free tile list
static void generateFood(void) {
    if (freeTileCount == 0) {         // No free tiles means player wins
        gameState = STATE_GAMEOVER;
        VDP_drawText("YOU WIN!", 16, 10);
        return;
    }
    
    u16 pick = random() % freeTileCount; // Pick a random free tile
    food.x = freeTiles[pick].x;
    food.y = freeTiles[pick].y;
    
    // Remove selected tile from free list
    freeTiles[pick] = freeTiles[freeTileCount - 1];
    freeTileCount--;
}

// Checks for collisions with snake body (excluding head) or maze walls
static u16 checkCollision(s16 x, s16 y) {
    for (u16 i = 1; i < snakeLength; i++) { // Check snake body, skip head
        if (snakeBody[i].x == x && snakeBody[i].y == y) return TRUE;
    }
    for (u16 i = 0; i < wallCount; i++) { // Check maze walls
        if (mazeWalls[i].x == x && mazeWalls[i].y == y) return TRUE;
    }
    return FALSE;
}

// Displays game over screen with snake disappearance animation and tune
static void showGameOver(void) {
    VDP_drawText("GAME OVER", 15, 10);
    VDP_drawText("START TO PLAY AGAIN", 11, 12); // Updated to match intro style
    VDP_drawText("FINAL SCORE:", 14, 14);
    
    char scoreText[5];
    sprintf(scoreText, "%d", score);
    VDP_drawText(scoreText, 19 - (score >= 10 ? (score >= 100 ? (score >= 1000 ? 3 : 2) : 1) : 0), 16);
    
    // Silence gameplay music
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
    PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
    
    waitMs(200);                      // Brief pause before animation
    
    // Define game-over tune (descending notes)
    Note gameOverTune[] = {
        {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_C4, 8}, {NOTE_G3, 12}, {NOTE_REST, 8}
    };
    const u16 tuneSize = 5;
    u16 tuneIndex = 0;
    u16 tuneCounter = 0;
    
    // Animate snake disappearance from tail to head
    for (u16 i = snakeLength - 1; i > 0; i--) {
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
            tuneCounter = gameOverTune[tuneIndex].baseDuration * 2;
            tuneIndex++;
        }
        if (tuneCounter > 0) tuneCounter--;
    }
    
    // Remove head sprite
    if (spriteHead) {
        SPR_releaseSprite(spriteHead);
        spriteHead = NULL;
        SPR_update();
        SYS_doVBlankProcess();
        waitMs(50);
    }
    
    // Remove food sprite
    if (spriteFood) {
        SPR_releaseSprite(spriteFood);
        spriteFood = NULL;
        SPR_update();
        SYS_doVBlankProcess();
        waitMs(50);
    }
    
    // Finish playing tune
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
    
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
    PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
}

// Plays "chomp" sound effect when snake eats food
static void playEatSound(void) {
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX); // Full volume
    PSG_setFrequency(0, 1000);            // High pitch
    waitMs(20);
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX / 2); // Half volume
    PSG_setFrequency(0, 400);             // Lower pitch
    waitMs(30);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN); // Silence
}

// Updates chiptune music playback (melody and bass)
static void updateMusic(void) {
    if (gameState == STATE_GAMEOVER || (gameState == STATE_PLAYING && paused)) {
        PSG_setEnvelope(1, PSG_ENVELOPE_MIN); // Silence melody
        PSG_setEnvelope(2, PSG_ENVELOPE_MIN); // Silence bass
        return;
    }
    
    if (!musicEnabled) {                  // Music toggle off
        PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
        PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
        return;
    }
    
    u16 melodyVolume = PSG_ENVELOPE_MAX / 8;   // Lower volume for melody
    u16 bassVolume = PSG_ENVELOPE_MAX / 16;    // Even lower for bass
    
    // Define intro tune (upbeat pattern)
    static Note introMelody[MELODY_SIZE] = {
        {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_A4, 8}, {NOTE_G4, 8},
        {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_A4, 12}, {NOTE_REST, 8},
        {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_A4, 8},
        {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_A4, 12}, {NOTE_REST, 8}
    };
    
    // Select melody based on game state
    Note* currentMelody = (gameState == STATE_INTRO) ? introMelody : melody;
    u16 tempoFactor = (gameState == STATE_INTRO) ? 12 : max((frameDelay * 10) / INITIAL_DELAY, MAX_TEMPO_FACTOR);
    const u16 melodyDuration = (currentMelody[melodyIndex].baseDuration * tempoFactor) / 10;
    const u16 bassDuration = (bass[bassIndex].baseDuration * tempoFactor) / 10;
    
    // Update melody channel
    if (melodyCounter == 0) {
        PSG_setFrequency(1, currentMelody[melodyIndex].frequency);
        PSG_setEnvelope(1, currentMelody[melodyIndex].frequency != NOTE_REST ? melodyVolume : PSG_ENVELOPE_MIN);
        melodyCounter = melodyDuration;
        melodyIndex = (melodyIndex + 1) % MELODY_SIZE;
    }
    melodyCounter--;
    
    // Update bass channel
    if (bassCounter == 0) {
        PSG_setFrequency(2, bass[bassIndex].frequency);
        PSG_setEnvelope(2, bass[bassIndex].frequency != NOTE_REST ? bassVolume : PSG_ENVELOPE_MIN);
        bassCounter = bassDuration;
        bassIndex = (bassIndex + 1) % BASS_SIZE;
    }
    bassCounter--;
}

// Toggles pause state and manages "PAUSE" text display with tile restoration
static void togglePause(void) {
    paused = !paused;
    const u16 sandTileAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, sandVramIndex);
    if (paused) {
        VDP_drawText("PAUSE", 17, 14); // Display "PAUSE" at position (17, 14)
    } else {
        VDP_clearText(17, 14, 5);     // Clear "PAUSE" (5 characters)
        // Restore sand tiles where "PAUSE" was displayed
        for (u16 x = 17; x < 22; x++) {
            VDP_setTileMapXY(BG_A, sandTileAttr, x, 14);
        }
    }
}