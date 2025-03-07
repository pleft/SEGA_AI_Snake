// Snake Game for Sega Mega Drive using SGDK 2.00
// Enhanced with levels, custom wall/sand tiles, dark green text, portals, maze, and smooth level transitions as of March 07, 2025
//
// Overview:
// A classic Snake game with a maze playfield where the snake must eat a specified number of food sprites
// to complete each level. Levels increase in difficulty with more food targets and maze walls. Portals
// allow teleportation between borders. Level transitions now include a brief intermission with a "Level X"
// message, blinking text, and a jingle for polish.
//
// Main Features:
// 1. Levels: Each level requires eating a set number of food sprites (e.g., 5 for Level 1, 10 for Level 2).
//    On completion, a transition state displays the new level before resetting the maze and portals.
// 2. Gameplay: D-pad controls movement; snake grows on food (score +10); ends on collision with walls/self.
// 3. Visuals:
//    - Head: 32x8 sprite sheet (4 frames: down, right, up, left).
//    - Body: 16x8 sprite sheet (2 frames: horizontal, vertical).
//    - Food: 8x8 red dot.
//    - Playfield: Sand tile background, wall tiles for borders/maze, sand tiles as portals.
//    - Text: Dark green (PAL0 index 15) for score, level info, intro, pause, and game-over screens.
// 4. Audio: Chiptune melody with capped tempo, intro tune, "chomp" sound, game-over tune, level-up jingle, toggleable.
// 5. Controls: Start toggles states/pauses; D-pad moves snake; B toggles music in intro.

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
#define MAX_FREE_TILES ((GRID_WIDTH - 2) * (GRID_HEIGHT - 3)) // Max free tiles: 38x25 = 950
#define NUM_PORTALS 2          // Number of portal pairs (top-bottom, left-right)
#define TRANSITION_DURATION 90 // Transition display time (~1.5s at 60 FPS, adjustable)

// Snake directions (correspond to head sprite frames)
#define DIR_UP 0               // Up direction (frame 2)
#define DIR_RIGHT 1            // Right direction (frame 1)
#define DIR_DOWN 2             // Down direction (frame 0)
#define DIR_LEFT 3             // Left direction (frame 3)

// Game states
#define STATE_INTRO 0          // Intro screen state
#define STATE_PLAYING 1        // Active gameplay state
#define STATE_GAMEOVER 2       // Game over state
#define STATE_LEVEL_TRANSITION 3 // Level transition state

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
#define LEVEL_UP_SIZE 4        // Length of level-up jingle

// Data structures
typedef struct {
    s16 x;                     // X position in tiles
    s16 y;                     // Y position in tiles
} Point;

typedef struct {
    u16 frequency;             // PSG frequency in Hz
    u16 baseDuration;          // Base duration in frames
} Note;

typedef struct {
    Point entry;               // Entry portal position
    Point exit;                // Exit portal position
} Portal;

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
static Portal portals[NUM_PORTALS];       // Array of portal pairs
static u16 currentLevel = 1;              // Current level number (starts at 1)
static u16 foodEatenThisLevel = 0;        // Food eaten in the current level
static u16 foodTarget = 5;                // Target food count for current level
static u16 transitionTimer = 0;           // Frames remaining for level transition

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
static Note levelUpJingle[LEVEL_UP_SIZE] = { // Level-up jingle (rising scale)
    {NOTE_C4, 6}, {NOTE_E4, 6}, {NOTE_G4, 6}, {NOTE_C5, 12}
};
static u16 melodyIndex = 0;               // Current melody note index
static u16 bassIndex = 0;                 // Current bass note index
static u16 melodyCounter = 0;             // Frames remaining for current melody note
static u16 bassCounter = 0;               // Frames remaining for current bass note
static u16 jingleIndex = 0;               // Current jingle note index
static u16 jingleCounter = 0;             // Frames remaining for jingle note

// Sprite and tile objects
static Sprite* spriteHead = NULL;         // Head sprite (4 animation frames)
static Sprite* spriteBody[SNAKE_MAX_LENGTH - 1]; // Body sprites (2 frames: horizontal, vertical)
static Sprite* spriteFood = NULL;         // Food sprite (single frame)
static u16 headVramIndexes[4];            // VRAM tile indices for head frames
static u16 bodyVramIndexes[2];            // VRAM tile indices for body frames
static u16 wallVramIndex;                 // VRAM index for wall tile
static u16 sandVramIndex;                 // VRAM index for sand tile

