#pragma once
// ParticleSystem.hpp — DSParticleSystem
// CPU-driven particle system integrated into the D3D12 vertex-streaming pipeline.
// Supports spinning billboards, air drag, per-type size curves, and multiple
// effect types (hit, critical, fire, sparks, magic).

#define DS_MAX_EMITTERS 16  // simultaneous burst emitters
#define DS_MAX_PARTICLES 48 // particles per emitter

// Effect category — controls physics tuning, size curve, and texture selection.
enum DSEmitterType {
	EMITTER_HIT,      // blood / impact burst (normal hit)
	EMITTER_CRITICAL, // large blood burst    (critical hit)
	EMITTER_FIRE,     // upward fire cone     (torches, lava splatter)
	EMITTER_SPARKS,   // high-speed sparks    (metal-on-metal)
	EMITTER_MAGIC,    // magical ring swirl   (spell impact)
};

// A single simulated particle.
struct DSParticle {
	float x, y, z;    // world-space position
	float vx, vy, vz; // velocity (units / second)
	float life;       // remaining lifetime (seconds)
	float maxLife;    // initial lifetime (seconds)
	float size;       // billboard half-extent (world units)
	float spin;       // current billboard rotation angle (radians)
	float spinRate;   // rotation speed (radians / second)
	float drag;       // velocity damping coefficient (fraction / second)
};

// One burst emitter (spawned on a hit event).
struct DSParticleEmitter {
	bool active;
	DSEmitterType type;
	int particleCount;
	DSParticle particles[DS_MAX_PARTICLES];
};

// ---- Hit type enum -------------------------------------------------------
// Passed to DisplayDamage() to select the appropriate particle effect.
enum DSHitType {
	HIT_SWORD,     // melee weapon — blood burst
	HIT_MISSILE,   // magic missile / generic projectile — sparks
	HIT_FIREBALL,  // fire spell — fire cone bloom
	HIT_LIGHTNING, // lightning spell — magic ring swirl
};

// ---- Public API -----------------------------------------------------------

// Spawn a burst of hit particles at world-space (x, y, z).
// Pass critical = true for a larger / longer-lived burst (critical hit).
void SpawnHitParticles(float x, float y, float z, bool critical);

// Spawn a rising fire-cone effect (torches, lava splatter, burning objects).
void SpawnFireParticles(float x, float y, float z);

// Spawn high-speed directional sparks (metal-on-metal, trap triggers).
void SpawnSparkParticles(float x, float y, float z);

// Spawn an outward magical ring burst (spell impacts, enchanted items).
void SpawnMagicParticles(float x, float y, float z);

// Advance particle physics.  Must be called once per frame BEFORE DrawParticles().
void UpdateParticles(float dt);

// Emit billboard quads for all active particles into the current frame's
// vertex / draw-call arrays (src_v, ObjectsToDraw, etc.).
// Call this during the geometry-building phase of UpdateWorld().
void DrawParticles();
