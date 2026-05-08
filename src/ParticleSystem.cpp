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
extern XMFLOAT3          m_vEyePt;
extern XMFLOAT3          m_vLookatPt;
extern D3DVERTEX2       *src_v;
extern int               cnt;
extern int               number_of_polys_per_frame;
extern int              *verts_per_poly;
extern D3DPRIMITIVETYPE *dp_commands;
extern BOOL             *dp_command_index_mode;
extern int              *texture_list_buffer;

// random_num(n) returns [0, n-1]  (declared in world.hpp)

// ---- emitter pool ---------------------------------------------------------
static DSParticleEmitter gEmitters[DS_MAX_EMITTERS];
static bool              gInitialized = false;

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

    // Find a free emitter slot.
    DSParticleEmitter *em = nullptr;
    for (int i = 0; i < DS_MAX_EMITTERS; i++) {
        if (!gEmitters[i].active) {
            em = &gEmitters[i];
            break;
        }
    }
    if (!em) return; // all slots busy – silently skip

    em->active        = true;
    em->critical      = critical;
    em->particleCount = critical ? DS_MAX_PARTICLES : 8;

    const float baseSpeed    = critical ? 170.0f : 110.0f;
    const float baseLifetime = critical ? 0.65f  : 0.42f;
    const float baseSize     = critical ? 110.0f  :  7.0f;

    static const float twoPi = 6.28318530f;

    for (int i = 0; i < em->particleCount; i++) {
        DSParticle &p = em->particles[i];

        // Scatter spawn position around the hit point.
        p.x = x + RandRange(-6.0f,  6.0f);
        p.y = y + RandRange( 0.0f, 12.0f);
        p.z = z + RandRange(-6.0f,  6.0f);

        // Random velocity: azimuth [0, 2π], elevation [15°, 75°].
        float azimuth   = RandRange(0.0f, twoPi);
        float elevation = RandRange(0.2618f, 1.3090f); // 15° .. 75° in radians
        float speed     = RandRange(baseSpeed * 0.5f, baseSpeed);

        p.vx = cosf(azimuth) * cosf(elevation) * speed;
        p.vy = sinf(elevation) * speed;
        p.vz = sinf(azimuth)  * cosf(elevation) * speed;

        p.life    = baseLifetime * RandRange(0.7f, 1.0f);
        p.maxLife = p.life;
        p.size    = baseSize     * RandRange(0.7f, 1.0f);
    }
}

// ---- UpdateParticles ------------------------------------------------------
void UpdateParticles(float dt) {
    EnsureInit();

    const float gravity = -280.0f; // world-units / sec²

    for (int e = 0; e < DS_MAX_EMITTERS; e++) {
        DSParticleEmitter &em = gEmitters[e];
        if (!em.active) continue;

        bool anyAlive = false;
        for (int i = 0; i < em.particleCount; i++) {
            DSParticle &p = em.particles[i];
            if (p.life <= 0.0f) continue;

            p.life -= dt;
            if (p.life <= 0.0f) continue;

            // Simple Euler integration with gravity.
            p.vy += gravity * dt;
            p.x  += p.vx * dt;
            p.y  += p.vy * dt;
            p.z  += p.vz * dt;

            anyAlive = true;
        }

        if (!anyAlive)
            em.active = false;
    }
}

// ---- DrawParticles --------------------------------------------------------
// Each active particle is rendered as a camera-facing billboard quad
// (two triangles, six vertices) written directly into src_v.
// All particles share a single ObjectsToDraw entry (texture alias 370).
void DrawParticles() {
    EnsureInit();

    // Compute camera billboard axes.
    XMVECTOR eye  = XMLoadFloat3(&m_vEyePt);
    XMVECTOR look = XMLoadFloat3(&m_vLookatPt);
    XMVECTOR fwd  = XMVector3Normalize(look - eye);
    XMVECTOR up   = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    // Camera-right: perpendicular to world-up and view direction.
    XMVECTOR rgt  = XMVector3Normalize(XMVector3Cross(up, fwd));

    XMFLOAT3 camRight;
    XMStoreFloat3(&camRight, rgt);

    XMFLOAT3 norm; // normal toward camera (used for lighting)
    XMStoreFloat3(&norm, XMVectorNegate(fwd));

    const int startCnt = cnt;
    int       numVerts  = 0;

    for (int e = 0; e < DS_MAX_EMITTERS; e++) {
        const DSParticleEmitter &em = gEmitters[e];
        if (!em.active) continue;

        for (int i = 0; i < em.particleCount; i++) {
            const DSParticle &p = em.particles[i];
            if (p.life <= 0.0f) continue;

            // Ensure we don't overflow the vertex buffer.
            if (cnt + 6 >= MAX_NUM_QUADS) continue;

            // Billboard shrinks as the particle ages (t: 1 → 0).
            const float t    = p.life / p.maxLife;
            const float half = p.size * t;

            // Billboard right-axis and world-up axis scaled by half-size.
            const float rx = camRight.x * half;
            const float ry = camRight.y * half;
            const float rz = camRight.z * half;
            const float uy = half; // world-up: only Y component is non-zero

            const float cx = p.x, cy = p.y, cz = p.z;

            // Quad corners:
            //   v0 = top-left   (-right + up)
            //   v1 = top-right  ( right + up)
            //   v2 = bot-right  ( right - up)
            //   v3 = bot-left   (-right - up)
            // Triangle 1: v0, v1, v2
            // Triangle 2: v0, v2, v3

            struct { float ox, oy, oz, u, v; } vdata[6] = {
                { -rx,  +uy, -rz,  0.0f, 0.0f }, // v0
                { +rx,  +uy, +rz,  1.0f, 0.0f }, // v1
                { +rx,  -uy, +rz,  1.0f, 1.0f }, // v2
                { -rx,  +uy, -rz,  0.0f, 0.0f }, // v0
                { +rx,  -uy, +rz,  1.0f, 1.0f }, // v2
                { -rx,  -uy, -rz,  0.0f, 1.0f }, // v3
            };

            for (int vi = 0; vi < 6; vi++) {
                D3DVERTEX2 &dv = src_v[cnt];
                memset(&dv, 0, sizeof(D3DVERTEX2));
                dv.x  = cx + vdata[vi].ox;
                dv.y  = cy + vdata[vi].oy;
                dv.z  = cz + vdata[vi].oz;
                dv.tu = vdata[vi].u;
                dv.tv = vdata[vi].v;
                dv.nx = norm.x;
                dv.ny = norm.y;
                dv.nz = norm.z;
                dv.CastShadow = 0;
                cnt++;
            }
            numVerts += 6;
        }
    }

    if (numVerts == 0) return;

    // Register a single non-indexed draw call for all particle quads.
    const int slot = number_of_polys_per_frame;

    ObjectsToDraw[slot].vert_index   = slot;
    ObjectsToDraw[slot].srcstart     = startCnt;
    ObjectsToDraw[slot].srcfstart    = 0;
    ObjectsToDraw[slot].objectId     = -1;
    ObjectsToDraw[slot].castshaddow  = 0; // particles cast no shadow
    ObjectsToDraw[slot].vertsperpoly = numVerts;
    ObjectsToDraw[slot].facesperpoly = numVerts / 3;

    verts_per_poly[slot]        = numVerts;
    dp_commands[slot]           = D3DPT_TRIANGLELIST;
    dp_command_index_mode[slot] = 1; // USE_NON_INDEXED_DP
    texture_list_buffer[slot]   = 370; // blood / impact texture alias

    number_of_polys_per_frame++;
}
