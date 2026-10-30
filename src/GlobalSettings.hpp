
#ifndef __GLOBALSETTINGS_H
#define __GLOBALSETTINGS_H

#define MAX_MISSLE 100

#define MAX_NUM_QUADS 900000

#define MAX_NUM_TEXTURES 650

#define MAX_NUM_PLAYERS 5000
#define MAX_NUM_PMMODELS 240

#define MAX_NUM_ITEMS 5000
#define MAX_NUM_3DS 5000
#define MAX_NUM_MONSTERS 5000
#define MAX_NUM_GUNS 300
#define MAX_OBJECTLIST 25000

#define MAX_OBJECTLIGHTS 10000
#define MAX_OBJECTDOORS 10000

#define MAX_OBJECTSOUNDS 10000

#define MaxLights 32

#define MaxRectangle 10

// ============================================================================
// GAMEPLAY & MOVEMENT CONSTANTS (Tweakable for Fast-Paced FPS Feel)
// ============================================================================

// Player Movement Speeds
// Base player speed is 3x original (~900 units/sec)
constexpr float PLAYER_BASE_SPEED = 900.0f;

// Sprint multiplier when holding Shift key (6x original = 1800 units/sec)
constexpr float PLAYER_SPRINT_MULTIPLIER = 2.0f;
constexpr float PLAYER_SPRINT_SPEED = PLAYER_BASE_SPEED * PLAYER_SPRINT_MULTIPLIER; // 1800.0f

// Player Acceleration (responsive FPS acceleration)
constexpr float PLAYER_ACCELERATION = 6000.0f;

// Keyboard / Button Turning Speed (Degrees per second)
constexpr float KEYBOARD_TURN_SPEED_DEG = 315.0f;

// Mouse Control & Smoothing
constexpr float MOUSE_SENSITIVITY_DIVIDER = 12.0f;    // Lower = higher mouse sensitivity
constexpr float MOUSE_SMOOTHING_SPRINGINESS = 45.0f;  // Higher = tighter, responsive mouse look (no floaty lag)

// Monster Movement Speeds & AI
constexpr float MONSTER_BASE_SPEED = 400.0f; // Base monster speed (original: 165.0f)
constexpr float MONSTER_SPEED_SCALE = 5.0f;  // Monster speed stat scaling (original: 2.0f)
constexpr float MONSTER_TURN_RATE = 18.0f;   // Monster AI turn rate in radians/sec (original: 9.5f)

#endif