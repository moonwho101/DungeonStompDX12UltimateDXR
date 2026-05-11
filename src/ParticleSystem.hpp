#pragma once
// ParticleSystem.hpp — DSParticleSystem
// Simple CPU-driven hit-particle system, conceptually similar to Godot's
// GPUParticles3D but software-simulated and integrated into the existing
// D3D12 vertex-streaming pipeline.

#define DS_MAX_EMITTERS  10   // simultaneous burst emitters
#define DS_MAX_PARTICLES 25   // particles per emitter

// A single simulated particle.
struct DSParticle {
    float x,  y,  z;    // world-space position
    float vx, vy, vz;   // velocity (units / second)
    float life;          // remaining lifetime (seconds)
    float maxLife;       // initial lifetime (seconds)
    float size;          // billboard half-extent (world units)
};

// One burst emitter (spawned on a hit event).
struct DSParticleEmitter {
    bool       active;
    bool       critical;
    int        particleCount;
    DSParticle particles[DS_MAX_PARTICLES];
};

// ---- Public API -----------------------------------------------------------

// Spawn a burst of hit particles at world-space (x, y, z).
// Pass critical = true for a larger / longer-lived burst (critical hit).
void SpawnHitParticles(float x, float y, float z, bool critical);

// Advance particle physics.  Must be called once per frame BEFORE DrawParticles().
void UpdateParticles(float dt);

// Emit billboard quads for all active particles into the current frame's
// vertex / draw-call arrays (src_v, ObjectsToDraw, etc.).
// Call this during the geometry-building phase of UpdateWorld().
void DrawParticles();
