// ParticleSystem.cpp — DSParticleSystem implementation
// Integrates into the existing D3D12 vertex-streaming pipeline:
//   UpdateParticles() advances physics each frame.
//   DrawParticles()   emits billboard quads into src_v / ObjectsToDraw.

#include "ParticleSystem.hpp"
#include "world.hpp"          // POLY_SORT, D3DVERTEX2, ObjectsToDraw, etc.
#include "GlobalSettings.hpp" // MAX_NUM_QUADS

#include <cstring>
#include <cmath>

using namespace DirectX;

// ---- pipeline globals (defined in ProcessModel.cpp / World.cpp) -----------
extern XMFLOAT3 m_vEyePt;
extern D3DVERTEX2 *src_v;
extern int cnt;
extern int number_of_polys_per_frame;
extern int *verts_per_poly;
extern D3DPRIMITIVETYPE *dp_commands;
extern BOOL *dp_command_index_mode;
extern int *texture_list_buffer;
extern float k;       // pi/180 (degrees -> radians)
extern D3DVALUE angy; // camera yaw in degrees

// random_num(n) returns [0, n-1]  (declared in world.hpp)

// ---- emitter pool ---------------------------------------------------------
static DSParticleEmitter gEmitters[DS_MAX_EMITTERS];
static bool gInitialized = false;

static void EnsureInit() {
	if (!gInitialized) {
		memset(gEmitters, 0, sizeof(gEmitters));
		gInitialized = true;
	}
}

// Returns a float in [lo, hi).
static inline float RandRange(float lo, float hi) {
	float t = static_cast<float>(random_num(10000)) / 9999.0f;
	return lo + t * (hi - lo);
}

// ---- SpawnHitParticles ----------------------------------------------------
void SpawnHitParticles(float x, float y, float z, bool critical) {
	EnsureInit();

	DSParticleEmitter *em = nullptr;
	for (int i = 0; i < DS_MAX_EMITTERS; i++) {
		if (!gEmitters[i].active) {
			em = &gEmitters[i];
			break;
		}
	}
	if (!em)
		return;

	em->active = true;
	em->type = critical ? EMITTER_CRITICAL : EMITTER_HIT;
	em->particleCount = DS_MAX_PARTICLES;

	const float baseSpeed = critical ? 340.0f : 220.0f;
	const float baseLifetime = critical ? 13.0f : 10.5f;
	const float baseSize = critical ? 5.0f : 2.5f;
	const float baseDrag = critical ? 1.8f : 2.2f; // friction (fraction/sec)

	static const float twoPi = 6.28318530f;

	for (int i = 0; i < em->particleCount; i++) {
		DSParticle &p = em->particles[i];

		p.x = x + RandRange(-6.0f, 6.0f);
		p.y = y + RandRange(0.0f, 12.0f);
		p.z = z + RandRange(-6.0f, 6.0f);

		float azimuth = RandRange(0.0f, twoPi);
		float elevation = RandRange(0.2618f, 1.3090f); // 15° .. 75°
		float speed = RandRange(baseSpeed * 0.5f, baseSpeed);

		p.vx = cosf(azimuth) * cosf(elevation) * speed;
		p.vy = sinf(elevation) * speed;
		p.vz = sinf(azimuth) * cosf(elevation) * speed;

		p.life = baseLifetime * RandRange(0.6f, 1.0f);
		p.maxLife = p.life;
		p.size = baseSize * RandRange(0.6f, 1.0f);
		p.drag = baseDrag * RandRange(0.7f, 1.3f);

		// Spin: hit particles tumble moderately; critical ones spin faster.
		p.spin = RandRange(0.0f, twoPi);
		p.spinRate = RandRange(-4.0f, 4.0f) * (critical ? 2.0f : 1.0f);
	}
}
void SpawnFireParticles(float x, float y, float z) {
	EnsureInit();

	DSParticleEmitter *em = nullptr;
	for (int i = 0; i < DS_MAX_EMITTERS; i++) {
		if (!gEmitters[i].active) {
			em = &gEmitters[i];
			break;
		}
	}
	if (!em)
		return;

	em->active = true;
	em->type = EMITTER_FIRE;
	em->particleCount = DS_MAX_PARTICLES;

	static const float twoPi = 6.28318530f;

	for (int i = 0; i < em->particleCount; i++) {
		DSParticle &p = em->particles[i];

		// ----------------------------------------------------
		// 1. Spawn in a tight sphere (explosion origin)
		// ----------------------------------------------------
		float rx = RandRange(-1.0f, 1.0f);
		float ry = RandRange(-1.0f, 1.0f);
		float rz = RandRange(-1.0f, 1.0f);

		float len = sqrtf(rx * rx + ry * ry + rz * rz) + 0.0001f;
		rx /= len;
		ry /= len;
		rz /= len;

		p.x = x + rx * RandRange(0.0f, 4.0f);
		p.y = y + ry * RandRange(0.0f, 4.0f);
		p.z = z + rz * RandRange(0.0f, 4.0f);

		// ----------------------------------------------------
		// 2. Fireball layers (using only existing fields)
		// ----------------------------------------------------
		float layer = RandRange(0.0f, 1.0f);
		float speed;

		if (layer < 0.25f) {
			// HOT CORE FLASH
			speed = RandRange(600.0f, 900.0f);
			p.life = RandRange(0.2f, 0.4f);
			p.size = RandRange(3.0f, 4.0f);
		} else if (layer < 0.75f) {
			// MAIN FIREBALL
			speed = RandRange(300.0f, 550.0f);
			p.life = RandRange(0.6f, 1.2f);
			p.size = RandRange(2.0f, 3.0f);
		} else {
			// EMBERS
			speed = RandRange(80.0f, 200.0f);
			p.life = RandRange(2.0f, 4.0f);
			p.size = RandRange(1.0f, 1.8f);
		}

		// ----------------------------------------------------
		// 3. Velocity (spherical blast + chaos)
		// ----------------------------------------------------
		p.vx = rx * speed + RandRange(-40.0f, 40.0f);
		p.vy = ry * speed + RandRange(-40.0f, 40.0f);
		p.vz = rz * speed + RandRange(-40.0f, 40.0f);

		// ----------------------------------------------------
		// 4. Drag, spin, life
		// ----------------------------------------------------
		p.maxLife = p.life;
		p.drag = RandRange(0.5f, 1.5f); // explosions don't slow immediately
		p.spin = RandRange(0.0f, twoPi);
		p.spinRate = RandRange(-10.0f, 10.0f);
	}
}

