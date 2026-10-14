#ifndef __PLAYERAI_H
#define __PLAYERAI_H

// Player AI (F1 toggles it).
//
// The AI plays the game through the same CONTROLS structure the keyboard and mouse fill in, so every game rule
// (attack timing, doors, pickups, spells) behaves exactly as it does for a human. Each frame it:
//   - picks a goal: nearby monsters first, then treasure / potions / weapons, then closed doors, then the level exit,
//     and explores unvisited space when nothing is left,
//   - steers toward the goal with the same context steering idea as MonsterAI (whisker raycasts against the level
//     triangles, ledge probes and missile avoidance),
//   - turns the camera (angy / look_up_ang), walks, strafes, jumps when stuck, opens doors, swings the weapon,
//     casts scrolls at range and heals when hurt,
//   - gets back up after dying.

struct Controls_typ;

// Call after the input devices were read and before the controls are applied. Does nothing while the AI is off.
void PlayerAIUpdate(Controls_typ *controls, float fElapsedTime);

// Forget targets, bans and the explored map (call on level load). Keeps the enabled state.
void PlayerAIReset();

void PlayerAISetEnabled(bool enabled);
bool PlayerAIIsEnabled();
void PlayerAIToggle();

#endif
