#ifndef __MONSTERAI_H
#define __MONSTERAI_H

// Monster steering AI.
//
// Context steering: every monster owns an "interest" map (where it wants to go) and a "danger" map
// (where it should not go) sampled over a ring of directions. The danger map is filled by raycasting
// whiskers against the level triangles, plus ledge probes and neighbour separation. The final heading is
// picked from the interest map after masking the dangerous slots.

struct MonsterSteering {
	float dirX;      // unit move direction on the XZ plane
	float dirZ;      // unit move direction on the XZ plane
	float speedScale; // 0..1 multiplier for the monster's normal walk speed
	float faceAngle; // rot_angle in degrees the monster should use this frame
	bool hasLineOfSight; // clear ray between the monster and the player
	bool kiter;          // prefers to keep range and shoot
};

// Call once per frame before the monsters are updated; resets the per frame think budget and advances simulation time.
void MonsterAIBeginFrame(float fElapsedTime = 0.0f);

// Updates one monster. 'walking' is true while the monster is allowed to move this frame.
MonsterSteering MonsterAIUpdate(int monsterIndex, float fElapsedTime, bool walking);

// Forget every monster's AI memory and optionally re-seed the deterministic generator (call on level load / demo reset).
void MonsterAIReset(unsigned int seed = 0x9E3779B9u);

#endif