// Function prototypes
static void initGame(void);               // Initializes game state and first level
static void initLevel(void);              // Resets maze, portals, and food for a new level
static void showIntroScreen(void);        // Displays intro screen with title
static void updateIntroScreen(void);      // Updates intro screen animation
static void startGame(void);              // Transitions to gameplay state
static void handleInput(void);            // Processes player input from joypad
static void updateGame(void);             // Updates game logic (movement, collisions, levels)
static void drawGame(void);               // Renders game sprites
static void generateFood(void);           // Places new food using free tile list
static u16 checkCollision(s16 x, s16 y);  // Checks collisions with snake body or walls
static void showGameOver(void);           // Displays game over screen with animation
static void playEatSound(void);           // Plays food-eating sound effect
static void togglePause(void);            // Toggles pause state with tile restoration
static void updateMusic(void);            // Updates background music and jingle playback
static void updateLevelDisplay(void);     // Updates level and food progress display

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
            updateIntroScreen();      // Update intro animation
            SPR_update();             // Update sprite engine
        }
        else if (gameState == STATE_PLAYING) {
            frameCount++;
            if (frameCount >= frameDelay && !paused) { // Update game logic at intervals
                frameCount = 0;
                updateGame();
            }
            drawGame();               // Render game state
            SPR_update();             // Update sprites
        }
        else if (gameState == STATE_LEVEL_TRANSITION) { // Handle level transition
            if (transitionTimer > 0) {
                transitionTimer--;
                // Blink "Level X" text (20 frames on, 20 frames off)
                if ((transitionTimer % 40) < 20) {
                    char levelText[8];
                    sprintf(levelText, "LEVEL %d", currentLevel);
                    VDP_drawText(levelText, 16, 12); // Centered-ish
                } else {
                    VDP_clearText(16, 12, 8); // Clear max 8 chars
                }
                drawGame();           // Keep rendering snake and food
                SPR_update();         // Update sprites
            }
            if (transitionTimer == 0) {
                VDP_clearText(16, 12, 8); // Ensure text is cleared
                initLevel();          // Reset maze and portals
                gameState = STATE_PLAYING; // Resume gameplay
                jingleIndex = 0;      // Reset jingle for next transition
                jingleCounter = 0;
            }
        }
        updateMusic();                // Update music and jingle playback
        SYS_doVBlankProcess();        // Sync to V-blank (60 FPS)
    }
    
    return 0;                         // Never reached due to infinite loop
}

// Initializes game state and sets up the first level with a transition
static void initGame(void) {
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0xDEB887)); // Set gameplay background to sand
    
    // Clean up existing sprites
    if (spriteHead) SPR_releaseSprite(spriteHead);
    for (u16 i = 0; i < SNAKE_MAX_LENGTH - 1; i++) {
        if (spriteBody[i]) SPR_releaseSprite(spriteBody[i]);
        spriteBody[i] = NULL;
    }
    if (spriteFood) SPR_releaseSprite(spriteFood);
    spriteHead = NULL;
    spriteFood = NULL;
    
    // Reset game-wide state
    snakeLength = SNAKE_START_LENGTH;
    for (u16 i = 0; i < snakeLength; i++) {
        snakeBody[i].x = SNAKE_START_X - i; // Snake starts horizontally facing right
        snakeBody[i].y = SNAKE_START_Y;
    }
    direction = DIR_RIGHT;
    nextDirection = DIR_RIGHT;
    score = 0;
    frameDelay = INITIAL_DELAY;
    frameCount = 0;
    paused = FALSE;
    prevStartState = FALSE;
    melodyIndex = 0;
    bassIndex = 0;
    melodyCounter = 0;
    bassCounter = 0;
    jingleIndex = 0;
    jingleCounter = 0;
    currentLevel = 1;
    foodEatenThisLevel = 0;
    foodTarget = 5;
    
    initLevel();                      // Set up initial level
    gameState = STATE_LEVEL_TRANSITION; // Start with transition for Level 1
    transitionTimer = TRANSITION_DURATION;
    VDP_drawText("LEVEL 1", 16, 12);  // Display "Level 1" immediately
    
    // Display initial score and level info
    char scoreText[20];
    sprintf(scoreText, "SCORE: %4d", score);
    VDP_drawText(scoreText, 1, 0);
    updateLevelDisplay();
}

