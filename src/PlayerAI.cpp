#include "d3dtypes.h"
#include "world.hpp"
#include "GlobalSettings.hpp"
#include "Missle.hpp"
#include "GameLogic.hpp"
#include "DirectInput.hpp"
#include "PlayerAI.hpp"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <vector>

extern int endc;
extern int src_collide[MAX_NUM_QUADS];
extern int usespell;
extern char gActionMessage[2048];
int UpdateScrollList(int r, int g, int b);

namespace {

constexpr int kSlots = 16;
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717958f;
constexpr float kSlotStep = kTwoPi / kSlots;

constexpr float kThinkInterval = 0.10f; // seconds between decisions
constexpr float kMaxDt = 0.10f;

// Awareness. WakeUpMonsters only treats things inside a box around the player as present, 800 units is always inside it.
constexpr float kSenseRadius = 800.0f;
constexpr float kDoorSenseRadius = 1100.0f;
constexpr float kMaxMonsterDy = 220.0f;
constexpr float kMaxItemDy = 160.0f;
constexpr float kMaxDoorDy = 150.0f;

// Steering. The player's collision ellipsoid is centred on the eye with a radius of 25 (XZ) and 50 (Y).
constexpr float kBodyRadius = 28.0f;
constexpr float kWhiskerBase = 170.0f;
constexpr float kWhiskerExtra = 110.0f;
constexpr float kGatherRadius = 450.0f;
constexpr float kLowWhiskerY = -20.0f; // whisker heights relative to the eye
constexpr float kHighWhiskerY = 20.0f;
constexpr float kLedgeProbeDist = 55.0f;
constexpr float kLedgeProbeRise = 30.0f;
constexpr float kLedgeProbeDrop = 230.0f;
constexpr float kLedgeDanger = 0.55f;
constexpr float kWalkableNormalY = 0.55f;
constexpr float kMaxStepRiserHeight = 25.0f; // stair risers are 20 units; walls are >= 120 units
constexpr size_t kMaxCachedTris = 2048;
constexpr float kExploreLook = 420.0f;
constexpr float kMissileRadius = 350.0f;
constexpr float kSpiralAvoidRadius = 170.0f;

// Combat.
constexpr float kEngageRange = 260.0f; // face the monster instead of the walking direction
constexpr float kSwingRange = 105.0f;  // start swinging; the hit lands later and needs the monster within 100
constexpr float kHoldRange = 70.0f;    // close enough, stop walking into the monster
constexpr float kBackoffRange = 45.0f;
constexpr float kScrollMinRange = 240.0f;
constexpr float kScrollKeepRange = 150.0f;
constexpr float kScrollMaxRange = 850.0f;
constexpr int kScrollMinMonsterHp = 8;
constexpr float kLowHealth = 0.40f;
constexpr float kWeakHealth = 0.35f;

// Camera.
constexpr float kMaxTurnRate = 600.0f; // degrees per second
constexpr float kTurnResponse = 10.0f;
constexpr float kPitchRate = 120.0f;
constexpr float kPlayerWalkSpeed = 260.0f; // units per second, for stuck detection

constexpr float kExitIdleTime = 15.0f; // explore this long without any goal before taking the level exit
constexpr float kRespawnDelay = 2.5f;
constexpr float kVisitedCell = 150.0f;
constexpr int kMaxBans = 48;

struct V3 {
	float x, y, z;
};

inline V3 Sub(const V3 &a, const V3 &b) {
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}
inline float Dot(const V3 &a, const V3 &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline V3 Cross(const V3 &a, const V3 &b) {
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
inline float Clamp(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
inline float Sat(float v) {
	return Clamp(v, 0.0f, 1.0f);
}

struct CachedTri {
	V3 v0, e1, e2;
	float ny;
	float height;
};

enum TargetKind {
	T_NONE,
	T_MONSTER,
	T_ITEM,
	T_DOOR,
	T_EXIT,
	T_EXPLORE
};

enum ItemKind {
	IK_NONE,
	IK_TREASURE,
	IK_GEAR,
	IK_HEAL,
	IK_EXIT
};

struct Ban {
	int kind = T_NONE;
	int index = -1;
	float until = 0.0f;
};

struct State {
	bool enabled = false;
	unsigned int rng = 0x2545F491u;
	float clock = 0.0f;
	float thinkTimer = 0.0f;
	float deadTimer = 0.0f;

	// Goal.
	int kind = T_NONE;
	int index = -1;
	float tx = 0.0f, ty = 0.0f, tz = 0.0f;
	float tdist = 0.0f; // horizontal distance to the goal
	bool los = true;
	int losBlockedCount = 0;
	float bestDist = 1e9f;
	float noProgress = 0.0f;
	float arriveTimer = 0.0f;
	float idleTime = 0.0f;

	// Steering result.
	float steerX = 0.0f, steerZ = 1.0f;
	float speedScale = 1.0f;
	bool hold = false;
	bool boxedIn = false;

	// Exploration.
	float exploreX = 0.0f, exploreZ = 1.0f;
	float exploreTimer = 0.0f;

	// Stuck handling.
	float prevX = 0.0f, prevZ = 0.0f;
	bool prevValid = false;
	bool cmdMove = false;
	float walkExpected = 0.0f, walkMoved = 0.0f;
	int stuckCount = 0;
	float escapeTimer = 0.0f;
	int escapeSide = 1;
	float followTimer = 0.0f;
	int followSide = 1;
	bool jumpPulse = false;

	// Doors.
	int nearDoor = -1;
	float holdTimer = 0.0f;
	float doorPulse[MAX_OBJECTDOORS];
	int doorSkip[MAX_OBJECTDOORS]; // key count when the door turned out to be locked, -1 when untried

	// Weapons.
	float switchCooldown = 0.0f;
	float switchBan = 0.0f;
	int switchAttempts = 0;

	Ban bans[kMaxBans];
	int nextBan = 0;
};

State g;
std::vector<CachedTri> g_tris;
float g_slotX[kSlots], g_slotZ[kSlots];
bool g_slotsReady = false;
std::unordered_map<long long, unsigned char> g_visited;

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

float Wrap180(float a) {
	while (a > 180.0f)
		a -= 360.0f;
	while (a < -180.0f)
		a += 360.0f;
	return a;
}

float Wrap360(float a) {
	while (a >= 360.0f)
		a -= 360.0f;
	while (a < 0.0f)
		a += 360.0f;
	return a;
}

// The game's yaw: forward is (sin(angy), cos(angy)) on the XZ plane.
float YawOf(float dx, float dz) {
	return Wrap360(atan2f(dx, dz) / k);
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

enum RayMode {
	RAY_WALLS,
	RAY_FLOORS
};

// Returns the distance to the nearest wall (or floor) hit, or maxT when nothing is hit.
float CastRay(const V3 &o, const V3 &d, float maxT, RayMode mode) {
	float best = maxT;
	for (const CachedTri &tri : g_tris) {
		float ay = fabsf(tri.ny);
		if (mode == RAY_WALLS && (ay >= kWalkableNormalY || tri.height <= kMaxStepRiserHeight))
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

// One sweep over the level triangles: caches the ones around the player for the whisker casts and tests the line of
// sight rays against every wall they could cross.
void GatherGeometry(const V3 &c, LosRay *los, int losCount) {
	g_tris.clear();

	const float lx0 = c.x - kGatherRadius, lx1 = c.x + kGatherRadius;
	const float lz0 = c.z - kGatherRadius, lz1 = c.z + kGatherRadius;
	const float ly0 = c.y - 260.0f, ly1 = c.y + 160.0f;

	float sx0[2] = {}, sx1[2] = {}, sy0[2] = {}, sy1[2] = {}, sz0[2] = {}, sz1[2] = {};
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
		tri.height = maxy - miny;

		if (local)
			g_tris.push_back(tri);

		if (losCandidate && (tri.height > kMaxStepRiserHeight && fabsf(tri.ny) < kWalkableNormalY)) {
			for (int r = 0; r < losCount; r++) {
				float t;
				if (los[r].active && !los[r].blocked && RayTriangle(los[r].o, los[r].d, tri, t) && t < los[r].len)
					los[r].blocked = true;
			}
		}
	}
}

void Say(const char *msg, int r, int gr, int b) {
	strcpy_s(gActionMessage, msg);
	UpdateScrollList(r, gr, b);
}

//-----------------------------------------------------------------------------
// Bans: goals that could not be reached are ignored for a while.
//-----------------------------------------------------------------------------
bool Banned(const State &s, int kind, int index) {
	for (const Ban &b : s.bans) {
		if (b.until > s.clock && b.kind == kind && b.index == index)
			return true;
	}
	return false;
}

void AddBan(State &s, int kind, int index, float seconds) {
	for (Ban &b : s.bans) {
		if (b.kind == kind && b.index == index) {
			b.until = s.clock + seconds;
			return;
		}
	}
	Ban &b = s.bans[s.nextBan];
	s.nextBan = (s.nextBan + 1) % kMaxBans;
	b.kind = kind;
	b.index = index;
	b.until = s.clock + seconds;
}

void ClearGoal(State &s) {
	s.kind = T_NONE;
	s.index = -1;
	s.bestDist = 1e9f;
	s.noProgress = 0.0f;
	s.arriveTimer = 0.0f;
	s.los = true;
	s.losBlockedCount = 0;
}

//-----------------------------------------------------------------------------
// Game queries
//-----------------------------------------------------------------------------
bool Huntable(const PLAYER &m) {
	return m.bIsPlayerAlive == TRUE && m.bIsPlayerValid == TRUE && m.health > 0 && m.current_sequence != 5 &&
	       m.current_frame != 183 && m.current_frame != 189 && m.current_frame != 197 &&
	       strcmp(m.rname, "SLAVE") != 0 && strcmp(m.rname, "CENTAUR") != 0;
}

int ItemCategory(const char *n) {
	if (strcmp(n, "COIN") == 0 || strcmp(n, "GOBLET") == 0 || strcmp(n, "diamond") == 0 || strcmp(n, "KEY2") == 0)
		return IK_TREASURE;
	if (strcmp(n, "AXE") == 0 || strcmp(n, "FLAMESWORD") == 0 || strcmp(n, "BASTARDSWORD") == 0 ||
	    strcmp(n, "BATTLEAXE") == 0 || strcmp(n, "ICESWORD") == 0 || strcmp(n, "MORNINGSTAR") == 0 ||
	    strcmp(n, "SPIKEDFLAIL") == 0 || strcmp(n, "SPLITSWORD") == 0 || strcmp(n, "SUPERFLAMESWORD") == 0 ||
	    strcmp(n, "LIGHTNINGSWORD") == 0 || strstr(n, "SCROLL") != NULL || strcmp(n, "armour") == 0 ||
	    strcmp(n, "spellbook") == 0)
		return IK_GEAR;
	if (strcmp(n, "POTION") == 0 || strcmp(n, "pot1") == 0 || strcmp(n, "pot2") == 0 || strcmp(n, "pot3") == 0 ||
	    strcmp(n, "mushroom") == 0 || strcmp(n, "cheese1") == 0 || strcmp(n, "bread1") == 0)
		return IK_HEAL;
	if (strcmp(n, "spiral") == 0)
		return IK_EXIT;
	return IK_NONE;
}

// An item GetItem() would still pick up.
bool ItemAvailable(const PLAYER &it) {
	return it.bIsPlayerAlive == TRUE && it.attackspeed <= 0 && it.rname[0] != '\0';
}

float HealthFrac() {
	const PLAYER &me = player_list[trueplayernum];
	return me.hp > 0 ? (float)me.health / (float)me.hp : 1.0f;
}

bool DoorCanOpen(const State &s, int d) {
	if (d < 0 || d >= doorcounter || d >= MAX_OBJECTDOORS)
		return false;
	if (door[d].open != 0 || door[d].type == 1)
		return false;
	if (door[d].doornum < 0 || door[d].doornum >= oblist_length)
		return false;
	const int keys = player_list[trueplayernum].keys;
	if (s.doorSkip[d] >= 0 && keys <= s.doorSkip[d])
		return false;
	bool locked = (door[d].key != 0 && door[d].key != -1) || door[d].key == -2;
	return !locked || keys > 0;
}

//-----------------------------------------------------------------------------
// Weapons
//-----------------------------------------------------------------------------
bool IsScroll(int gun) {
	return gun >= 0 && gun <= num_your_guns && strstr(your_gun[gun].gunname, "SCROLL") != NULL;
}

bool ScrollReady(int gun) {
	return IsScroll(gun) && your_gun[gun].active != 0 && your_gun[gun].x_offset > 0.0f;
}

int BestMeleeGun() {
	int best = -1;
	float bestValue = -1e9f;
	for (int i = 0; i < num_your_guns; i++) {
		if (!your_gun[i].active || IsScroll(i))
			continue;
		float value = (your_gun[i].damage2 + 1) * 0.5f + (float)your_gun[i].sdamage + 0.5f * (float)your_gun[i].sattack;
		if (value > bestValue) {
			bestValue = value;
			best = i;
		}
	}
	return best;
}

int BestAttackScroll() {
	int best = -1;
	int bestDamage = -1;
	for (int i = 0; i < num_your_guns; i++) {
		if (!ScrollReady(i) || strstr(your_gun[i].gunname, "HEALING") != NULL)
			continue;
		if (your_gun[i].damage2 > bestDamage) {
			bestDamage = your_gun[i].damage2;
			best = i;
		}
	}
	return best;
}

int HealScroll() {
	for (int i = 0; i < num_your_guns; i++) {
		if (ScrollReady(i) && strstr(your_gun[i].gunname, "HEALING") != NULL)
			return i;
	}
	return -1;
}

int WantedGun(const State &s) {
	const PLAYER &me = player_list[trueplayernum];
	const bool monsterGoal = s.kind == T_MONSTER && s.index >= 0 && s.index < num_monsters;

	if (me.health < me.hp && HealthFrac() < kLowHealth) {
		int heal = HealScroll();
		if (heal >= 0)
			return heal;
	}

	if (monsterGoal && s.los && HealthFrac() > 0.30f) {
		int scroll = BestAttackScroll();
		float lower = IsScroll(current_gun) ? kScrollKeepRange : kScrollMinRange;
		if (scroll >= 0 && s.tdist >= lower && s.tdist <= kScrollMaxRange && monster_list[s.index].health >= kScrollMinMonsterHp)
			return scroll;
	}

	return BestMeleeGun();
}

// Steps the game's own weapon cycling toward the wanted gun, one step at a time like a human tapping the key.
void ManageWeapon(State &s, float dt) {
	s.switchCooldown = fmaxf(0.0f, s.switchCooldown - dt);
	s.switchBan = fmaxf(0.0f, s.switchBan - dt);

	int wanted = WantedGun(s);
	if (wanted < 0 || wanted == current_gun) {
		s.switchAttempts = 0;
		return;
	}
	if (s.switchCooldown > 0.0f || s.switchBan > 0.0f)
		return;

	if (s.switchAttempts > num_your_guns + 1) {
		s.switchAttempts = 0;
		s.switchBan = 8.0f;
		return;
	}

	CycleNextWeapon();
	s.switchAttempts++;
	s.switchCooldown = 0.30f;
}

//-----------------------------------------------------------------------------
// Explored map
//-----------------------------------------------------------------------------
inline long long CellKey(float x, float z) {
	long long cx = (long long)floorf(x / kVisitedCell);
	long long cz = (long long)floorf(z / kVisitedCell);
	return (cx << 32) ^ (cz & 0xFFFFFFFFLL);
}

void MarkVisited(float x, float z) {
	unsigned char &v = g_visited[CellKey(x, z)];
	if (v < 255)
		v++;
}

int Visits(float x, float z) {
	auto it = g_visited.find(CellKey(x, z));
	return it == g_visited.end() ? 0 : (int)it->second;
}

//-----------------------------------------------------------------------------
// Goal selection
//-----------------------------------------------------------------------------
struct Cand {
	int kind = T_NONE;
	int index = -1;
	float x = 0.0f, y = 0.0f, z = 0.0f;
	float score = 1e30f;
};

void Consider(const State &s, Cand &best, Cand &cur, int kind, int index, float x, float y, float z, float score) {
	Cand c;
	c.kind = kind;
	c.index = index;
	c.x = x;
	c.y = y;
	c.z = z;
	c.score = score;
	if (score < best.score)
		best = c;
	if (kind == s.kind && index == s.index)
		cur = c;
}

void SelectGoal(State &s) {
	const PLAYER &me = player_list[trueplayernum];
	const float px = m_vEyePt.x, py = m_vEyePt.y, pz = m_vEyePt.z;
	const float hpFrac = HealthFrac();
	const bool weak = hpFrac < kWeakHealth;
	const bool hurt = me.health < me.hp;
	const bool wantExit = s.idleTime > kExitIdleTime;

	Cand best, cur;

	for (int i = 0; i < num_monsters; i++) {
		const PLAYER &m = monster_list[i];
		if (!Huntable(m) || Banned(s, T_MONSTER, i))
			continue;
		float dx = m.x - px, dz = m.z - pz;
		float d = sqrtf(dx * dx + dz * dz);
		float dy = fabsf(m.y - py);
		if (d > kSenseRadius || dy > kMaxMonsterDy)
			continue;
		float score = d + 1.5f * dy;
		if (d < 450.0f)
			score *= 0.5f; // finish what is close first
		if (weak && d > 250.0f)
			score += 600.0f; // hurt: do not go looking for new fights
		Consider(s, best, cur, T_MONSTER, i, m.x, m.y, m.z, score);
	}

	for (int i = 0; i < itemlistcount; i++) {
		const PLAYER &it = item_list[i];
		if (!ItemAvailable(it))
			continue;
		int cat = ItemCategory(it.rname);
		if (cat == IK_NONE || Banned(s, cat == IK_EXIT ? T_EXIT : T_ITEM, i))
			continue;
		if (cat == IK_HEAL && !hurt)
			continue; // GetItem ignores healing items at full health
		if (cat == IK_EXIT && !wantExit)
			continue;
		float dx = it.x - px, dz = it.z - pz;
		float d = sqrtf(dx * dx + dz * dz);
		float dy = fabsf(it.y - py);
		if (cat == IK_EXIT) {
			Consider(s, best, cur, T_EXIT, i, it.x, it.y, it.z, d);
			continue;
		}
		if (d > kSenseRadius || dy > kMaxItemDy)
			continue;
		float score = d * 0.85f + 1.5f * dy;
		if (cat == IK_GEAR)
			score *= 0.8f;
		if (cat == IK_HEAL)
			score *= weak ? 0.15f : (hpFrac < 0.7f ? 0.6f : 1.0f);
		Consider(s, best, cur, T_ITEM, i, it.x, it.y, it.z, score);
	}

	for (int d = 0; d < doorcounter && d < MAX_OBJECTDOORS; d++) {
		if (!DoorCanOpen(s, d) || Banned(s, T_DOOR, d))
			continue;
		const OBJECTLIST &o = oblist[door[d].doornum];
		float dx = o.x - px, dz = o.z - pz;
		float dist = sqrtf(dx * dx + dz * dz);
		float dy = fabsf(o.y - py);
		if (dist > kDoorSenseRadius || dy > kMaxDoorDy)
			continue;
		Consider(s, best, cur, T_DOOR, d, o.x, o.y, o.z, dist + 250.0f);
	}

	// Hysteresis: do not drop the current goal for one that is only slightly better.
	if (cur.kind != T_NONE && cur.score <= best.score * 1.35f)
		best = cur;

	if (best.kind == T_NONE) {
		if (s.kind != T_EXPLORE) {
			ClearGoal(s);
			s.kind = T_EXPLORE;
		}
		return;
	}

	if (best.kind != s.kind || best.index != s.index) {
		ClearGoal(s);
		s.kind = best.kind;
		s.index = best.index;
	}
	s.tx = best.x;
	s.ty = best.y;
	s.tz = best.z;
	s.idleTime = 0.0f;
}

// Closed door within reach of the use key (the game measures from the look-at point).
void ScanDoors(State &s) {
	s.nearDoor = -1;
	float bestD = 1e9f;
	for (int d = 0; d < doorcounter && d < MAX_OBJECTDOORS; d++) {
		if (door[d].doornum < 0 || door[d].doornum >= oblist_length)
			continue;

		// A door we pressed and that did not move is locked.
		if (s.doorPulse[d] > 0.0f && door[d].open == 0 && s.clock - s.doorPulse[d] > 0.5f && s.doorSkip[d] < 0) {
			s.doorSkip[d] = player_list[trueplayernum].keys;
			s.doorPulse[d] = 0.0f;
			if (s.kind == T_DOOR && s.index == d)
				ClearGoal(s);
		}

		if (!DoorCanOpen(s, d))
			continue;
		const OBJECTLIST &o = oblist[door[d].doornum];
		float dist = FastDistance(m_vLookatPt.x - o.x, m_vLookatPt.y - o.y, m_vLookatPt.z - o.z);
		if (dist <= 160.0f && dist < bestD) {
			bestD = dist;
			s.nearDoor = d;
		}
	}
}

//-----------------------------------------------------------------------------
// Steering
//-----------------------------------------------------------------------------
void PickExploreDirection(State &s, const V3 &o) {
	float bestScore = -1.0f;
	int best = -1;
	for (int i = 0; i < kSlots; i++) {
		V3 dir = { g_slotX[i], 0.0f, g_slotZ[i] };
		float open = fminf(CastRay(V3{ o.x, o.y + kLowWhiskerY, o.z }, dir, kExploreLook, RAY_WALLS),
		                   CastRay(V3{ o.x, o.y + kHighWhiskerY, o.z }, dir, kExploreLook, RAY_WALLS));
		if (open < 130.0f)
			continue;

		V3 probe = { o.x + dir.x * kLedgeProbeDist, o.y + kLedgeProbeRise, o.z + dir.z * kLedgeProbeDist };
		if (CastRay(probe, V3{ 0.0f, -1.0f, 0.0f }, kLedgeProbeDrop, RAY_FLOORS) >= kLedgeProbeDrop)
			continue;

		float near1 = fminf(open - 30.0f, 220.0f);
		float v1 = (float)Visits(o.x + dir.x * near1, o.z + dir.z * near1);
		float v2 = open > 380.0f ? (float)Visits(o.x + dir.x * 380.0f, o.z + dir.z * 380.0f) : 3.0f;
		float novelty = 0.65f / (1.0f + v1) + 0.35f / (1.0f + v2);
		float score = 0.5f * Sat(open / kExploreLook) + 1.2f * novelty +
		              0.3f * fmaxf(0.0f, g_slotX[i] * s.exploreX + g_slotZ[i] * s.exploreZ) + 0.2f * Rand01(s);
		if (score > bestScore) {
			bestScore = score;
			best = i;
		}
	}

	if (best < 0) {
		// Dead end: turn around.
		s.exploreX = -s.exploreX;
		s.exploreZ = -s.exploreZ;
		if (fabsf(s.exploreX) + fabsf(s.exploreZ) < 1e-3f) {
			s.exploreX = g_slotX[0];
			s.exploreZ = g_slotZ[0];
		}
	} else {
		s.exploreX = g_slotX[best];
		s.exploreZ = g_slotZ[best];
	}
	s.exploreTimer = 2.0f + 1.5f * Rand01(s);
}

void Think(State &s) {
	const V3 origin = { m_vEyePt.x, m_vEyePt.y, m_vEyePt.z };

	MarkVisited(origin.x, origin.z);
	SelectGoal(s);
	ScanDoors(s);

	// Direction and distance to the goal.
	const bool hasPos = s.kind == T_MONSTER || s.kind == T_ITEM || s.kind == T_DOOR || s.kind == T_EXIT;
	float tx = s.steerX, tz = s.steerZ, tdist = 0.0f;
	if (hasPos) {
		float dx = s.tx - origin.x, dz = s.tz - origin.z;
		tdist = sqrtf(dx * dx + dz * dz);
		if (tdist > 1e-3f) {
			tx = dx / tdist;
			tz = dz / tdist;
		}
	}
	s.tdist = tdist;

	// Line of sight to monsters and items: two rays so a single stair lip does not blind the player.
	LosRay los[2] = {};
	const bool losGoal = s.kind == T_MONSTER || s.kind == T_ITEM || s.kind == T_EXIT;
	for (int r = 0; r < 2; r++) {
		float yOff = r == 0 ? 0.0f : 20.0f;
		V3 from = { origin.x + tx * 10.0f, origin.y + yOff, origin.z + tz * 10.0f };
		V3 to = { s.tx - tx * 25.0f, s.ty + yOff, s.tz - tz * 25.0f };
		V3 dv = Sub(to, from);
		float len = sqrtf(Dot(dv, dv));
		los[r].o = from;
		los[r].len = len;
		los[r].blocked = false;
		los[r].active = losGoal && tdist > 60.0f && len > 1.0f;
		los[r].d = len > 1e-3f ? V3{ dv.x / len, dv.y / len, dv.z / len } : V3{ tx, 0.0f, tz };
	}

	GatherGeometry(origin, los, 2);

	bool blocked = losGoal && los[0].active && los[0].blocked && los[1].blocked;
	s.losBlockedCount = blocked ? s.losBlockedCount + 1 : 0;
	s.los = !losGoal || s.losBlockedCount < 2;

	bool hasGround = CastRay(V3{ origin.x, origin.y + kLedgeProbeRise, origin.z }, V3{ 0.0f, -1.0f, 0.0f }, kLedgeProbeDrop + 60.0f, RAY_FLOORS) < kLedgeProbeDrop + 60.0f;

	// Stuck detection: we tried to walk but barely moved.
	if (s.walkExpected > 90.0f) {
		if (s.walkMoved < 0.3f * s.walkExpected) {
			s.stuckCount++;
			s.escapeTimer = 0.7f + 0.35f * (float)(s.stuckCount < 4 ? s.stuckCount : 4);
			float leftLen = CastRay(V3{ origin.x, origin.y, origin.z }, V3{ -tz, 0.0f, tx }, 200.0f, RAY_WALLS);
			float rightLen = CastRay(V3{ origin.x, origin.y, origin.z }, V3{ tz, 0.0f, -tx }, 200.0f, RAY_WALLS);
			if (fabsf(leftLen - rightLen) < 15.0f)
				s.escapeSide = -s.escapeSide;
			else
				s.escapeSide = leftLen > rightLen ? 1 : -1;
			if (s.stuckCount >= 2)
				s.jumpPulse = true; // might be a ledge or a low obstacle
			s.exploreTimer = 0.0f;
		} else if (s.stuckCount > 0) {
			s.stuckCount--;
		}
		s.walkExpected = 0.0f;
		s.walkMoved = 0.0f;
	}

	// Base direction of travel.
	if (s.kind == T_EXPLORE) {
		if (s.exploreTimer <= 0.0f || (s.exploreX == 0.0f && s.exploreZ == 0.0f))
			PickExploreDirection(s, origin);
		tx = s.exploreX;
		tz = s.exploreZ;
	}

	s.hold = false;
	if (s.kind == T_MONSTER && tdist < kHoldRange && s.los)
		s.hold = true;
	if (s.holdTimer > 0.0f)
		s.hold = true;

	float dx = tx, dz = tz;
	if (s.escapeTimer > 0.0f && !s.hold) {
		dx = -tz * s.escapeSide + 0.25f * tx;
		dz = tx * s.escapeSide + 0.25f * tz;
		float dl = sqrtf(dx * dx + dz * dz);
		if (dl > 1e-4f) {
			dx /= dl;
			dz /= dl;
		}
	}

	// Danger map: whiskers at two heights, longer toward where we want to go, plus ledge probes.
	float danger[kSlots];
	for (int i = 0; i < kSlots; i++) {
		float align = g_slotX[i] * dx + g_slotZ[i] * dz;
		float len = kWhiskerBase + kWhiskerExtra * (align > 0.0f ? align : 0.0f);

		V3 dir = { g_slotX[i], 0.0f, g_slotZ[i] };
		float d = fminf(CastRay(V3{ origin.x, origin.y + kLowWhiskerY, origin.z }, dir, len, RAY_WALLS),
		                CastRay(V3{ origin.x, origin.y + kHighWhiskerY, origin.z }, dir, len, RAY_WALLS));

		float dg = 0.0f;
		if (d < len)
			dg = Sat(1.0f - (d - kBodyRadius) / (len - kBodyRadius));

		if (hasGround && align > -0.35f) {
			V3 probe = { origin.x + dir.x * kLedgeProbeDist, origin.y + kLedgeProbeRise, origin.z + dir.z * kLedgeProbeDist };
			if (CastRay(probe, V3{ 0.0f, -1.0f, 0.0f }, kLedgeProbeDrop, RAY_FLOORS) >= kLedgeProbeDrop)
				dg = fmaxf(dg, kLedgeDanger);
		}
		danger[i] = dg;
	}

	// Walking over the level exit by accident would leave the level.
	if (s.kind != T_EXIT) {
		for (int i = 0; i < itemlistcount; i++) {
			const PLAYER &it = item_list[i];
			if (!ItemAvailable(it) || strcmp(it.rname, "spiral") != 0)
				continue;
			float nx = it.x - origin.x, nz = it.z - origin.z;
			float nd2 = nx * nx + nz * nz;
			if (nd2 > kSpiralAvoidRadius * kSpiralAvoidRadius || nd2 < 1e-4f || fabsf(it.y - origin.y) > 150.0f)
				continue;
			float nd = sqrtf(nd2);
			nx /= nd;
			nz /= nd;
			float prox = 1.0f - nd / kSpiralAvoidRadius;
			for (int i2 = 0; i2 < kSlots; i2++) {
				float dd = g_slotX[i2] * nx + g_slotZ[i2] * nz;
				if (dd > 0.2f)
					danger[i2] = fmaxf(danger[i2], 0.8f * (0.4f + prox) * dd);
			}
		}
	}

	// Missiles fired by monsters: slots toward the missile get dangerous, more so when it flies at us.
	if (your_missle != nullptr) {
		for (int mc = 0; mc < MAX_MISSLE; mc++) {
			if (your_missle[mc].active != 1 || your_missle[mc].playertype != 0)
				continue;

			float mx = your_missle[mc].x - origin.x;
			float my = your_missle[mc].y - origin.y;
			float mz = your_missle[mc].z - origin.z;
			if (fabsf(my) > 100.0f)
				continue;

			float md2 = mx * mx + mz * mz;
			if (md2 > kMissileRadius * kMissileRadius || md2 < 1e-4f)
				continue;

			float md = sqrtf(md2);
			float nx = mx / md;
			float nz = mz / md;

			float vx = your_missle[mc].velocity.x;
			float vz = your_missle[mc].velocity.z;
			float vl = sqrtf(vx * vx + vz * vz);
			float prox = 1.0f - md / kMissileRadius;

			float headingToward = 0.0f;
			if (vl > 1e-3f)
				headingToward = fmaxf(0.0f, (vx / vl) * (-nx) + (vz / vl) * (-nz));

			for (int i = 0; i < kSlots; i++) {
				float dotMissile = g_slotX[i] * nx + g_slotZ[i] * nz;
				if (dotMissile > -0.2f) {
					float mDanger = prox * (dotMissile + 0.2f) / 1.2f;
					mDanger *= (1.0f + 1.2f * headingToward);
					danger[i] = fmaxf(danger[i], Sat(mDanger));
				}
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

	// An exploring player that sees a wall ahead picks a new direction right away.
	if (s.kind == T_EXPLORE && dil[SlotFromDir(s.exploreX, s.exploreZ)] > 0.55f)
		s.exploreTimer = 0.0f;

	// Wall following: when the way to the goal is blocked, commit to one side for a while to avoid dithering.
	float fX = -dz, fZ = dx;
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

	s.boxedIn = false;
	if (best < 0) {
		// Boxed in: keep pushing toward the goal, slowly.
		s.steerX = dx;
		s.steerZ = dz;
		s.speedScale = 0.25f;
		s.boxedIn = true;
		return;
	}

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
}

//-----------------------------------------------------------------------------
// Per frame
//-----------------------------------------------------------------------------
void ClearDrive(CONTROLS *c) {
	c->bLeft = c->bRight = 0;
	c->bHeadUp = c->bHeadDown = 0;
	c->bForward = c->bBackward = FALSE;
	c->bForwardButton = FALSE;
	c->bStepLeft = c->bStepRight = FALSE;
	c->bFire = c->bFire2 = FALSE;
	c->spell = FALSE;
	c->missle = FALSE;
	c->opendoor = FALSE;
	c->bNextWeap = c->bPrevWeap = FALSE;
}

void ResetState(State &s) {
	s.clock = 0.0f;
	s.thinkTimer = 0.0f;
	s.deadTimer = 0.0f;
	ClearGoal(s);
	s.tdist = 0.0f;
	s.idleTime = 0.0f;
	s.steerX = 0.0f;
	s.steerZ = 1.0f;
	s.speedScale = 1.0f;
	s.hold = false;
	s.boxedIn = false;
	s.exploreX = s.exploreZ = 0.0f;
	s.exploreTimer = 0.0f;
	s.prevValid = false;
	s.cmdMove = false;
	s.walkExpected = s.walkMoved = 0.0f;
	s.stuckCount = 0;
	s.escapeTimer = 0.0f;
	s.followTimer = 0.0f;
	s.jumpPulse = false;
	s.nearDoor = -1;
	s.holdTimer = 0.0f;
	for (int i = 0; i < MAX_OBJECTDOORS; i++) {
		s.doorPulse[i] = 0.0f;
		s.doorSkip[i] = -1;
	}
	s.switchCooldown = 0.0f;
	s.switchBan = 0.0f;
	s.switchAttempts = 0;
	for (Ban &b : s.bans)
		b = Ban();
	s.nextBan = 0;
	g_visited.clear();
}

void Respawn(State &s) {
	PLAYER &me = player_list[trueplayernum];
	me.bIsPlayerAlive = TRUE;
	me.health = me.hp;
	ResetState(s);
	Say("Player AI got back up.", 0, 255, 255);
}

void Drive(State &s, CONTROLS *c, float dt) {
	const PLAYER &me = player_list[trueplayernum];
	const float px = m_vEyePt.x, pz = m_vEyePt.z;

	// Monsters move and die: refresh the goal every frame so the swing stays on target.
	if (s.kind == T_MONSTER) {
		if (s.index < 0 || s.index >= num_monsters || !Huntable(monster_list[s.index])) {
			ClearGoal(s);
			s.thinkTimer = 0.0f;
		} else {
			s.tx = monster_list[s.index].x;
			s.ty = monster_list[s.index].y;
			s.tz = monster_list[s.index].z;
		}
	} else if (s.kind == T_ITEM || s.kind == T_EXIT) {
		if (s.index < 0 || s.index >= itemlistcount || !ItemAvailable(item_list[s.index])) {
			ClearGoal(s);
			s.thinkTimer = 0.0f;
		}
	} else if (s.kind == T_DOOR) {
		if (!DoorCanOpen(s, s.index)) {
			ClearGoal(s);
			s.thinkTimer = 0.0f;
		}
	}

	const bool hasPos = s.kind == T_MONSTER || s.kind == T_ITEM || s.kind == T_DOOR || s.kind == T_EXIT;
	float tdx = 0.0f, tdz = 0.0f;
	if (hasPos) {
		tdx = s.tx - px;
		tdz = s.tz - pz;
		s.tdist = sqrtf(tdx * tdx + tdz * tdz);
		if (s.tdist > 1e-3f) {
			tdx /= s.tdist;
			tdz /= s.tdist;
		}
	}
	const float dy = hasPos ? fabsf(s.ty - m_vEyePt.y) : 0.0f;

	// Give up on goals we are not getting any closer to.
	if (hasPos) {
		const bool fighting = s.kind == T_MONSTER && s.tdist < 150.0f && s.los;
		if (s.tdist < s.bestDist - 40.0f) {
			s.bestDist = s.tdist;
			s.noProgress = 0.0f;
		} else if (!fighting) {
			s.noProgress += dt;
		}

		if (s.kind != T_MONSTER && s.tdist < 60.0f)
			s.arriveTimer += dt; // standing on it but nothing was picked up (wrong height)
		else
			s.arriveTimer = 0.0f;

		const float limit = s.kind == T_MONSTER ? 9.0f : 6.0f;
		if (s.noProgress > limit || s.arriveTimer > 1.5f) {
			AddBan(s, s.kind, s.index, s.kind == T_MONSTER ? 15.0f : 30.0f);
			ClearGoal(s);
			s.thinkTimer = 0.0f;
		}
	}
	if (s.kind == T_EXPLORE || s.kind == T_NONE)
		s.idleTime += dt;

	// Facing.
	const bool monsterGoal = s.kind == T_MONSTER;
	const bool engaged = monsterGoal && s.tdist < kEngageRange && (s.los || s.tdist < 120.0f);
	float faceYaw = engaged ? YawOf(tdx, tdz) : YawOf(s.steerX, s.steerZ);
	if (s.hold && !engaged && hasPos)
		faceYaw = YawOf(tdx, tdz); // waiting at a door: look at it

	float a = angy;
	float err = Wrap180(faceYaw - a);
	float step = Clamp(err * fminf(1.0f, kTurnResponse * dt), -kMaxTurnRate * dt, kMaxTurnRate * dt);
	angy = Wrap360(a + step);
	const float err2 = Wrap180(faceYaw - angy);

	// Look level, or at the target when casting.
	float wantPitch = 0.0f;
	const bool casting = usespell == 1 && monsterGoal && s.los && ScrollReady(current_gun) &&
	                     strstr(your_gun[current_gun].gunname, "HEALING") == NULL;
	if (casting && s.tdist > 1.0f)
		wantPitch = Clamp(atan2f(m_vEyePt.y - s.ty, s.tdist) / k, -60.0f, 60.0f);
	look_up_ang += Clamp(wantPitch - look_up_ang, -kPitchRate * dt, kPitchRate * dt);

	// Movement.
	const float fx = sinf(angy * k), fz = cosf(angy * k); // forward
	const float rx = cosf(angy * k), rz = -sinf(angy * k); // right (strafe 7 is angy + 90)

	if (!s.hold) {
		if (engaged) {
			float f = s.steerX * fx + s.steerZ * fz;
			float r = s.steerX * rx + s.steerZ * rz;
			if (f > 0.35f)
				c->bForward = TRUE;
			else if (f < -0.35f)
				c->bBackward = TRUE;
			if (r > 0.35f)
				c->bStepRight = TRUE;
			else if (r < -0.35f)
				c->bStepLeft = TRUE;
		} else if (fabsf(err2) < 55.0f) {
			c->bForward = TRUE;
			if (s.escapeTimer > 0.0f) {
				// Walking sideways around an obstacle: strafe instead of turning on the spot.
				float r = s.steerX * rx + s.steerZ * rz;
				if (r > 0.5f)
					c->bStepRight = TRUE;
				else if (r < -0.5f)
					c->bStepLeft = TRUE;
			}
		}
	}
	if (engaged && s.tdist < kBackoffRange && fabsf(err2) < 60.0f) {
		c->bBackward = TRUE;
		c->bForward = FALSE;
	}
	s.cmdMove = c->bForward || c->bBackward || c->bStepLeft || c->bStepRight;

	// Attack.
	if (monsterGoal && usespell == 0 && s.tdist < kSwingRange && dy < 70.0f && fabsf(err2) < 30.0f && s.los)
		c->bFire = TRUE;

	if (usespell == 1) {
		if (strstr(your_gun[current_gun].gunname, "HEALING") != NULL) {
			if (ScrollReady(current_gun) && me.health < me.hp)
				c->spell = TRUE;
		} else if (casting && s.tdist > 100.0f && fabsf(err2) < 6.0f) {
			c->spell = TRUE;
		}
	}

	// Doors: one press per door, pressing again on an open door would close it.
	if (s.nearDoor >= 0 && s.holdTimer <= 0.0f && DoorCanOpen(s, s.nearDoor)) {
		c->opendoor = TRUE;
		s.doorPulse[s.nearDoor] = s.clock;
		s.holdTimer = 0.9f; // let it swing open before walking into it
		s.nearDoor = -1;
	}

	if (s.jumpPulse) {
		c->missle = TRUE;
		s.jumpPulse = false;
	}
}

} // namespace

void PlayerAIReset() {
	ResetState(g);
}

void PlayerAISetEnabled(bool enabled) {
	if (g.enabled == enabled)
		return;
	g.enabled = enabled;
	if (enabled) {
		EnsureSlots();
		ResetState(g);
		g.rng = 0x2545F491u ^ (unsigned int)(m_vEyePt.x * 7.0f) ^ (unsigned int)(m_vEyePt.z * 13.0f);
		if (g.rng == 0)
			g.rng = 0x2545F491u;
		Say("Player AI on. Press F1 to take over.", 0, 255, 255);
	} else {
		Say("Player AI off.", 0, 255, 255);
	}
}

bool PlayerAIIsEnabled() {
	return g.enabled;
}

void PlayerAIToggle() {
	PlayerAISetEnabled(!g.enabled);
}

void PlayerAIUpdate(Controls_typ *controls, float fElapsedTime) {
	if (!g.enabled || controls == nullptr)
		return;

	EnsureSlots();
	State &s = g;

	float dt = fElapsedTime;
	if (!(dt > 0.0f))
		dt = 1.0f / 60.0f;
	dt = Clamp(dt, 0.001f, kMaxDt);

	ClearDrive(controls);
	s.clock += dt;

	PLAYER &me = player_list[trueplayernum];
	if (me.bIsPlayerAlive == FALSE) {
		s.deadTimer += dt;
		if (s.deadTimer > kRespawnDelay)
			Respawn(s);
		return;
	}
	s.deadTimer = 0.0f;

	// How far did the last frame's walking actually take us?
	if (s.prevValid) {
		float mx = m_vEyePt.x - s.prevX, mz = m_vEyePt.z - s.prevZ;
		if (s.cmdMove) {
			s.walkExpected += kPlayerWalkSpeed * dt;
			s.walkMoved += sqrtf(mx * mx + mz * mz);
		}
	}
	s.prevX = m_vEyePt.x;
	s.prevZ = m_vEyePt.z;
	s.prevValid = true;

	s.escapeTimer = fmaxf(0.0f, s.escapeTimer - dt);
	s.followTimer = fmaxf(0.0f, s.followTimer - dt);
	s.exploreTimer = fmaxf(0.0f, s.exploreTimer - dt);
	s.holdTimer = fmaxf(0.0f, s.holdTimer - dt);

	s.thinkTimer -= dt;
	if (s.thinkTimer <= 0.0f) {
		Think(s);
		s.thinkTimer = kThinkInterval;
	}

	ManageWeapon(s, dt);
	Drive(s, controls, dt);
}
