#include "d3dtypes.h"
#include "world.hpp"
#include "GlobalSettings.hpp"
#include "MonsterAI.hpp"
#include <math.h>
#include <string.h>
#include <vector>

extern int endc;
extern int src_collide[MAX_NUM_QUADS];

namespace {

constexpr int kSlots = 16;
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717958f;
constexpr float kSlotStep = kTwoPi / kSlots;

constexpr float kThinkInterval = 0.10f; // seconds between steering decisions
constexpr int kThinkBudgetPerFrame = 10;
constexpr float kMaxDt = 0.10f;

constexpr float kBodyRadius = 35.0f;      // whisker hits closer than this are treated as blocked
constexpr float kWhiskerBase = 170.0f;    // whisker length on the sides and behind
constexpr float kWhiskerExtra = 110.0f;   // extra length for whiskers pointing where we want to go
constexpr float kGatherRadius = 320.0f;   // level geometry cached around the monster
constexpr float kLowWhiskerY = 15.0f;     // whisker heights relative to the monster centre (kept high so inclines are not hit)
constexpr float kHighWhiskerY = 45.0f;
constexpr float kLedgeProbeDist = 55.0f;  // how far ahead the floor is checked
constexpr float kLedgeProbeRise = 60.0f; // the floor probe starts this far above the monster centre so rising slopes are found
constexpr float kLedgeProbeDrop = 230.0f; // total probe length, measured from the raised start point
constexpr float kLedgeDanger = 0.55f;

constexpr float kWalkableNormalY = 0.35f;  // triangles flatter than this (about 60 degrees) are floors/ramps, not walls
constexpr float kSeparationRadius = 130.0f;
constexpr float kTurnRate = 9.5f;         // radians per second (about 540 degrees)
constexpr float kMeleeRange = 80.0f;
constexpr float kKiteMinRange = 220.0f;
constexpr float kKiteMaxRange = 380.0f;
constexpr size_t kMaxCachedTris = 1536;

struct V3 {
	float x, y, z;
};

inline V3 Sub(const V3 &a, const V3 &b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline float Dot(const V3 &a, const V3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 Cross(const V3 &a, const V3 &b) {
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Sat(float v) { return Clamp(v, 0.0f, 1.0f); }

struct CachedTri {
	V3 v0, e1, e2;
	float ny; // unit normal Y component
};

struct State {
	int monsterId = -1;
	bool init = false;
	unsigned int rng = 0;

	float lastActiveSimTime = 0.0f;
	uint64_t lastActiveFrame = 0;

	float thinkTimer = 0.0f;
	float headX = 1.0f, headZ = 0.0f;   // smoothed move direction
	float steerX = 1.0f, steerZ = 0.0f; // last decision
	float speedScale = 1.0f;
	float faceRad = 0.0f;

	bool los = true;
	int losBlockedCount = 0;

	bool kiter = false;
	bool coward = false;
	bool cowardChecked = false;
	bool aggressive = false;
	bool aggressiveChecked = false;
	int strafeSide = 1;
	float flank = 0.0f;
	float phase = 0.0f;
	float clock = 0.0f;
	int maxHp = 1;

	float prevX = 0.0f, prevZ = 0.0f;
	float walkExpected = 0.0f, walkMoved = 0.0f;
	int stuckCount = 0;

	float escapeTimer = 0.0f;
	int escapeSide = 1;
	float followTimer = 0.0f;
	int followSide = 1;
	float fleeTimer = 0.0f, fleeCooldown = 0.0f;
	float retreatTime = 0.0f, retreatCooldown = 0.0f;
	bool retreating = false;
	float strafeFlipTimer = 0.0f;
};

State g_state[MAX_NUM_MONSTERS];
std::vector<CachedTri> g_tris;
float g_slotX[kSlots], g_slotZ[kSlots];
bool g_slotsReady = false;
int g_thinkBudget = kThinkBudgetPerFrame;

float g_simTime = 0.0f;
uint64_t g_simFrame = 0;
unsigned int g_aiSeed = 0x9E3779B9u;

// Deterministic integer hash to seed each monster's PRNG independently from its ID and session seed.
inline unsigned int Hash32(unsigned int x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return x ? x : 0x9E3779B9u;
}

// Per-monster Xorshift32 PRNG: avoids global RNG coupling so each monster's decisions remain isolated and deterministic.
inline unsigned int NextRand(State &s) {
	s.rng ^= s.rng << 13;
	s.rng ^= s.rng >> 17;
	s.rng ^= s.rng << 5;
	return s.rng;
}

inline float Rand01(State &s) {
	return (NextRand(s) & 0xFFFFFF) / 16777216.0f;
}

inline int RandSign(State &s) {
	return (NextRand(s) & 1) ? 1 : -1;
}

void EnsureSlots() {
	if (g_slotsReady)
		return;
	for (int i = 0; i < kSlots; i++) {
		g_slotX[i] = cosf(i * kSlotStep);
		g_slotZ[i] = sinf(i * kSlotStep);
	}
	g_tris.reserve(kMaxCachedTris);
	g_slotsReady = true;
}

int SlotFromDir(float x, float z) {
	float a = atan2f(z, x);
	int i = (int)floorf(a / kSlotStep + 0.5f);
	return ((i % kSlots) + kSlots) % kSlots;
}

float WrapAngle(float a) {
	while (a > kPi)
		a -= kTwoPi;
	while (a < -kPi)
		a += kTwoPi;
	return a;
}

// Moves 'cur' toward 'target' by at most maxStep radians.
float TurnToward(float cur, float target, float maxStep) {
	float d = WrapAngle(target - cur);
	d = Clamp(d, -maxStep, maxStep);
	return cur + d;
}

void Rotate(float x, float z, float a, float &ox, float &oz) {
	float c = cosf(a), s = sinf(a);
	ox = x * c - z * s;
	oz = x * s + z * c;
}

// Moller-Trumbore ray/triangle test, double sided.
bool RayTriangle(const V3 &o, const V3 &d, const CachedTri &t, float &tOut) {
	V3 p = Cross(d, t.e2);
	float det = Dot(t.e1, p);
	if (fabsf(det) < 1e-7f)
		return false;
	float inv = 1.0f / det;
	V3 s = Sub(o, t.v0);
	float u = Dot(s, p) * inv;
	if (u < -1e-4f || u > 1.0001f)
		return false;
	V3 q = Cross(s, t.e1);
	float v = Dot(d, q) * inv;
	if (v < -1e-4f || u + v > 1.0001f)
		return false;
	float tt = Dot(t.e2, q) * inv;
	if (tt < 0.001f)
		return false;
	tOut = tt;
	return true;
}

enum RayMode { RAY_WALLS,
	           RAY_FLOORS };

// Returns the distance to the nearest wall (or floor) hit, or maxT when nothing is hit.
float CastRay(const V3 &o, const V3 &d, float maxT, RayMode mode) {
	float best = maxT;
	for (const CachedTri &tri : g_tris) {
		float ay = fabsf(tri.ny);
		if (mode == RAY_WALLS && ay >= kWalkableNormalY)
			continue;
		if (mode == RAY_FLOORS && ay < kWalkableNormalY)
			continue;
		float t;
		if (RayTriangle(o, d, tri, t) && t < best)
			best = t;
	}
	return best;
}

struct LosRay {
	V3 o, d;
	float len;
	bool blocked;
	bool active;
};

// One sweep over the level triangles: caches the ones near the monster for the whisker casts and tests the
// line of sight rays against every triangle they could cross.
void GatherGeometry(const V3 &c, LosRay *los, int losCount) {
	g_tris.clear();

	const float lx0 = c.x - kGatherRadius, lx1 = c.x + kGatherRadius;
	const float lz0 = c.z - kGatherRadius, lz1 = c.z + kGatherRadius;
	const float ly0 = c.y - 260.0f, ly1 = c.y + 90.0f;

	float sx0[2], sx1[2], sy0[2], sy1[2], sz0[2], sz1[2];
	for (int r = 0; r < losCount; r++) {
		V3 e = { los[r].o.x + los[r].d.x * los[r].len, los[r].o.y + los[r].d.y * los[r].len, los[r].o.z + los[r].d.z * los[r].len };
		sx0[r] = fminf(los[r].o.x, e.x) - 2.0f;
		sx1[r] = fmaxf(los[r].o.x, e.x) + 2.0f;
		sy0[r] = fminf(los[r].o.y, e.y) - 2.0f;
		sy1[r] = fmaxf(los[r].o.y, e.y) + 2.0f;
		sz0[r] = fminf(los[r].o.z, e.z) - 2.0f;
		sz1[r] = fmaxf(los[r].o.z, e.z) + 2.0f;
	}

	for (int i = 0; i + 2 < endc; i += 3) {
		if (src_collide[i] != 1)
			continue;

		const D3DVERTEX2 &a = src_v[i];
		const D3DVERTEX2 &b = src_v[i + 1];
		const D3DVERTEX2 &d = src_v[i + 2];

		float minx = fminf(a.x, fminf(b.x, d.x)), maxx = fmaxf(a.x, fmaxf(b.x, d.x));
		float miny = fminf(a.y, fminf(b.y, d.y)), maxy = fmaxf(a.y, fmaxf(b.y, d.y));
		float minz = fminf(a.z, fminf(b.z, d.z)), maxz = fmaxf(a.z, fmaxf(b.z, d.z));

		bool local = !(maxx < lx0 || minx > lx1 || maxz < lz0 || minz > lz1 || maxy < ly0 || miny > ly1) &&
		             g_tris.size() < kMaxCachedTris;

		bool losCandidate = false;
		for (int r = 0; r < losCount; r++) {
			if (los[r].active && !los[r].blocked &&
			    !(maxx < sx0[r] || minx > sx1[r] || maxy < sy0[r] || miny > sy1[r] || maxz < sz0[r] || minz > sz1[r])) {
				losCandidate = true;
				break;
			}
		}

		if (!local && !losCandidate)
			continue;

		CachedTri tri;
		tri.v0 = { a.x, a.y, a.z };
		tri.e1 = { b.x - a.x, b.y - a.y, b.z - a.z };
		tri.e2 = { d.x - a.x, d.y - a.y, d.z - a.z };
		V3 n = Cross(tri.e1, tri.e2);
		float nl = sqrtf(Dot(n, n));
		if (nl < 1e-4f)
			continue;
		tri.ny = n.y / nl;

		if (local)
			g_tris.push_back(tri);

		if (losCandidate) {
			for (int r = 0; r < losCount; r++) {
				float t;
				if (los[r].active && !los[r].blocked && RayTriangle(los[r].o, los[r].d, tri, t) && t < los[r].len)
					los[r].blocked = true;
			}
		}
	}
}

bool IsKiter(const char *name) {
	static const char *casters[] = { "WRAITH", "BAUUL", "FAERIE", "FULIMO", "MUMMY", "HYDRA", "IMP",
		                             "SORCERER", "NECROMANCER", "OKTA", "SORB", "DEMON" };
	for (const char *c : casters) {
		if (strstr(name, c) != NULL)
			return true;
	}
	return false;
}

void InitState(State &s, int idx) {
	const PLAYER &m = monster_list[idx];
	const PLAYER &p = player_list[trueplayernum];

	s = State();
	s.monsterId = m.monsterid;
	s.init = true;
	s.rng = Hash32((unsigned int)m.monsterid ^ g_aiSeed);
	s.lastActiveSimTime = g_simTime;
	s.lastActiveFrame = g_simFrame;

	float dx = p.x - m.x, dz = p.z - m.z;
	float len = sqrtf(dx * dx + dz * dz);
	if (len > 1e-3f) {
		dx /= len;
		dz /= len;
	} else {
		dx = 1.0f;
		dz = 0.0f;
	}
	s.headX = s.steerX = dx;
	s.headZ = s.steerZ = dz;
	s.faceRad = atan2f(dz, dx);
	s.thinkTimer = Rand01(s) * kThinkInterval; // spread the thinks of a pack across frames deterministically

	s.kiter = IsKiter(m.rname);
	s.coward = false;
	s.aggressive = false;
	s.strafeSide = RandSign(s);
	s.flank = RandSign(s) * (0.5f + 0.5f * Rand01(s));
	s.phase = Rand01(s) * kTwoPi;
	s.maxHp = m.hp > m.health ? m.hp : m.health;
	if (s.maxHp < 1)
		s.maxHp = 1;
	s.prevX = m.x;
	s.prevZ = m.z;
	s.escapeSide = RandSign(s);
	s.followSide = RandSign(s);
}

bool NeighbourAlive(const PLAYER &n) {
	return n.bIsPlayerAlive == TRUE && n.bIsPlayerValid == TRUE && n.health > 0;
}

void Think(State &s, int idx, float dist, float tx, float tz) {
	const PLAYER &m = monster_list[idx];
	const PLAYER &p = player_list[trueplayernum];

	const V3 origin = { m.x, m.y, m.z };

	// Line of sight: two rays (centre and a bit higher) so a single stair lip or table edge does not blind the monster.
	LosRay los[2];
	for (int r = 0; r < 2; r++) {
		float yOff = r == 0 ? 0.0f : 20.0f;
		V3 from = { m.x + tx * 10.0f, m.y + yOff, m.z + tz * 10.0f };
		V3 to = { p.x - tx * 25.0f, p.y + yOff, p.z - tz * 25.0f };
		V3 dv = Sub(to, from);
		float len = sqrtf(Dot(dv, dv));
		los[r].o = from;
		los[r].len = len;
		los[r].blocked = false;
		los[r].active = dist > 60.0f && len > 1.0f;
		los[r].d = len > 1e-3f ? V3{ dv.x / len, dv.y / len, dv.z / len } : V3{ tx, 0.0f, tz };
	}

	GatherGeometry(origin, los, 2);

	bool blocked = los[0].active && los[0].blocked && los[1].blocked;
	s.losBlockedCount = blocked ? s.losBlockedCount + 1 : 0;
	s.los = s.losBlockedCount < 2;

	// Ledges are only checked while we are actually standing on something.
	bool hasGround = CastRay(V3{ origin.x, origin.y + kLedgeProbeRise, origin.z }, V3{ 0.0f, -1.0f, 0.0f }, kLedgeProbeDrop + 60.0f, RAY_FLOORS) < kLedgeProbeDrop + 60.0f;

	// Stuck detection: we wanted to walk, but barely moved.
	if (s.walkExpected > 90.0f) {
		if (s.walkMoved < 0.3f * s.walkExpected) {
			s.stuckCount++;
			s.escapeTimer = 0.7f + 0.35f * (float)(s.stuckCount < 4 ? s.stuckCount : 4);
			float leftLen = CastRay(origin, V3{ -tz, 0.0f, tx }, 200.0f, RAY_WALLS);
			float rightLen = CastRay(origin, V3{ tz, 0.0f, -tx }, 200.0f, RAY_WALLS);
			if (fabsf(leftLen - rightLen) < 15.0f)
				s.escapeSide = -s.escapeSide;
			else
				s.escapeSide = leftLen > rightLen ? 1 : -1;
		} else if (s.stuckCount > 0) {
			s.stuckCount--;
		}
		s.walkExpected = 0.0f;
		s.walkMoved = 0.0f;
	}

	// Morale and aggressiveness logic based on HD and hitpoints fraction:
	float hpFrac = (float)m.health / (float)s.maxHp;

	if (m.hd < p.hd && hpFrac < 0.10f) {
		if (!s.cowardChecked) {
			s.cowardChecked = true;
			if (Rand01(s) < 0.5f) {
				s.coward = true;
			}
		}
	} else {
		s.coward = false;
		s.cowardChecked = false;
	}

	if (m.hd > p.hd && hpFrac >= 0.80f) {
		if (!s.aggressiveChecked) {
			s.aggressiveChecked = true;
			if (Rand01(s) < 0.5f) {
				s.aggressive = true;
			}
		}
	} else {
		s.aggressive = false;
		s.aggressiveChecked = false;
	}

	if (s.coward && s.fleeTimer <= 0.0f && s.fleeCooldown <= 0.0f) {
		s.fleeTimer = 2.5f + 1.5f * Rand01(s);
		s.fleeCooldown = 9.0f;
	}
	bool fleeing = s.fleeTimer > 0.0f;

	// Behaviour mix.
	float wSeek = 1.0f, wStrafe = 0.0f, wFlee = 0.0f;
	bool retreating = false;
	if (fleeing) {
		wSeek = 0.0f;
		wFlee = 1.0f;
		wStrafe = 0.25f;
	} else if (s.aggressive) {
		wSeek = 1.8f;
		wStrafe = 0.0f;
	} else if (s.kiter && s.los && s.retreatCooldown <= 0.0f && dist < kKiteMaxRange) {
		if (dist < kKiteMinRange) {
			wSeek = 0.0f;
			wFlee = 1.0f;
			wStrafe = 0.6f;
			retreating = true;
		} else {
			wSeek = 0.0f;
			wStrafe = 1.0f;
		}
	} else if (!s.kiter && dist > kMeleeRange && dist < 260.0f && m.attackspeed > 0) {
		// Waiting for the attack cooldown: circle the player instead of standing in a line.
		wSeek = 0.6f;
		wStrafe = 0.7f;
	}
	s.retreating = retreating;

	// Approach direction: flank offset (fades out near the target) and a lazy wander when far away.
	float flankAmt = s.aggressive ? 0.0f : s.flank * Sat((dist - 150.0f) / 450.0f);
	float wander = s.aggressive ? 0.0f : 0.28f * sinf(s.clock * 1.3f + s.phase) * Sat((dist - 350.0f) / 450.0f);
	float sx, sz;
	Rotate(tx, tz, flankAmt + wander, sx, sz);

	float tangX = -tz * s.strafeSide;
	float tangZ = tx * s.strafeSide;

	float dx, dz;
	if (s.escapeTimer > 0.0f && !fleeing) {
		dx = -tz * s.escapeSide + 0.25f * tx;
		dz = tx * s.escapeSide + 0.25f * tz;
	} else {
		dx = wSeek * sx + wStrafe * tangX - wFlee * tx;
		dz = wSeek * sz + wStrafe * tangZ - wFlee * tz;
	}
	float dl = sqrtf(dx * dx + dz * dz);
	if (dl < 1e-4f) {
		dx = tx;
		dz = tz;
	} else {
		dx /= dl;
		dz /= dl;
	}

	// Danger map: one whisker per slot at two heights, longer toward where we want to go.
	float danger[kSlots];
	for (int i = 0; i < kSlots; i++) {
		float align = g_slotX[i] * dx + g_slotZ[i] * dz;
		float len = kWhiskerBase + kWhiskerExtra * (align > 0.0f ? align : 0.0f);

		V3 dir = { g_slotX[i], 0.0f, g_slotZ[i] };
		V3 lowO = { m.x, m.y + kLowWhiskerY, m.z };
		V3 highO = { m.x, m.y + kHighWhiskerY, m.z };
		float d = fminf(CastRay(lowO, dir, len, RAY_WALLS), CastRay(highO, dir, len, RAY_WALLS));

		float dg = 0.0f;
		if (d < len)
			dg = Sat(1.0f - (d - kBodyRadius) / (len - kBodyRadius));

		if (hasGround && align > -0.35f) {
			V3 probe = { m.x + dir.x * kLedgeProbeDist, m.y + kLedgeProbeRise, m.z + dir.z * kLedgeProbeDist };
			if (CastRay(probe, V3{ 0.0f, -1.0f, 0.0f }, kLedgeProbeDrop, RAY_FLOORS) >= kLedgeProbeDrop)
				dg = fmaxf(dg, kLedgeDanger);
		}
		danger[i] = dg;
	}

	// Neighbour separation: other monsters are soft obstacles so packs spread out instead of stacking.
	if (!fleeing) {
		for (int j = 0; j < num_monsters; j++) {
			if (j == idx || !NeighbourAlive(monster_list[j]))
				continue;
			float nx = monster_list[j].x - m.x;
			float nz = monster_list[j].z - m.z;
			float nd2 = nx * nx + nz * nz;
			if (nd2 > kSeparationRadius * kSeparationRadius || nd2 < 1e-4f || fabsf(monster_list[j].y - m.y) > 100.0f)
				continue;
			float nd = sqrtf(nd2);
			nx /= nd;
			nz /= nd;
			float prox = 1.0f - nd / kSeparationRadius;
			for (int i = 0; i < kSlots; i++) {
				float dd = g_slotX[i] * nx + g_slotZ[i] * nz;
				if (dd > 0.2f)
					danger[i] = fmaxf(danger[i], 0.7f * prox * dd);
			}
		}
	}

	// A whisker also warns about the slots next to it, so the body clears corners.
	float dil[kSlots];
	for (int i = 0; i < kSlots; i++) {
		float v = danger[i];
		v = fmaxf(v, 0.6f * danger[(i + 1) % kSlots]);
		v = fmaxf(v, 0.6f * danger[(i + kSlots - 1) % kSlots]);
		v = fmaxf(v, 0.25f * danger[(i + 2) % kSlots]);
		v = fmaxf(v, 0.25f * danger[(i + kSlots - 2) % kSlots]);
		dil[i] = v;
	}

	// Wall following: when the way to the goal is blocked, commit to one side for a while to avoid dithering.
	float fX = -dz, fZ = dx; // left tangent of the desired direction
	if (dil[SlotFromDir(dx, dz)] > 0.55f && s.escapeTimer <= 0.0f) {
		if (s.followTimer <= 0.0f) {
			float leftD = dil[SlotFromDir(fX, fZ)];
			float rightD = dil[SlotFromDir(-fX, -fZ)];
			if (fabsf(leftD - rightD) > 0.05f)
				s.followSide = leftD < rightD ? 1 : -1;
		}
		s.followTimer = 0.9f;
	}
	if (s.followTimer > 0.0f) {
		float sideD = dil[SlotFromDir(fX * s.followSide, fZ * s.followSide)];
		float otherD = dil[SlotFromDir(-fX * s.followSide, -fZ * s.followSide)];
		if (sideD > 0.7f && otherD < sideD - 0.2f)
			s.followSide = -s.followSide;
	}

	// Circling monsters turn around when the side they are circling toward is blocked.
	if (wStrafe > 0.0f && s.strafeFlipTimer <= 0.0f && dil[SlotFromDir(tangX, tangZ)] > 0.6f) {
		s.strafeSide = -s.strafeSide;
		s.strafeFlipTimer = 1.0f;
	}

	// Interest map, masked by danger.
	float minD = 1.0f;
	for (int i = 0; i < kSlots; i++)
		minD = fminf(minD, dil[i]);
	float threshold = fmaxf(minD + 0.05f, 0.5f);

	float score[kSlots];
	int best = -1;
	float bestScore = 0.0f;
	for (int i = 0; i < kSlots; i++) {
		float interest = fmaxf(0.0f, g_slotX[i] * dx + g_slotZ[i] * dz);
		interest += 0.25f * fmaxf(0.0f, g_slotX[i] * s.steerX + g_slotZ[i] * s.steerZ); // momentum
		interest += 0.04f;
		if (s.followTimer > 0.0f)
			interest += 0.35f * fmaxf(0.0f, (g_slotX[i] * fX + g_slotZ[i] * fZ) * s.followSide);

		score[i] = dil[i] > threshold ? 0.0f : interest * (1.0f - dil[i]);
		if (score[i] > bestScore) {
			bestScore = score[i];
			best = i;
		}
	}

	if (best < 0) {
		// Boxed in: no direction is safe. Keep pushing toward the goal, slowly.
		s.steerX = dx;
		s.steerZ = dz;
		s.speedScale = 0.25f;
		return;
	}

	// Blend the best slot with its neighbours for a heading finer than the slot spacing.
	int prev = (best + kSlots - 1) % kSlots, next = (best + 1) % kSlots;
	float vx = g_slotX[best] * score[best] + g_slotX[prev] * score[prev] + g_slotX[next] * score[next];
	float vz = g_slotZ[best] * score[best] + g_slotZ[prev] * score[prev] + g_slotZ[next] * score[next];
	float vl = sqrtf(vx * vx + vz * vz);
	if (vl > 1e-4f) {
		s.steerX = vx / vl;
		s.steerZ = vz / vl;
	} else {
		s.steerX = g_slotX[best];
		s.steerZ = g_slotZ[best];
	}

	s.speedScale = Clamp(1.0f - 0.6f * dil[best], 0.4f, 1.0f);
	if (s.aggressive)
		s.speedScale *= 1.25f;
}

} // namespace

void MonsterAIBeginFrame(float fElapsedTime) {
	g_simTime += Clamp(fElapsedTime, 0.0f, kMaxDt);
	g_simFrame++;
	g_thinkBudget = kThinkBudgetPerFrame;
}

void MonsterAIReset(unsigned int seed) {
	g_aiSeed = seed ? seed : 0x9E3779B9u;
	g_simTime = 0.0f;
	g_simFrame = 0;
	g_thinkBudget = kThinkBudgetPerFrame;
	for (int i = 0; i < MAX_NUM_MONSTERS; i++)
		g_state[i] = State();
}

MonsterSteering MonsterAIUpdate(int idx, float fElapsedTime, bool walking) {
	EnsureSlots();

	const PLAYER &m = monster_list[idx];
	const PLAYER &p = player_list[trueplayernum];
	State &s = g_state[idx];

	if (!s.init || s.monsterId != m.monsterid)
		InitState(s, idx);

	// A monster that was off screen for a while starts with a clean slate.
	// Uses game simulation time to guarantee deterministic replay across different frame rates.
	if (s.lastActiveSimTime > 0.0f && (g_simTime - s.lastActiveSimTime > 0.5f)) {
		s.escapeTimer = s.followTimer = 0.0f;
		s.walkExpected = s.walkMoved = 0.0f;
		s.thinkTimer = 0.0f;
		s.prevX = m.x;
		s.prevZ = m.z;
	}
	s.lastActiveSimTime = g_simTime;
	s.lastActiveFrame = g_simFrame;

	float dt = Clamp(fElapsedTime, 0.0f, kMaxDt);
	s.clock += dt;

	float tx = p.x - m.x, tz = p.z - m.z;
	float dist = sqrtf(tx * tx + tz * tz);
	if (dist > 1e-3f) {
		tx /= dist;
		tz /= dist;
	} else {
		tx = s.headX;
		tz = s.headZ;
	}

	s.escapeTimer = fmaxf(0.0f, s.escapeTimer - dt);
	s.followTimer = fmaxf(0.0f, s.followTimer - dt);
	s.fleeTimer = fmaxf(0.0f, s.fleeTimer - dt);
	s.fleeCooldown = fmaxf(0.0f, s.fleeCooldown - dt);
	s.retreatCooldown = fmaxf(0.0f, s.retreatCooldown - dt);
	s.strafeFlipTimer = fmaxf(0.0f, s.strafeFlipTimer - dt);
	if (s.retreating) {
		s.retreatTime += dt;
		if (s.retreatTime > 2.5f) {
			s.retreatTime = 0.0f;
			s.retreatCooldown = 4.0f; // stand and fight for a bit
		}
	} else {
		s.retreatTime = fmaxf(0.0f, s.retreatTime - dt);
	}

	// Track how far we really travelled compared to how far we tried to.
	float moved = sqrtf((m.x - s.prevX) * (m.x - s.prevX) + (m.z - s.prevZ) * (m.z - s.prevZ));
	s.prevX = m.x;
	s.prevZ = m.z;
	if (walking && dist > kMeleeRange + 20.0f) {
		s.walkExpected += (165.0f + m.speed * 2.0f) * s.speedScale * dt;
		s.walkMoved += moved;
	}

	s.thinkTimer -= dt;
	if (s.thinkTimer <= 0.0f && g_thinkBudget > 0) {
		g_thinkBudget--;
		Think(s, idx, dist, tx, tz);
		s.thinkTimer = kThinkInterval + Rand01(s) * 0.04f;
	}

	// Smooth the heading so direction changes between thinks are never a snap.
	float curA = atan2f(s.headZ, s.headX);
	float tgtA = atan2f(s.steerZ, s.steerX);
	float newA = TurnToward(curA, tgtA, kTurnRate * dt);
	s.headX = cosf(newA);
	s.headZ = sinf(newA);

	float align = s.headX * s.steerX + s.headZ * s.steerZ;
	float speedMul = 0.45f + 0.55f * fmaxf(0.0f, align);

	// Facing: look at the player when close, shooting or otherwise engaged; otherwise look where we are going.
	float playerA = atan2f(tz, tx);
	bool snapToPlayer = !walking || dist < 120.0f;
	bool facePlayer = snapToPlayer || (s.los && (dist < 300.0f || s.kiter));
	if (snapToPlayer)
		s.faceRad = playerA;
	else
		s.faceRad = TurnToward(s.faceRad, facePlayer ? playerA : newA, kTurnRate * dt);

	float deg = s.faceRad / k;
	while (deg < 0.0f)
		deg += 360.0f;
	while (deg >= 360.0f)
		deg -= 360.0f;

	MonsterSteering out;
	out.dirX = s.headX;
	out.dirZ = s.headZ;
	out.speedScale = s.speedScale * speedMul;
	out.faceAngle = deg;
	out.hasLineOfSight = s.los || dist < 60.0f;
	out.kiter = s.kiter;
	return out;
}