// ---- SpawnSparkParticles --------------------------------------------------
void SpawnSparkParticles(float x, float y, float z) {
	EnsureInit();

	DSParticleEmitter *em = nullptr;
	for (int i = 0; i < DS_MAX_EMITTERS; i++) {
		if (!gEmitters[i].active) {
			em = &gEmitters[i];
			break;
		}
	}
	if (!em)
		return;

	em->active = true;
	em->type = EMITTER_SPARKS;
	em->particleCount = DS_MAX_PARTICLES;

	static const float twoPi = 6.28318530f;

	for (int i = 0; i < em->particleCount; i++) {
		DSParticle &p = em->particles[i];

		p.x = x + RandRange(-2.0f, 2.0f);
		p.y = y + RandRange(0.0f, 4.0f);
		p.z = z + RandRange(-2.0f, 2.0f);

		// Full hemisphere burst — high speed, shallow-to-steep launch angles
		float azimuth = RandRange(0.0f, twoPi);
		float elevation = RandRange(0.1f, 1.5708f);
		float speed = RandRange(200.0f, 450.0f); // fast sparks

		p.vx = cosf(azimuth) * cosf(elevation) * speed;
		p.vy = sinf(elevation) * speed;
		p.vz = sinf(azimuth) * cosf(elevation) * speed;

		p.life = RandRange(3.0f, 7.0f);
		p.maxLife = p.life;
		p.size = RandRange(0.8f, 1.8f); // tiny, sharp sparks
		p.drag = RandRange(0.4f, 0.9f); // low drag — sparks fly far
		p.spin = RandRange(0.0f, twoPi);
		p.spinRate = RandRange(-8.0f, 8.0f); // fast spin for shimmer
	}
}

