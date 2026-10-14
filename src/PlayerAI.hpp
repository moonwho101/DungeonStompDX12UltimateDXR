#ifndef __PLAYERAI_HPP
#define __PLAYERAI_HPP

#include "DirectInput.hpp"

// Player AI Controller
// Allows the game to be played automatically when toggled (F1).
// Searches for monsters to fight, items/treasure to collect, and doors/switches to open.

void TogglePlayerAI();
bool IsPlayerAIEnabled();
void SetPlayerAIEnabled(bool enable);

// Resets Player AI internal state (call on level load or demo reset)
void PlayerAIReset();

// Main update function: populates Controls and adjusts view angles (angy, look_up_ang)
void PlayerAIUpdate(CONTROLS *pControls, float fElapsedTime);

#endif // __PLAYERAI_HPP