// Resets maze, portals, and food for a new level while preserving snake state
static void initLevel(void) {
    // Load wall tile
    u16 vramIndex = TILE_USER_INDEX + intro.tileset->numTile;
    VDP_loadTileSet(&wall_tileset, vramIndex, DMA);
    wallVramIndex = vramIndex;
    const u16 wallTileAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, wallVramIndex);
    vramIndex += wall_tileset.numTile;
    
    // Load sand tile and tile the playfield
    VDP_loadTileSet(&sand_tileset, vramIndex, DMA);
    sandVramIndex = vramIndex;
    const u16 sandTileAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, sandVramIndex);
    vramIndex += sand_tileset.numTile;
    
    // Clear playfield and redraw borders
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    for (u16 y = 2; y < GRID_HEIGHT - 1; y++) {
        for (u16 x = 1; x < GRID_WIDTH - 1; x++) {
            VDP_setTileMapXY(BG_A, sandTileAttr, x, y);
        }
    }
    for (u16 i = 0; i < GRID_WIDTH; i++) {
        VDP_setTileMapXY(BG_A, wallTileAttr, i, 1);
        VDP_setTileMapXY(BG_A, wallTileAttr, i, GRID_HEIGHT - 1);
    }
    for (u16 i = 2; i < GRID_HEIGHT - 1; i++) {
        VDP_setTileMapXY(BG_A, wallTileAttr, 0, i);
        VDP_setTileMapXY(BG_A, wallTileAttr, GRID_WIDTH - 1, i);
    }
    
    // Randomize portal positions
    portals[0].entry.x = 5 + (random() % (GRID_WIDTH - 10));
    portals[0].entry.y = 1;
    portals[0].exit.x = 5 + (random() % (GRID_WIDTH - 10));
    portals[0].exit.y = GRID_HEIGHT - 1;
    VDP_setTileMapXY(BG_A, sandTileAttr, portals[0].entry.x, portals[0].entry.y);
    VDP_setTileMapXY(BG_A, sandTileAttr, portals[0].exit.x, portals[0].exit.y);
    
    portals[1].entry.x = 0;
    portals[1].entry.y = 5 + (random() % (GRID_HEIGHT - 10));
    portals[1].exit.x = GRID_WIDTH - 1;
    portals[1].exit.y = 5 + (random() % (GRID_HEIGHT - 10));
    VDP_setTileMapXY(BG_A, sandTileAttr, portals[1].entry.x, portals[1].entry.y);
    VDP_setTileMapXY(BG_A, sandTileAttr, portals[1].exit.x, portals[1].exit.y);
    
    // Generate random maze walls
    wallCount = 0;
    u16 numWalls = 5 + currentLevel;
    if (numWalls > MAX_WALLS) numWalls = MAX_WALLS;
    for (u16 w = 0; w < numWalls && wallCount < MAX_WALLS * 5; w++) {
        u16 isVertical = random() % 2;
        u16 length = 3 + (random() % 3);
        u16 x, y;
        if (isVertical) {
            x = 2 + (random() % (GRID_WIDTH - 4));
            y = 3 + (random() % (GRID_HEIGHT - length - 4));
            for (u16 i = 0; i < length && y + i < GRID_HEIGHT - 1 && wallCount < MAX_WALLS * 5; i++) {
                u16 valid = TRUE;
                for (u16 j = 0; j < snakeLength; j++) {
                    if (x == snakeBody[j].x && y + i == snakeBody[j].y) valid = FALSE;
                }
                if (valid) {
                    VDP_setTileMapXY(BG_A, wallTileAttr, x, y + i);
                    mazeWalls[wallCount].x = x;
                    mazeWalls[wallCount].y = y + i;
                    wallCount++;
                }
            }
        } else {
            x = 2 + (random() % (GRID_WIDTH - length - 3));
            y = 3 + (random() % (GRID_HEIGHT - 5));
            for (u16 i = 0; i < length && x + i < GRID_WIDTH - 1 && wallCount < MAX_WALLS * 5; i++) {
                u16 valid = TRUE;
                for (u16 j = 0; j < snakeLength; j++) {
                    if (x + i == snakeBody[j].x && y == snakeBody[j].y) valid = FALSE;
                }
                if (valid) {
                    VDP_setTileMapXY(BG_A, wallTileAttr, x + i, y);
                    mazeWalls[wallCount].x = x + i;
                    mazeWalls[wallCount].y = y;
                    wallCount++;
                }
            }
        }
    }
    
    // Build free tile list
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
            u16 isSnake = FALSE;
            for (u16 i = 0; i < snakeLength; i++) {
                if (snakeBody[i].x == x && snakeBody[i].y == y) {
                    isSnake = TRUE;
                    break;
                }
            }
            if (!isWall && !isSnake) {
                freeTiles[freeTileCount].x = x;
                freeTiles[freeTileCount].y = y;
                freeTileCount++;
            }
        }
    }
    for (u16 i = 0; i < NUM_PORTALS; i++) {
        freeTiles[freeTileCount].x = portals[i].entry.x;
        freeTiles[freeTileCount].y = portals[i].entry.y;
        freeTileCount++;
        freeTiles[freeTileCount].x = portals[i].exit.x;
        freeTiles[freeTileCount].y = portals[i].exit.y;
        freeTileCount++;
    }
    
    // Load head sprite frames
    if (!spriteHead) {
        Animation* headAnim = snake_head_sprite.animations[0];
        for (u16 i = 0; i < 4; i++) {
            TileSet* tileset = headAnim->frames[i]->tileset;
            VDP_loadTileSet(tileset, vramIndex, DMA);
            headVramIndexes[i] = vramIndex;
            vramIndex += tileset->numTile;
        }
        spriteHead = SPR_addSprite(&snake_head_sprite,
                                  snakeBody[0].x * SNAKE_TILE_SIZE,
                                  snakeBody[0].y * SNAKE_TILE_SIZE,
                                  TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
        SPR_setAutoTileUpload(spriteHead, FALSE);
        SPR_setFrame(spriteHead, direction);
        SPR_setVRAMTileIndex(spriteHead, headVramIndexes[direction]);
    }
    
    // Load body sprite frames
    Animation* bodyAnim = snake_body_sprite.animations[0];
    if (!spriteBody[0]) {
        for (u16 i = 0; i < 2; i++) {
            TileSet* tileset = bodyAnim->frames[i]->tileset;
            VDP_loadTileSet(tileset, vramIndex, DMA);
            bodyVramIndexes[i] = vramIndex;
            vramIndex += tileset->numTile;
        }
        for (u16 i = 1; i < snakeLength; i++) {
            spriteBody[i-1] = SPR_addSprite(&snake_body_sprite,
                                           snakeBody[i].x * SNAKE_TILE_SIZE,
                                           snakeBody[i].y * SNAKE_TILE_SIZE,
                                           TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
            SPR_setAutoTileUpload(spriteBody[i-1], FALSE);
            SPR_setFrame(spriteBody[i-1], (snakeBody[i-1].x != snakeBody[i].x) ? 0 : 1);
            SPR_setVRAMTileIndex(spriteBody[i-1], bodyVramIndexes[(snakeBody[i-1].x != snakeBody[i].x) ? 0 : 1]);
        }
    }
    
    // Place initial food
    generateFood();
    if (!spriteFood) {
        spriteFood = SPR_addSprite(&food_sprite,
                                  food.x * SNAKE_TILE_SIZE,
                                  food.y * SNAKE_TILE_SIZE,
                                  TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
    } else {
        SPR_setPosition(spriteFood, food.x * SNAKE_TILE_SIZE, food.y * SNAKE_TILE_SIZE);
    }
    
    updateLevelDisplay();
}

// Displays intro screen with title
static void showIntroScreen(void) {
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    
    VDP_loadTileSet(intro.tileset, TILE_USER_INDEX, DMA);
    VDP_setMapEx(BG_B, intro.tilemap, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_USER_INDEX),
                 0, 0, 0, 0, 40, 28);
    
    VDP_drawText("AI-MAZE-ING SNAKE", 12, 2);
    VDP_drawText("START TO PLAY", 14, 6);
    VDP_drawText("B TO TOGGLE MUSIC", 12, 10);
    
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

// Updates intro screen animation (blinking text)
static void updateIntroScreen(void) {
    introAnimFrame++;
    if (introAnimFrame % 60 < 30) {
        VDP_drawText("START TO PLAY", 14, 6);
    } else {
        VDP_clearText(14, 6, 13);
    }
}

// Transitions from intro to gameplay with Level 1 transition
static void startGame(void) {
    initGame();
    // gameState set to STATE_LEVEL_TRANSITION in initGame()
}

// Processes player input from joypad
static void handleInput(void) {
    const u16 joy = JOY_readJoypad(JOY_1);
    const u16 startPressed = joy & BUTTON_START;
    const u16 bPressed = joy & BUTTON_B;
    
    if (startPressed && !prevStartState) {
        if (gameState == STATE_INTRO) startGame();
        else if (gameState == STATE_PLAYING) togglePause();
        else if (gameState == STATE_GAMEOVER) showIntroScreen();
    }
    prevStartState = startPressed;
    
    static u16 prevBState = FALSE;
    if (gameState == STATE_INTRO && bPressed && !prevBState) {
        musicEnabled = !musicEnabled;
        if (!musicEnabled) {
            PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
            PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
        }
    }
    prevBState = bPressed;
    
    if (gameState == STATE_PLAYING && !paused) {
        if (joy & BUTTON_UP && direction != DIR_DOWN) nextDirection = DIR_UP;
        else if (joy & BUTTON_RIGHT && direction != DIR_LEFT) nextDirection = DIR_RIGHT;
        else if (joy & BUTTON_DOWN && direction != DIR_UP) nextDirection = DIR_DOWN;
        else if (joy & BUTTON_LEFT && direction != DIR_RIGHT) nextDirection = DIR_LEFT;
    }
}

// Updates game logic (movement, collisions, level progression)
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
    
    // Portal teleportation
    for (u16 i = 0; i < NUM_PORTALS; i++) {
        if (newHeadX == portals[i].entry.x && newHeadY == portals[i].entry.y) {
            newHeadX = portals[i].exit.x;
            newHeadY = portals[i].exit.y;
            break;
        }
        else if (newHeadX == portals[i].exit.x && newHeadY == portals[i].exit.y) {
            newHeadX = portals[i].entry.x;
            newHeadY = portals[i].entry.y;
            break;
        }
    }
    
    // Collision detection
    if ((newHeadX <= 0 || newHeadX >= GRID_WIDTH - 1 || newHeadY <= 1 || newHeadY >= GRID_HEIGHT - 1) &&
        !((newHeadX == portals[0].entry.x && newHeadY == portals[0].entry.y) ||
          (newHeadX == portals[0].exit.x && newHeadY == portals[0].exit.y) ||
          (newHeadX == portals[1].entry.x && newHeadY == portals[1].entry.y) ||
          (newHeadX == portals[1].exit.x && newHeadY == portals[1].exit.y)) ||
        checkCollision(newHeadX, newHeadY)) {
        gameState = STATE_GAMEOVER;
        showGameOver();
        return;
    }
    
    // Food collision handling
    if (newHeadX == food.x && newHeadY == food.y) {
        foodEatenThisLevel++;
        if (snakeLength < SNAKE_MAX_LENGTH) {
            for (u16 i = snakeLength; i > 0; i--) {
                snakeBody[i] = snakeBody[i - 1];
            }
            if (snakeLength > 1 && !spriteBody[snakeLength-2]) {
                spriteBody[snakeLength-2] = SPR_addSprite(&snake_body_sprite,
                                                         -16, -16,
                                                         TILE_ATTR(PAL0, TRUE, FALSE, FALSE));
                if (!spriteBody[snakeLength-2]) {
                    snakeLength--;
                    VDP_drawText("SPRITE LIMIT!", 14, 10);
                    return;
                }
                SPR_setAutoTileUpload(spriteBody[snakeLength-2], FALSE);
                SPR_setFrame(spriteBody[snakeLength-2], 0);
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
        
        if (foodEatenThisLevel >= foodTarget) { // Level complete
            currentLevel++;
            foodEatenThisLevel = 0;
            foodTarget = 5 + (currentLevel - 1) * 5;
            if (frameDelay > MIN_DELAY) frameDelay--;
            
            // Trigger transition state
            gameState = STATE_LEVEL_TRANSITION;
            transitionTimer = TRANSITION_DURATION;
            jingleIndex = 0;      // Start level-up jingle
            jingleCounter = 0;
            char levelText[8];
            sprintf(levelText, "LEVEL %d", currentLevel);
            VDP_drawText(levelText, 16, 12); // Initial display before blinking
        } else {
            generateFood();
            SPR_setPosition(spriteFood, food.x * SNAKE_TILE_SIZE, food.y * SNAKE_TILE_SIZE);
        }
        
        updateLevelDisplay();
    } else { // Move without eating
        for (u16 i = snakeLength - 1; i > 0; i--) {
            snakeBody[i] = snakeBody[i - 1];
        }
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
    
    snakeBody[0].x = newHeadX;
    snakeBody[0].y = newHeadY;
}

// Renders game sprites
static void drawGame(void) {
    SPR_setPosition(spriteHead, snakeBody[0].x * SNAKE_TILE_SIZE, snakeBody[0].y * SNAKE_TILE_SIZE);
    switch (direction) {
        case DIR_DOWN:  SPR_setFrame(spriteHead, 0); SPR_setVRAMTileIndex(spriteHead, headVramIndexes[0]); break;
        case DIR_RIGHT: SPR_setFrame(spriteHead, 1); SPR_setVRAMTileIndex(spriteHead, headVramIndexes[1]); break;
        case DIR_UP:    SPR_setFrame(spriteHead, 2); SPR_setVRAMTileIndex(spriteHead, headVramIndexes[2]); break;
        case DIR_LEFT:  SPR_setFrame(spriteHead, 3); SPR_setVRAMTileIndex(spriteHead, headVramIndexes[3]); break;
    }
    
    for (u16 i = 1; i < snakeLength; i++) {
        if (spriteBody[i-1]) {
            SPR_setPosition(spriteBody[i-1], snakeBody[i].x * SNAKE_TILE_SIZE, snakeBody[i].y * SNAKE_TILE_SIZE);
            s16 dx = snakeBody[i-1].x - snakeBody[i].x;
            s16 dy = snakeBody[i-1].y - snakeBody[i].y;
            if (dx != 0) {
                SPR_setFrame(spriteBody[i-1], 0);
                SPR_setVRAMTileIndex(spriteBody[i-1], bodyVramIndexes[0]);
            } else if (dy != 0) {
                SPR_setFrame(spriteBody[i-1], 1);
                SPR_setVRAMTileIndex(spriteBody[i-1], bodyVramIndexes[1]);
            }
        }
    }
}

// Places new food at a random free tile
static void generateFood(void) {
    if (freeTileCount == 0) {
        gameState = STATE_GAMEOVER;
        VDP_drawText("YOU WIN!", 16, 10);
        return;
    }
    
    u16 pick = random() % freeTileCount;
    food.x = freeTiles[pick].x;
    food.y = freeTiles[pick].y;
    
    freeTiles[pick] = freeTiles[freeTileCount - 1];
    freeTileCount--;
}

// Checks collisions with snake body or walls
static u16 checkCollision(s16 x, s16 y) {
    for (u16 i = 1; i < snakeLength; i++) {
        if (snakeBody[i].x == x && snakeBody[i].y == y) return TRUE;
    }
    for (u16 i = 0; i < wallCount; i++) {
        if (mazeWalls[i].x == x && mazeWalls[i].y == y) return TRUE;
    }
    return FALSE;
}

// Displays game over screen with animation
static void showGameOver(void) {
    VDP_drawText("GAME OVER", 15, 10);
    VDP_drawText("START TO PLAY AGAIN", 11, 12);
    VDP_drawText("FINAL SCORE:", 14, 14);
    char scoreText[5];
    sprintf(scoreText, "%d", score);
    VDP_drawText(scoreText, 19 - (score >= 10 ? (score >= 100 ? (score >= 1000 ? 3 : 2) : 1) : 0), 16);
    char levelText[12];
    sprintf(levelText, "LEVEL: %d", currentLevel);
    VDP_drawText(levelText, 15, 18);
    
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
    PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
    
    waitMs(200);
    
    Note gameOverTune[] = {
        {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_C4, 8}, {NOTE_G3, 12}, {NOTE_REST, 8}
    };
    const u16 tuneSize = 5;
    u16 tuneIndex = 0;
    u16 tuneCounter = 0;
    
    for (u16 i = snakeLength - 1; i > 0; i--) {
        if (spriteBody[i-1]) {
            SPR_releaseSprite(spriteBody[i-1]);
            spriteBody[i-1] = NULL;
            SPR_update();
            SYS_doVBlankProcess();
            waitMs(50);
        }
        
        if (tuneCounter == 0 && tuneIndex < tuneSize) {
            PSG_setFrequency(1, gameOverTune[tuneIndex].frequency);
            PSG_setEnvelope(1, gameOverTune[tuneIndex].frequency != NOTE_REST ? PSG_ENVELOPE_MAX / 4 : PSG_ENVELOPE_MIN);
            tuneCounter = gameOverTune[tuneIndex].baseDuration * 2;
            tuneIndex++;
        }
        if (tuneCounter > 0) tuneCounter--;
    }
    
    if (spriteHead) {
        SPR_releaseSprite(spriteHead);
        spriteHead = NULL;
        SPR_update();
        SYS_doVBlankProcess();
        waitMs(50);
    }
    
    if (spriteFood) {
        SPR_releaseSprite(spriteFood);
        spriteFood = NULL;
        SPR_update();
        SYS_doVBlankProcess();
        waitMs(50);
    }
    
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

// Plays "chomp" sound effect
static void playEatSound(void) {
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1000);
    waitMs(20);
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX / 2);
    PSG_setFrequency(0, 400);
    waitMs(30);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
}

// Updates chiptune music and level-up jingle playback
static void updateMusic(void) {
    if (gameState == STATE_GAMEOVER || (gameState == STATE_PLAYING && paused)) {
        PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
        PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
        PSG_setEnvelope(3, PSG_ENVELOPE_MIN); // Silence jingle channel
        return;
    }
    
    if (!musicEnabled) {
        PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
        PSG_setEnvelope(2, PSG_ENVELOPE_MIN);
        PSG_setEnvelope(3, PSG_ENVELOPE_MIN);
        return;
    }
    
    u16 melodyVolume = PSG_ENVELOPE_MAX / 8;
    u16 bassVolume = PSG_ENVELOPE_MAX / 16;
    u16 jingleVolume = PSG_ENVELOPE_MAX / 4; // Louder for jingle
    
    static Note introMelody[MELODY_SIZE] = {
        {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_A4, 8}, {NOTE_G4, 8},
        {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_A4, 12}, {NOTE_REST, 8},
        {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_G4, 8}, {NOTE_A4, 8},
        {NOTE_G4, 8}, {NOTE_E4, 8}, {NOTE_A4, 12}, {NOTE_REST, 8}
    };
    
    Note* currentMelody = (gameState == STATE_INTRO) ? introMelody : melody;
    u16 tempoFactor = (gameState == STATE_INTRO) ? 12 : min((frameDelay * 10) / INITIAL_DELAY, MAX_TEMPO_FACTOR);
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
    
    // Update level-up jingle (channel 3) during transition
    if (gameState == STATE_LEVEL_TRANSITION && transitionTimer > 0) {
        if (jingleCounter == 0 && jingleIndex < LEVEL_UP_SIZE) {
            PSG_setFrequency(3, levelUpJingle[jingleIndex].frequency);
            PSG_setEnvelope(3, levelUpJingle[jingleIndex].frequency != NOTE_REST ? jingleVolume : PSG_ENVELOPE_MIN);
            jingleCounter = levelUpJingle[jingleIndex].baseDuration * 3; // Slower for effect
            jingleIndex++;
        }
        if (jingleCounter > 0) jingleCounter--;
    } else {
        PSG_setEnvelope(3, PSG_ENVELOPE_MIN); // Silence jingle outside transition
    }
}

// Toggles pause state
static void togglePause(void) {
    paused = !paused;
    const u16 sandTileAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, sandVramIndex);
    if (paused) {
        VDP_drawText("PAUSE", 17, 14);
    } else {
        VDP_clearText(17, 14, 5);
        for (u16 x = 17; x < 22; x++) {
            VDP_setTileMapXY(BG_A, sandTileAttr, x, 14);
        }
    }
}

// Updates level and food progress display
static void updateLevelDisplay(void) {
    char levelText[20];
    sprintf(levelText, "LEVEL %d: %d/%d", currentLevel, foodEatenThisLevel, foodTarget);
    VDP_clearText(GRID_WIDTH - 14, 0, 14);
    VDP_drawText(levelText, GRID_WIDTH - strlen(levelText) - 1, 0);
}