// ---- SpawnMagicParticles --------------------------------------------------
void SpawnMagicParticles(float x, float y, float z) {
	EnsureInit();

	DSParticleEmitter *em = nullptr;
	for (int i = 0; i < DS_MAX_EMITTERS; i++) {
		if (!gEmitters[i].active) {
			em = &gEmitters[i];
			break;
		}
	}
	if (!em)
		return;

	em->active = true;
	em->type = EMITTER_MAGIC;
	em->particleCount = DS_MAX_PARTICLES / 3;

	static const float twoPi = 6.28318530f;

	for (int i = 0; i < em->particleCount; i++) {
		DSParticle &p = em->particles[i];

		// Spawn in a ring around the impact point
		float angle = twoPi * static_cast<float>(i) / static_cast<float>(em->particleCount);
		float radius = RandRange(4.0f, 10.0f);

		p.x = x + cosf(angle) * radius;
		p.y = y + RandRange(2.0f, 8.0f);
		p.z = z + sinf(angle) * radius;

		// Outward + upward drift
		float speed = RandRange(40.0f, 120.0f);
		p.vx = cosf(angle) * speed;
		p.vy = RandRange(20.0f, 80.0f); // gentle lift
		p.vz = sinf(angle) * speed;

		p.life = RandRange(8.0f, 14.0f);
		p.maxLife = p.life;
		p.size = RandRange(1.0f, 2.0f);
		p.drag = RandRange(1.5f, 1.5f);
		p.spin = angle; // offset spin per particle for visual spread
		p.spinRate = RandRange(-3.0f, 3.0f);
	}
}

// ---- UpdateParticles ------------------------------------------------------
void UpdateParticles(float dt) {
	EnsureInit();

	const float gravity = -280.0f; // world-units / sec²

	for (int e = 0; e < DS_MAX_EMITTERS; e++) {
		DSParticleEmitter &em = gEmitters[e];
		if (!em.active)
			continue;

		bool anyAlive = false;
		for (int i = 0; i < em.particleCount; i++) {
			DSParticle &p = em.particles[i];
			if (p.life <= 0.0f)
				continue;

			p.life -= dt;
			if (p.life <= 0.0f)
				continue;

			// Air drag: exponential decay approximated by (1 - drag*dt).
			// Clamp so drag can never reverse velocity direction.
			float lateralDamp = 1.0f - p.drag * dt;
			if (lateralDamp < 0.0f)
				lateralDamp = 0.0f;
			p.vx *= lateralDamp;
			p.vz *= lateralDamp;
			// Vertical drag is reduced (gravity wins on the way down).
			float vertDamp = 1.0f - p.drag * 0.4f * dt;
			if (vertDamp < 0.0f)
				vertDamp = 0.0f;
			p.vy += gravity * dt;
			p.vy *= vertDamp;

			p.x += p.vx * dt;
			p.y += p.vy * dt;
			p.z += p.vz * dt;

			// Billboard spin.
			p.spin += p.spinRate * dt;

			anyAlive = true;
		}

		if (!anyAlive)
			em.active = false;
	}
}

