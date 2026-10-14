#include "PlayerAI.hpp"
#include "d3dtypes.h"
#include "world.hpp"
#include "GlobalSettings.hpp"
#include "GameLogic.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern PLAYER *monster_list;
extern PLAYER *item_list;
extern int num_monsters;
extern int itemlistcount;
extern int trueplayernum;
extern PLAYER *player_list;
extern D3DVALUE angy;
extern float look_up_ang;
extern char gActionMessage[2048];
extern int endc;
extern int src_collide[MAX_NUM_QUADS];
extern D3DVERTEX2 *src_v;
extern DOOR door[200];
extern int num_doors;
extern PLAYER *player_list2;
extern int num_players2;
extern int usespell;

namespace {

bool g_bPlayerAIEnabled = false;

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717958f;
constexpr float kDegToRad = 0.017453292519943295f;
constexpr float kRadToDeg = 57.29577951308232f;

constexpr float kAttackDistance = 350.0f;
constexpr float kMeleeDistance = 75.0f;
constexpr float kDoorInteractDistance = 140.0f;

struct Vector3 {
	float x, y, z;
};

inline Vector3 VecSub(const Vector3 &a, const Vector3 &b) {
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline float VecDot(const Vector3 &a, const Vector3 &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3 VecCross(const Vector3 &a, const Vector3 &b) {
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline float VecLength(const Vector3 &v) {
	return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline float ClampFloat(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

// Ray-triangle test
bool RayTriIntersect(const Vector3 &o, const Vector3 &d, const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, float &tOut) {
	Vector3 e1 = VecSub(v1, v0);
	Vector3 e2 = VecSub(v2, v0);
	Vector3 p = VecCross(d, e2);
	float det = VecDot(e1, p);
	if (fabsf(det) < 1e-6f)
		return false;
	float invDet = 1.0f / det;
	Vector3 s = VecSub(o, v0);
	float u = VecDot(s, p) * invDet;
	if (u < -1e-4f || u > 1.0001f)
		return false;
	Vector3 q = VecCross(s, e1);
	float v = VecDot(d, q) * invDet;
	if (v < -1e-4f || (u + v) > 1.0001f)
		return false;
	float t = VecDot(e2, q) * invDet;
	if (t < 0.001f)
		return false;
	tOut = t;
	return true;
}

bool CheckLineOfSight(const Vector3 &from, const Vector3 &to) {
	Vector3 dir = VecSub(to, from);
	float len = VecLength(dir);
	if (len < 1e-3f)
		return true;
	dir.x /= len;
	dir.y /= len;
	dir.z /= len;

	for (int i = 0; i + 2 < endc; i += 3) {
		if (src_collide[i] != 1)
			continue;

		Vector3 v0 = { src_v[i].x, src_v[i].y, src_v[i].z };
		Vector3 v1 = { src_v[i + 1].x, src_v[i + 1].y, src_v[i + 1].z };
		Vector3 v2 = { src_v[i + 2].x, src_v[i + 2].y, src_v[i + 2].z };

		// Check vertical height of tri
		float miny = fminf(v0.y, fminf(v1.y, v2.y));
		float maxy = fmaxf(v0.y, fmaxf(v1.y, v2.y));
		if (maxy - miny <= 25.0f)
			continue; // skip small stairs/floors

		Vector3 e1 = VecSub(v1, v0);
		Vector3 e2 = VecSub(v2, v0);
		Vector3 n = VecCross(e1, e2);
		float nl = VecLength(n);
		if (nl < 1e-4f)
			continue;
		if (fabsf(n.y / nl) >= 0.55f)
			continue; // floor/ramp

		float t;
		if (RayTriIntersect(from, dir, v0, v1, v2, t) && t < len) {
			return false; // blocked by wall
		}
	}
	return true;
}

// Raycast against walls to get distance
float RaycastWall(const Vector3 &o, float dirX, float dirZ, float maxDist) {
	Vector3 d = { dirX, 0.0f, dirZ };
	float best = maxDist;

	for (int i = 0; i + 2 < endc; i += 3) {
		if (src_collide[i] != 1)
			continue;

		Vector3 v0 = { src_v[i].x, src_v[i].y, src_v[i].z };
		Vector3 v1 = { src_v[i + 1].x, src_v[i + 1].y, src_v[i + 1].z };
		Vector3 v2 = { src_v[i + 2].x, src_v[i + 2].y, src_v[i + 2].z };

		float miny = fminf(v0.y, fminf(v1.y, v2.y));
		float maxy = fmaxf(v0.y, fmaxf(v1.y, v2.y));
		if (maxy - miny <= 25.0f)
			continue;

		Vector3 e1 = VecSub(v1, v0);
		Vector3 e2 = VecSub(v2, v0);
		Vector3 n = VecCross(e1, e2);
		float nl = VecLength(n);
		if (nl < 1e-4f || fabsf(n.y / nl) >= 0.55f)
			continue;

		float t;
		if (RayTriIntersect(o, d, v0, v1, v2, t) && t < best) {
			best = t;
		}
	}
	return best;
}

// Internal Player AI State
struct AIState {
	float wanderTimer = 0.0f;
	float wanderAngle = 0.0f;
	float attackTimer = 0.0f;
	float doorCheckTimer = 0.0f;
	float prevX = 0.0f, prevZ = 0.0f;
	float stuckTimer = 0.0f;
	int strafeDir = 1;
};

AIState g_aiState;

} // namespace

void TogglePlayerAI() {
	g_bPlayerAIEnabled = !g_bPlayerAIEnabled;
	sprintf_s(gActionMessage, "Player AI %s", g_bPlayerAIEnabled ? "ACTIVE (F1 to disable)" : "DISABLED");
	if (g_bPlayerAIEnabled) {
		PlayerAIReset();
	}
}

bool IsPlayerAIEnabled() {
	return g_bPlayerAIEnabled;
}

void SetPlayerAIEnabled(bool enable) {
	g_bPlayerAIEnabled = enable;
	if (g_bPlayerAIEnabled) {
		PlayerAIReset();
	}
}

void PlayerAIReset() {
	g_aiState = AIState();
	if (player_list != nullptr && trueplayernum >= 0) {
		g_aiState.prevX = player_list[trueplayernum].x;
		g_aiState.prevZ = player_list[trueplayernum].z;
	}
}

void PlayerAIUpdate(CONTROLS *pControls, float fElapsedTime) {
	if (!g_bPlayerAIEnabled || pControls == nullptr || player_list == nullptr)
		return;

	PLAYER &p = player_list[trueplayernum];
	if (p.bIsPlayerAlive == FALSE)
		return;

	float dt = ClampFloat(fElapsedTime, 0.001f, 0.1f);
	Vector3 px = { p.x, p.y + 30.0f, p.z };

	// 1. Check for nearby doors or door switches that need opening
	bool nearDoor = false;
	for (int i = 0; i < num_doors; i++) {
		if (door[i].open == 0 || door[i].open == 2) {
			float dx = door[i].x - p.x;
			float dy = door[i].y - p.y;
			float dz = door[i].z - p.z;
			float dist = sqrtf(dx * dx + dy * dy + dz * dz);
			if (dist <= kDoorInteractDistance) {
				nearDoor = true;
				break;
			}
		}
	}
	if (!nearDoor && player_list2 != nullptr) {
		for (int i = 0; i < num_players2; i++) {
			if (player_list2[i].model_id == 36 || strstr(player_list2[i].rname, "cdoorclosed") != NULL) {
				float dx = player_list2[i].x - p.x;
				float dy = player_list2[i].y - p.y;
				float dz = player_list2[i].z - p.z;
				float dist = sqrtf(dx * dx + dy * dy + dz * dz);
				if (dist <= kDoorInteractDistance) {
					nearDoor = true;
					break;
				}
			}
		}
	}

	if (nearDoor) {
		pControls->opendoor = TRUE;
	} else {
		pControls->opendoor = FALSE;
	}

	// 2. Scan for monsters (Target selection)
	int bestMonsterIdx = -1;
	float bestMonsterDist = 99999.0f;
	bool monsterInLOS = false;

	for (int i = 0; i < num_monsters; i++) {
		const PLAYER &m = monster_list[i];
		if (m.bIsPlayerAlive == FALSE || m.bIsPlayerValid == FALSE || m.health <= 0)
			continue;

		float dx = m.x - p.x;
		float dy = m.y - p.y;
		float dz = m.z - p.z;
		float dist = sqrtf(dx * dx + dy * dy + dz * dz);

		if (dist > 1200.0f)
			continue;

		Vector3 mx = { m.x, m.y + 30.0f, m.z };
		bool los = CheckLineOfSight(px, mx);

		// Prioritize monsters in line of sight
		if (los && (!monsterInLOS || dist < bestMonsterDist)) {
			bestMonsterIdx = i;
			bestMonsterDist = dist;
			monsterInLOS = true;
		} else if (!monsterInLOS && dist < bestMonsterDist) {
			bestMonsterIdx = i;
			bestMonsterDist = dist;
		}
	}

	// 3. Scan for items/treasure if no monster in sight or monster is far
	int bestItemIdx = -1;
	float bestItemDist = 99999.0f;

	if (!monsterInLOS || bestMonsterDist > 300.0f) {
		for (int i = 0; i < itemlistcount; i++) {
			const PLAYER &item = item_list[i];
			if (item.bIsPlayerValid == FALSE || item.bIsPlayerAlive == FALSE)
				continue;

			float dx = item.x - p.x;
			float dy = item.y - p.y;
			float dz = item.z - p.z;
			float dist = sqrtf(dx * dx + dy * dy + dz * dz);

			if (dist < bestItemDist && dist < 800.0f) {
				Vector3 ix = { item.x, item.y + 15.0f, item.z };
				if (CheckLineOfSight(px, ix)) {
					bestItemIdx = i;
					bestItemDist = dist;
				}
			}
		}
	}

	// 4. Determine AI movement target and action
	float targetX = 0.0f, targetZ = 0.0f;
	bool hasTarget = false;
	bool isCombat = false;

	if (bestMonsterIdx >= 0) {
		const PLAYER &m = monster_list[bestMonsterIdx];
		targetX = m.x;
		targetZ = m.z;
		hasTarget = true;
		isCombat = true;
	} else if (bestItemIdx >= 0) {
		const PLAYER &item = item_list[bestItemIdx];
		targetX = item.x;
		targetZ = item.z;
		hasTarget = true;
	}

	// Stuck detection
	float moveDist = sqrtf((p.x - g_aiState.prevX) * (p.x - g_aiState.prevX) + (p.z - g_aiState.prevZ) * (p.z - g_aiState.prevZ));
	g_aiState.prevX = p.x;
	g_aiState.prevZ = p.z;

	if (moveDist < 5.0f * dt && pControls->bForward) {
		g_aiState.stuckTimer += dt;
	} else {
		g_aiState.stuckTimer = fmaxf(0.0f, g_aiState.stuckTimer - dt);
	}

	float moveDirX = 0.0f, moveDirZ = 0.0f;

	if (hasTarget) {
		float dx = targetX - p.x;
		float dz = targetZ - p.z;
		float dist = sqrtf(dx * dx + dz * dz);
		if (dist > 1e-3f) {
			dx /= dist;
			dz /= dist;
		}

		if (isCombat) {
			// Aim view directly at monster
			float desiredYaw = atan2f(dx, dz) * kRadToDeg;
			while (desiredYaw < 0.0f)
				desiredYaw += 360.0f;
			while (desiredYaw >= 360.0f)
				desiredYaw -= 360.0f;

			// Smoothly rotate camera yaw
			float yawDiff = desiredYaw - angy;
			while (yawDiff > 180.0f)
				yawDiff -= 360.0f;
			while (yawDiff < -180.0f)
				yawDiff += 360.0f;

			angy += ClampFloat(yawDiff, -360.0f * dt, 360.0f * dt);
			if (angy < 0.0f)
				angy += 360.0f;
			if (angy >= 360.0f)
				angy -= 360.0f;

			// Aim pitch slightly
			const PLAYER &m = monster_list[bestMonsterIdx];
			float dy = (m.y + 20.0f) - p.y;
			float desiredPitch = -atan2f(dy, dist) * kRadToDeg;
			look_up_ang += ClampFloat(desiredPitch - look_up_ang, -180.0f * dt, 180.0f * dt);

			// Attack decision
			if (monsterInLOS && dist <= kAttackDistance) {
				pControls->bFire = TRUE;
				if (usespell == 1) {
					pControls->spell = TRUE;
				}
			} else {
				pControls->bFire = FALSE;
				pControls->spell = FALSE;
			}

			// Movement in combat
			if (dist > kMeleeDistance) {
				moveDirX = dx;
				moveDirZ = dz;
			} else {
				// Circle around target in melee
				moveDirX = -dz * g_aiState.strafeDir;
				moveDirZ = dx * g_aiState.strafeDir;
			}
		} else {
			// Moving toward item
			float desiredYaw = atan2f(dx, dz) * kRadToDeg;
			while (desiredYaw < 0.0f)
				desiredYaw += 360.0f;
			while (desiredYaw >= 360.0f)
				desiredYaw -= 360.0f;

			float yawDiff = desiredYaw - angy;
			while (yawDiff > 180.0f)
				yawDiff -= 360.0f;
			while (yawDiff < -180.0f)
				yawDiff += 360.0f;

			angy += ClampFloat(yawDiff, -270.0f * dt, 270.0f * dt);
			pControls->bFire = FALSE;
			pControls->spell = FALSE;

			moveDirX = dx;
			moveDirZ = dz;
		}
	} else {
		// Exploration / Wandering
		g_aiState.wanderTimer -= dt;
		if (g_aiState.wanderTimer <= 0.0f || g_aiState.stuckTimer > 0.5f) {
			g_aiState.wanderAngle += (rand() % 120 - 60) * kDegToRad;
			g_aiState.wanderTimer = 2.0f + (rand() % 200) * 0.01f;
			if (g_aiState.stuckTimer > 0.5f) {
				g_aiState.wanderAngle += 180.0f * kDegToRad;
				g_aiState.stuckTimer = 0.0f;
				g_aiState.strafeDir = -g_aiState.strafeDir;
			}
		}

		moveDirX = sinf(g_aiState.wanderAngle);
		moveDirZ = cosf(g_aiState.wanderAngle);

		float desiredYaw = atan2f(moveDirX, moveDirZ) * kRadToDeg;
		while (desiredYaw < 0.0f)
			desiredYaw += 360.0f;
		while (desiredYaw >= 360.0f)
			desiredYaw -= 360.0f;

		float yawDiff = desiredYaw - angy;
		while (yawDiff > 180.0f)
			yawDiff -= 360.0f;
		while (yawDiff < -180.0f)
			yawDiff += 360.0f;

		angy += ClampFloat(yawDiff, -180.0f * dt, 180.0f * dt);
		pControls->bFire = FALSE;
		pControls->spell = FALSE;
	}

	// 5. Context steering raycast clearance against walls
	constexpr int kRays = 8;
	float bestClearance = -1.0f;
	float bestMoveX = moveDirX, bestMoveZ = moveDirZ;

	for (int i = 0; i < kRays; i++) {
		float angleOffset = (i - kRays / 2) * (45.0f / kRays) * kDegToRad;
		float rx = moveDirX * cosf(angleOffset) - moveDirZ * sinf(angleOffset);
		float rz = moveDirX * sinf(angleOffset) + moveDirZ * cosf(angleOffset);

		float clearDist = RaycastWall(px, rx, rz, 120.0f);
		float score = clearDist;
		if (score > bestClearance) {
			bestClearance = score;
			bestMoveX = rx;
			bestMoveZ = rz;
		}
	}

	// Execute move command
	if (bestClearance > 25.0f) {
		pControls->bForward = TRUE;
		pControls->bBackward = FALSE;
	} else {
		// Very close to wall, step back or turn
		pControls->bForward = FALSE;
		pControls->bBackward = TRUE;
		angy += 45.0f;
	}
}
