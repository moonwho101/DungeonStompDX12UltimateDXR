
#ifndef __SPEEDGLOBALS_H
#define __SPEEDGLOBALS_H


// ============================================================================
// GAMEPLAY & MOVEMENT CONSTANTS (Tweakable for Fast-Paced FPS Feel)
// ============================================================================

// Player Movement Speeds
// Base player speed is 3x original (~900 units/sec)
constexpr float PLAYER_BASE_SPEED = 350.0f;

// Sprint multiplier when holding Shift key (6x original = 1800 units/sec)
constexpr float PLAYER_SPRINT_MULTIPLIER = 2.0f;
constexpr float PLAYER_SPRINT_SPEED = PLAYER_BASE_SPEED * PLAYER_SPRINT_MULTIPLIER; // 1800.0f

// Player Acceleration (responsive FPS acceleration)
constexpr float PLAYER_ACCELERATION = 6000.0f;

// Keyboard / Button Turning Speed (Degrees per second)
constexpr float KEYBOARD_TURN_SPEED_DEG = 315.0f;

// Mouse Control & Smoothing
constexpr float MOUSE_SENSITIVITY_DIVIDER = 18.0f;   // Lower = higher mouse sensitivity
constexpr float MOUSE_SMOOTHING_SPRINGINESS = 45.0f; // Higher = tighter, responsive mouse look (no floaty lag)

// Monster Movement Speeds & AI
constexpr float MONSTER_BASE_SPEED = 200.0f; // Base monster speed (original: 165.0f)
constexpr float MONSTER_SPEED_SCALE = 5.0f;  // Monster speed stat scaling (original: 2.0f)
constexpr float MONSTER_TURN_RATE = 18.0f;   // Monster AI turn rate in radians/sec (original: 9.5f)

#endif