// ---- DrawParticles --------------------------------------------------------
// Each active particle is rendered as a camera-facing billboard quad
// (two triangles, six vertices) written directly into src_v.
//
// Billboard axes are derived from the camera yaw angle (angy) rather than
// a cross-product of world-up and view-direction, giving consistent CW
// winding for all camera orientations.
//
// Per-particle spin is achieved by rotating the right and up axes around
// the billboard normal before building the quad:
//   new_right = camRight * cos(spin) + worldUp * sin(spin)
//   new_up    = -camRight * sin(spin) + worldUp * cos(spin)
//
// One draw call is issued per active emitter so that each type can bind
// a different texture.
void DrawParticles() {
	EnsureInit();

	const float cosA = cosf(angy * k);
	const float sinA = sinf(angy * k);
	const float normX = -sinA;
	const float normZ = -cosA;

	for (int e = 0; e < DS_MAX_EMITTERS; e++) {
		const DSParticleEmitter &em = gEmitters[e];
		if (!em.active)
			continue;

		const int startCnt = cnt;
		int numVerts = 0;

		for (int i = 0; i < em.particleCount; i++) {
			const DSParticle &p = em.particles[i];
			if (p.life <= 0.0f)
				continue;
			if (cnt + 6 >= MAX_NUM_QUADS)
				break;

			// Normalised age: 1 at birth → 0 at death.
			const float t = p.life / p.maxLife;

			// Per-type size curve.
			float half;
			switch (em.type) {
			case EMITTER_FIRE:
				// Bloom: tiny at birth, peaks at midlife, tiny at death.
				half = p.size * sinf((1.0f - t) * 3.14159f);
				break;
			case EMITTER_CRITICAL:
				// Stay larger for longer (t^0.67 falloff).
				half = p.size * powf(t, 0.67f);
				break;
			case EMITTER_MAGIC:
				// Linger big, drop off quickly near end (sqrt).
				half = p.size * sqrtf(t);
				break;
			default: // EMITTER_HIT, EMITTER_SPARKS
				half = p.size * t;
				break;
			}
			if (half < 0.01f)
				continue;

			// Spin-rotated billboard axes (rotate right/up in billboard plane).
			const float cs = cosf(p.spin);
			const float sn = sinf(p.spin);

			// new_right = camRight * cs + worldUp * sn
			//   camRight = (cosA, 0, -sinA),  worldUp = (0, 1, 0)
			const float rrx = cosA * cs * half;
			const float rry = sn * half;
			const float rrz = -sinA * cs * half;

			// new_up = -camRight * sn + worldUp * cs
			const float rux = -cosA * sn * half;
			const float ruy = cs * half;
			const float ruz = sinA * sn * half;

			const float cx = p.x, cy = p.y, cz = p.z;

			// Quad corners — CW from camera for all yaw angles and spin values:
			//   v0 = top-left  = -right + up
			//   v1 = top-right =  right + up
			//   v2 = bot-right =  right - up
			//   v3 = bot-left  = -right - up
			// Triangle 1: v0, v1, v2   Triangle 2: v0, v2, v3
			struct {
				float ox, oy, oz, u, v;
			} vdata[6] = {
				{ -rrx + rux, -rry + ruy, -rrz + ruz, 0.0f, 0.0f }, // v0
				{ rrx + rux, rry + ruy, rrz + ruz, 1.0f, 0.0f },    // v1
				{ rrx - rux, rry - ruy, rrz - ruz, 1.0f, 1.0f },    // v2
				{ -rrx + rux, -rry + ruy, -rrz + ruz, 0.0f, 0.0f }, // v0
				{ rrx - rux, rry - ruy, rrz - ruz, 1.0f, 1.0f },    // v2
				{ -rrx - rux, -rry - ruy, -rrz - ruz, 0.0f, 1.0f }, // v3
			};

			for (int vi = 0; vi < 6; vi++) {
				D3DVERTEX2 &dv = src_v[cnt];
				memset(&dv, 0, sizeof(D3DVERTEX2));
				dv.x = cx + vdata[vi].ox;
				dv.y = cy + vdata[vi].oy;
				dv.z = cz + vdata[vi].oz;
				dv.tu = vdata[vi].u;
				dv.tv = vdata[vi].v;
				dv.nx = normX;
				dv.ny = 0.0f;
				dv.nz = normZ;
				dv.CastShadow = 0;
				cnt++;
			}
			numVerts += 6;
		}

		if (numVerts == 0)
			continue;

		// One draw call per emitter (allows per-type texture binding).
		const int slot = number_of_polys_per_frame;

		ObjectsToDraw[slot].vert_index = slot;
		ObjectsToDraw[slot].srcstart = startCnt;
		ObjectsToDraw[slot].srcfstart = 0;
		ObjectsToDraw[slot].objectId = -1;
		ObjectsToDraw[slot].castshaddow = 0;
		ObjectsToDraw[slot].vertsperpoly = numVerts;
		ObjectsToDraw[slot].facesperpoly = numVerts / 3;

		verts_per_poly[slot] = numVerts;
		dp_commands[slot] = D3DPT_TRIANGLELIST;
		dp_command_index_mode[slot] = 1; // USE_NON_INDEXED_DP

		// Per-type texture selection.
		switch (em.type) {
		case EMITTER_CRITICAL:
			texture_list_buffer[slot] = 370 + random_num(5);
			break;
		case EMITTER_FIRE:
			texture_list_buffer[slot] = 200;
			break;
		case EMITTER_SPARKS:
			texture_list_buffer[slot] = 239;
			break;
		case EMITTER_MAGIC:
			texture_list_buffer[slot] = 157 - 1;
			break;
		default: // EMITTER_HIT
			texture_list_buffer[slot] = 200;
			break;
		}

		number_of_polys_per_frame++;
	}
}
