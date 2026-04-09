//***************************************************************************************
// Raytracing.hlsl by Mark Longo 2026 All Rights Reserved
// DirectX Raytracing Shader with PBR Lighting and Path Tracing
// Ray generation, closest hit, and miss shaders for DXR with physically-based rendering
// Features: inline shadow rays (DXR 1.1), full Cook-Torrance PBR, ACES tone mapping,
//           atmospheric fog, wet floor reflectance, per-light flicker,
//           multi-bounce path tracing with Russian Roulette, GGX importance sampling
//***************************************************************************************

#define MaxLights 32
#define PI 3.14159265f

// Max point lights that cast shadow rays (performance knob)
#define MAX_SHADOW_LIGHTS 32

// Fog density for dungeon atmosphere
#define FOG_DENSITY 0.0025f

// Shadow ray bias to prevent self-intersection
#define SHADOW_BIAS 0.15f

// Global illumination: single-bounce indirect diffuse (legacy, for reference)
#define GI_BOUNCE_STRENGTH 0.35f   // scale of the GI contribution
#define GI_MAX_DIST        80.0f   // max distance the GI bounce ray travels

//=============================================================================
// PATH TRACING CONFIGURATION
//=============================================================================
#define PATH_TRACING_ENABLED    1       // 0 = single-bounce GI, 1 = full path tracing
#define PT_MAX_BOUNCES          4       // Maximum path depth (2-8 recommended)
#define PT_MIN_BOUNCES          2       // Minimum bounces before Russian Roulette
#define PT_RR_PROBABILITY       0.95f   // Russian Roulette survival probability
#define PT_SAMPLES_PER_PIXEL    1       // Samples per pixel per frame (temporal accumulation handles the rest)
#define PT_CLAMP_FIREFLIES      5.0f    // Max luminance to prevent fireflies
#define PT_SPECULAR_PROBABILITY 0.5f    // Probability of specular vs diffuse bounce (MIS)


// Glare and flicker constants (for RayGen glare loop)
#define GLARE_INNER_WEIGHT      0.6f
#define GLARE_INNER_FALLOFF     0.9f
#define GLARE_OUTER_WEIGHT      0.4f
#define GLARE_OUTER_FALLOFF     4.7f
#define GLARE_THRESHOLD         0.001f
#define GLARE_SCALE             0.45f
#define FLICKER_BASE_STRENGTH   1.0f
#define FLICKER_FREQ            8.0f
#define FLICKER_AMP             0.25f

// Max distance for glare effect from camera to light
#define GLARE_MAX_DISTANCE      1500.0f

// Light structure matching CPU-side
struct Light
{
	float3 Strength;
	float FalloffStart;
	float3 Direction;
	float FalloffEnd;
	float3 Position;
	float SpotPower;
};

// Scene constants
cbuffer SceneConstants : register(b0)
{
	float4x4 gInvViewProj;
	float3 gCameraPos;
	float gPad0;
	float4 gAmbientLight;
	Light gLights[MaxLights];
	uint gNumLights;
	float gTotalTime;
	float gRoughness;
	float gMetallic;
	float gRayConeSpreadAngle;
	uint gOutside;    // 1 = outdoor level, 0 = indoor dungeon
	float2 gPad1;
};

// Raytracing output
RWTexture2D<float4> gOutput : register(u0);

// Acceleration structure
RaytracingAccelerationStructure gScene : register(t0);

// Vertex buffer (48 bytes per vertex: Pos(12) + Normal(12) + TexC(8) + TangentU(12) + CastShadow(4))
struct Vertex
{
	float3 Pos;
	float3 Normal;
	float2 TexC;
	float3 TangentU;
	int CastShadow;
};

ByteAddressBuffer gVertices : register(t1);

// Texture array (copied to DXR heap)
Texture2D gTextures[] : register(t2);

// Direct binding for sky cube map (Texture index 484 -> space2, t0)
TextureCube gCubeMap : register(t0, space2);

// Per-primitive texture index buffer
ByteAddressBuffer gPrimitiveTextureIndices : register(t0, space1);

struct AliasData
{
	uint textureIndex;
	int normalMapIndex;
	float roughness;
	float metallic;
	float4 diffuseAlbedo;
	float3 fresnelR0;
	float pad0;
};

// Per-alias material data buffer (texture indices + PBR material state)
StructuredBuffer<AliasData> gAliasData : register(t1, space1);

// Sampler for texture sampling
SamplerState gSampler : register(s0);

// Helper to load vertex from byte address buffer
Vertex LoadVertex(uint vertexIndex)
{
    // 48 bytes per vertex = 12 DWORDs
	uint address = vertexIndex * 48;
    
	Vertex v;
    // Load position (3 floats)
	v.Pos.x = asfloat(gVertices.Load(address));
	v.Pos.y = asfloat(gVertices.Load(address + 4));
	v.Pos.z = asfloat(gVertices.Load(address + 8));
    
    // Load normal (3 floats)
	v.Normal.x = asfloat(gVertices.Load(address + 12));
	v.Normal.y = asfloat(gVertices.Load(address + 16));
	v.Normal.z = asfloat(gVertices.Load(address + 20));
    
    // Load texcoord (2 floats)
	v.TexC.x = asfloat(gVertices.Load(address + 24));
	v.TexC.y = asfloat(gVertices.Load(address + 28));
    
    // Load tangent (3 floats)
	v.TangentU.x = asfloat(gVertices.Load(address + 32));
	v.TangentU.y = asfloat(gVertices.Load(address + 36));
	v.TangentU.z = asfloat(gVertices.Load(address + 40));
    
    // Load CastShadow (int)
	v.CastShadow = gVertices.Load(address + 44);
	return v;
}

// Ray payload
struct RayPayload
{
	float4 color;
	uint depth; // recursion depth for transparency
	bool isGIRay; // true => secondary GI ray, skip further GI recursion
	float hitT; // distance to surface (used for volumetric glare limits)
};

// Path tracing payload - carries throughput and state through bounces
struct PathPayload
{
	float3 radiance;        // Accumulated radiance
	float3 throughput;      // Path throughput (energy remaining)
	uint   bounceCount;     // Current bounce depth
	uint   seed;            // Random seed for this path
	bool   terminated;      // Path terminated flag
	float  hitT;            // Distance to surface
};

// Hard-coded transparent texture ranges (matching CPU-side alpha skip logic)
bool IsTransparentTexture(uint texIdx)
{
	if (texIdx >= 94 && texIdx <= 101)
		return true; // flames / effects
	if (texIdx >= 278 && texIdx <= 295)
		return true; // portal effects
	if (texIdx >= 205 && texIdx <= 209)
		return true; // flare
	if (texIdx >= 370 && texIdx <= 378)
		return true; // blood splatter decals
    //if (texIdx == 156)                  return true;  // wall crystal
    //if (texIdx == 181)                  return true;  // potion
	return false;
}

// Hard-coded player weapon texture range (also treated as transparent for shadow rays to prevent black weapon silhouettes)
bool IsWeaponTexture(uint texIdx)
{
	if (texIdx >= 127 && texIdx <= 137)
		return true; // player weapons (swords, bows, etc)
	return false;
}

// Hard-coded player weapon texture range (also treated as transparent for shadow rays to prevent black weapon silhouettes)
bool IsFlameTexture(uint texIdx)
{
	if (texIdx >= 94 && texIdx <= 101)
		return true; // flames / effects
	return false;
}

//=============================================================================
// Normal Map Helper
//=============================================================================

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    // Unpack from [0,1] to [-1,1]
	float3 normalT = 2.0f * normalMapSample - 1.0f;
    
    // Build orthonormal TBN basis
	float3 N = unitNormalW;
	float3 T = normalize(tangentW - dot(tangentW, N) * N);
	float3 B = cross(N, T);
	float3x3 TBN = float3x3(T, B, N);
    
	return mul(normalT, TBN);
}

//=============================================================================
// PBR Helper Functions
//=============================================================================

float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0f);
	float NdotH2 = NdotH * NdotH;
	float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
	denom = PI * denom * denom;
	return a2 / max(denom, 0.0001f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r * r) / 8.0f;
	return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0f);
	float NdotL = max(dot(N, L), 0.0f);
	float ggx1 = GeometrySchlickGGX(NdotV, roughness);
	float ggx2 = GeometrySchlickGGX(NdotL, roughness);
	return ggx1 * ggx2;
}

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

// Torch flicker effect
float TorchFlicker(float baseStrength, float time, float freq, float amp, float offset)
{
    // Each sin term oscillates around zero; sum them, then remap [−1,+1] → [0,1].
	float t = time + offset;
	float flicker = sin(t * freq) * 0.50f // primary wave
                  + sin(t * (freq * 2.13f)) * 0.25f // harmonic
                  + sin(t * (freq * 0.77f)) * 0.15f; // sub-harmonic
    // flicker is now in [−0.90, +0.90]; remap to [0, 1] and saturate for safety.
	flicker = saturate(flicker * 0.5f + 0.5f);
	return baseStrength * (1.0f - amp * flicker);
}

// PBR lighting for directional light
float3 ComputeDirectionalLight(Light L, float3 albedo, float3 fresnelR0, float3 N, float3 V, float roughness, float metallic)
{
	float3 F0 = lerp(fresnelR0, albedo, metallic);
	float3 Ld = -normalize(L.Direction);
	float3 H = normalize(V + Ld);
    
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, Ld, roughness);
	float NdotL = max(dot(N, Ld), 0.0f);
	float NdotV = max(dot(N, V), 0.0f);
	float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);
    
	float3 numerator = NDF * G * F;
	float denominator = 4.0f * NdotV * NdotL + 0.001f;
	float3 specular = numerator / denominator;
    
	float3 kS = F;
	float3 kD = 1.0f - kS;
	kD *= 1.0f - metallic;
    
	float3 diffuse = kD * albedo / PI;
	return (diffuse + specular) * L.Strength * NdotL;
}

// PBR lighting for point light (with torch flicker)
float3 ComputePointLight(Light L, float3 pos, float3 albedo, float3 fresnelR0, float3 N, float3 V, float roughness, float metallic, float lightIndex)
{
	float3 lightVec = L.Position - pos;
	float d = length(lightVec);
    
	if (d > L.FalloffEnd)
		return float3(0.0f, 0.0f, 0.0f);
    
	float3 F0 = lerp(fresnelR0, albedo, metallic);
	float3 Ld = normalize(lightVec);
	float3 H = normalize(V + Ld);
    
	float attenuation = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	float flicker = TorchFlicker(1.0f, gTotalTime, 8.0f, 0.25f, lightIndex);
    
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, Ld, roughness);
	float NdotL = max(dot(N, Ld), 0.0f);
	float NdotV = max(dot(N, V), 0.0f);
	float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);
    
	float3 numerator = NDF * G * F;
	float denominator = 4.0f * NdotV * NdotL + 0.001f;
	float3 specular = numerator / denominator;
    
	float3 kS = F;
	float3 kD = 1.0f - kS;
	kD *= 1.0f - metallic;
    
	float3 diffuse = kD * albedo / PI;
	float3 lightStrength = L.Strength * flicker;
    
	return (diffuse + specular) * lightStrength * NdotL * attenuation;
}

// PBR lighting for spot light
float3 ComputeSpotLight(Light L, float3 pos, float3 albedo, float3 fresnelR0, float3 N, float3 V, float roughness, float metallic)
{
	float3 lightVec = L.Position - pos;
	float d = length(lightVec);
    
	if (d > L.FalloffEnd)
		return float3(0.0f, 0.0f, 0.0f);
    
	float3 F0 = lerp(fresnelR0, albedo, metallic);
	float3 Ld = normalize(lightVec);
	float3 H = normalize(V + Ld);
    
	float attenuation = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	float spotFactor = pow(max(dot(-Ld, normalize(L.Direction)), 0.0f), L.SpotPower);
	attenuation *= spotFactor;
    
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, Ld, roughness);
	float NdotL = max(dot(N, Ld), 0.0f);
	float NdotV = max(dot(N, V), 0.0f);
	float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);
    
	float3 numerator = NDF * G * F;
	float denominator = 4.0f * NdotV * NdotL + 0.001f;
	float3 specular = numerator / denominator;
    
	float3 kS = F;
	float3 kD = 1.0f - kS;
	kD *= 1.0f - metallic;
    
	float3 diffuse = kD * albedo / PI;
	return (diffuse + specular) * L.Strength * NdotL * attenuation;
}

//=============================================================================
// Path Tracing Random Number Generation (PCG-based)
//=============================================================================

// PCG random number generator state advance
uint PCGHash(uint input)
{
	uint state = input * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

// Generate random float in [0, 1) from seed, advances seed
float RandomFloat(inout uint seed)
{
	seed = PCGHash(seed);
	return float(seed) / 4294967296.0f;
}

// Generate 2D random sample
float2 RandomFloat2(inout uint seed)
{
	return float2(RandomFloat(seed), RandomFloat(seed));
}

// Create initial seed from pixel coordinates and frame number
uint InitRandomSeed(uint2 pixelCoord, uint frame)
{
	return PCGHash(pixelCoord.x + PCGHash(pixelCoord.y + PCGHash(frame)));
}

//=============================================================================
// Path Tracing Sampling Functions
//=============================================================================

// Cosine-weighted hemisphere sampling (for diffuse)
float3 SampleCosineHemisphere(float2 u, float3 N, out float pdf)
{
	float phi = 2.0f * PI * u.x;
	float cosTheta = sqrt(u.y);
	float sinTheta = sqrt(1.0f - u.y);
	
	// Build orthonormal basis
	float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 T = normalize(cross(up, N));
	float3 B = cross(N, T);
	
	float3 dir = normalize(sinTheta * cos(phi) * T + sinTheta * sin(phi) * B + cosTheta * N);
	pdf = cosTheta / PI;
	return dir;
}

// GGX importance sampling for specular (samples visible normals)
float3 SampleGGX(float2 u, float3 N, float3 V, float roughness, out float pdf)
{
	float a = roughness * roughness;
	float a2 = a * a;
	
	// Sample half-vector in tangent space
	float phi = 2.0f * PI * u.x;
	float cosTheta = sqrt((1.0f - u.y) / (1.0f + (a2 - 1.0f) * u.y));
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	
	float3 H_tangent = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
	
	// Build orthonormal basis around N
	float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 T = normalize(cross(up, N));
	float3 B = cross(N, T);
	
	// Transform to world space
	float3 H = normalize(T * H_tangent.x + B * H_tangent.y + N * H_tangent.z);
	
	// Reflect V around H to get L
	float3 L = reflect(-V, H);
	
	// Compute PDF
	float NdotH = max(dot(N, H), 0.001f);
	float VdotH = max(dot(V, H), 0.001f);
	float D = DistributionGGX(N, H, roughness);
	pdf = D * NdotH / (4.0f * VdotH);
	
	return L;
}

// Evaluate BSDF for diffuse component
float3 EvaluateDiffuseBSDF(float3 albedo, float metallic)
{
	// Metals have no diffuse
	return albedo * (1.0f - metallic) / PI;
}

// Evaluate BSDF for specular component (Cook-Torrance)
float3 EvaluateSpecularBSDF(float3 albedo, float3 fresnelR0, float3 N, float3 V, float3 L, float roughness, float metallic)
{
	float3 H = normalize(V + L);
	float3 F0 = lerp(fresnelR0, albedo, metallic);
	
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, L, roughness);
	float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);
	
	float NdotV = max(dot(N, V), 0.001f);
	float NdotL = max(dot(N, L), 0.001f);
	
	return (NDF * G * F) / (4.0f * NdotV * NdotL + 0.001f);
}

// Russian Roulette probability based on throughput luminance
float ComputeRRProbability(float3 throughput)
{
	float luminance = dot(throughput, float3(0.2126f, 0.7152f, 0.0722f));
	return min(PT_RR_PROBABILITY, luminance);
}

// Clamp fireflies
float3 ClampFireflies(float3 radiance)
{
	float lum = dot(radiance, float3(0.2126f, 0.7152f, 0.0722f));
	if (lum > PT_CLAMP_FIREFLIES)
		return radiance * (PT_CLAMP_FIREFLIES / lum);
	return radiance;
}

//=============================================================================
// Shadow Ray (DXR 1.1 Inline Raytracing)
//=============================================================================

// Returns 1.0 if fully lit, 0.0 if fully shadowed
float TraceShadowRay(float3 origin, float3 direction, float maxDist)
{
    // DXR 1.1 Inline RayQuery for better performance in shadow tests
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.05f;
	ray.TMax = max(0.05f, maxDist - 0.1f);

	RayQuery < RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_NON_OPAQUE > q;
	q.TraceRayInline(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, ray);

	bool hitFound = false;
	while (q.Proceed())
	{
		if (q.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
		{
			uint primIdx = q.CandidatePrimitiveIndex();
			uint vertexIndex = primIdx * 3;
            
			Vertex v0 = LoadVertex(vertexIndex);
			Vertex v1 = LoadVertex(vertexIndex + 1);
			Vertex v2 = LoadVertex(vertexIndex + 2);

			bool triangleCastsShadow = (v0.CastShadow == 1) || (v1.CastShadow == 1) || (v2.CastShadow == 1);
			uint texIndex = gPrimitiveTextureIndices.Load(primIdx * 4);

			if (triangleCastsShadow && !IsTransparentTexture(texIndex) && !IsWeaponTexture(texIndex))
			{
				hitFound = true;
				q.Abort();
			}
		}
	}

	return hitFound ? 0.0f : 1.0f;
}

//=============================================================================
// Path Tracing Core Function
// Traces a complete path through the scene with multiple bounces
//=============================================================================

float3 TracePathSegment(float3 hitPos, float3 N, float3 V, float3 albedo, 
                        float3 fresnelR0, float roughness, float metallic, 
                        inout uint seed, float3 incomingThroughput)
{
    float3 totalRadiance = float3(0.0f, 0.0f, 0.0f);
    float3 throughput = incomingThroughput;
    float3 rayOrigin = hitPos;
    float3 rayDir;
    float3 currentN = N;
    float3 currentAlbedo = albedo;
    float3 currentFresnelR0 = fresnelR0;
    float currentRoughness = roughness;
    float currentMetallic = metallic;
    
    for (uint bounce = 0; bounce < PT_MAX_BOUNCES; bounce++)
    {
        // Russian Roulette termination after minimum bounces
        if (bounce >= PT_MIN_BOUNCES)
        {
            float rrProb = ComputeRRProbability(throughput);
            if (RandomFloat(seed) > rrProb)
                break;
            throughput /= rrProb;
        }
        
        // Choose between diffuse and specular sampling based on metallic
        // Higher metallic = more specular, rougher = more diffuse-like
        float specularWeight = lerp(0.04f, 1.0f, currentMetallic);
        specularWeight = lerp(specularWeight, 0.5f, currentRoughness * 0.5f);
        
        float pdf;
        float3 bsdfValue;
        float2 u = RandomFloat2(seed);
        
        if (RandomFloat(seed) < specularWeight)
        {
            // Specular bounce (GGX importance sampling)
            rayDir = SampleGGX(u, currentN, V, max(currentRoughness, 0.05f), pdf);
            if (dot(rayDir, currentN) <= 0.0f || pdf < 0.0001f)
                break;
            
            bsdfValue = EvaluateSpecularBSDF(currentAlbedo, currentFresnelR0, currentN, V, rayDir, currentRoughness, currentMetallic);
            float NdotL = max(dot(currentN, rayDir), 0.0f);
            
            // MIS weight for specular: account for the specularWeight probability
            throughput *= bsdfValue * NdotL / (pdf * specularWeight);
        }
        else
        {
            // Diffuse bounce (cosine-weighted hemisphere sampling)
            rayDir = SampleCosineHemisphere(u, currentN, pdf);
            if (pdf < 0.0001f)
                break;
            
            bsdfValue = EvaluateDiffuseBSDF(currentAlbedo, currentMetallic);
            float NdotL = max(dot(currentN, rayDir), 0.0f);
            
            // MIS weight for diffuse
            throughput *= bsdfValue * NdotL / (pdf * (1.0f - specularWeight));
        }
        
        // Prevent throughput explosion
        if (any(isnan(throughput)) || any(isinf(throughput)))
            break;
        throughput = min(throughput, float3(10.0f, 10.0f, 10.0f));
        
        // Trace the bounce ray using inline ray query
        RayDesc bounceRay;
        bounceRay.Origin = rayOrigin + currentN * SHADOW_BIAS;
        bounceRay.Direction = rayDir;
        bounceRay.TMin = 0.01f;
        bounceRay.TMax = GI_MAX_DIST;
        
        RayQuery<RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_NON_OPAQUE> bounceQuery;
        bounceQuery.TraceRayInline(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, bounceRay);
        
        bool hitSurface = false;
        float3 hitPosition;
        float3 hitNormal;
        float3 hitAlbedo = float3(0.5f, 0.5f, 0.5f);
        float3 hitFresnelR0 = float3(0.04f, 0.04f, 0.04f);
        float hitRoughness = 0.8f;
        float hitMetallic = 0.0f;
        uint hitTexIndex = 0;
        
        while (bounceQuery.Proceed())
        {
            if (bounceQuery.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
            {
                uint primIdx = bounceQuery.CandidatePrimitiveIndex();
                uint texIndex = gPrimitiveTextureIndices.Load(primIdx * 4);
                
                // Skip transparent textures for path tracing bounces
                if (!IsTransparentTexture(texIndex) && !IsWeaponTexture(texIndex))
                {
                    bounceQuery.CommitNonOpaqueTriangleHit();
                }
            }
        }
        
        if (bounceQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            hitSurface = true;
            float t = bounceQuery.CommittedRayT();
            hitPosition = bounceRay.Origin + bounceRay.Direction * t;
            
            // Get hit primitive data
            uint primIdx = bounceQuery.CommittedPrimitiveIndex();
            uint vertexIndex = primIdx * 3;
            
            Vertex v0 = LoadVertex(vertexIndex);
            Vertex v1 = LoadVertex(vertexIndex + 1);
            Vertex v2 = LoadVertex(vertexIndex + 2);
            
            float2 bary2D = bounceQuery.CommittedTriangleBarycentrics();
            float3 bary = float3(1.0f - bary2D.x - bary2D.y, bary2D.x, bary2D.y);
            
            hitNormal = normalize(v0.Normal * bary.x + v1.Normal * bary.y + v2.Normal * bary.z);
            float2 texCoord = v0.TexC * bary.x + v1.TexC * bary.y + v2.TexC * bary.z;
            
            // Ensure normal faces incoming direction
            if (dot(hitNormal, -rayDir) < 0.0f)
                hitNormal = -hitNormal;
            
            // Get material data
            uint aliasIndex = gPrimitiveTextureIndices.Load(primIdx * 4);
            AliasData ad = gAliasData[aliasIndex];
            hitTexIndex = ad.textureIndex;
            hitRoughness = ad.roughness;
            hitMetallic = ad.metallic;
            hitFresnelR0 = ad.fresnelR0;
            
            // Sample albedo texture
            if (hitTexIndex < 550)
            {
                float4 texSample = gTextures[NonUniformResourceIndex(hitTexIndex)].SampleLevel(gSampler, texCoord, 2.0f);
                hitAlbedo = texSample.rgb * ad.diffuseAlbedo.rgb;
            }
            else
            {
                hitAlbedo = ad.diffuseAlbedo.rgb;
            }
        }
        
        if (!hitSurface)
        {
            // Hit sky - sample environment and terminate
            float3 skyColor = gCubeMap.SampleLevel(gSampler, rayDir, 0).rgb;
            float upFactor = saturate(rayDir.y * 0.5f + 0.5f);
            skyColor *= lerp(0.4f, 1.0f, upFactor); // Darken lower sky for dungeon
            totalRadiance += throughput * skyColor * 0.3f; // Reduce sky contribution for indoor feel
            break;
        }
        
        // Direct lighting at hit point (next event estimation)
        float3 directLight = float3(0.0f, 0.0f, 0.0f);
        float3 hitV = -rayDir;
        float3 shadowOrigin = hitPosition + hitNormal * SHADOW_BIAS;
        
        // Sample one random light for direct illumination (stochastic next event estimation)
        if (gNumLights > 1)
        {
            uint lightIdx = 1 + uint(RandomFloat(seed) * float(min(gNumLights - 1, (uint)MaxLights - 1)));
            Light L = gLights[lightIdx];
            
            float3 lightVec = L.Position - hitPosition;
            float d = length(lightVec);
            
            if (d < L.FalloffEnd && d > 0.01f)
            {
                float3 lightDir = lightVec / d;
                float NdotL = max(dot(hitNormal, lightDir), 0.0f);
                
                if (NdotL > 0.001f)
                {
                    float shadow = TraceShadowRay(shadowOrigin, lightDir, d);
                    if (shadow > 0.0f)
                    {
                        float attenuation = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
                        float flicker = TorchFlicker(1.0f, gTotalTime, 8.0f, 0.25f, (float)lightIdx);
                        
                        // Evaluate BSDF for direct light
                        float3 H = normalize(hitV + lightDir);
                        float3 F0 = lerp(hitFresnelR0, hitAlbedo, hitMetallic);
                        float3 F = FresnelSchlick(max(dot(H, hitV), 0.0f), F0);
                        
                        float3 kS = F;
                        float3 kD = (1.0f - kS) * (1.0f - hitMetallic);
                        float3 diffuse = kD * hitAlbedo / PI;
                        
                        float NDF = DistributionGGX(hitNormal, H, hitRoughness);
                        float G = GeometrySmith(hitNormal, hitV, lightDir, hitRoughness);
                        float NdotV = max(dot(hitNormal, hitV), 0.001f);
                        float3 specular = (NDF * G * F) / (4.0f * NdotV * NdotL + 0.001f);
                        
                        // Scale by number of lights for unbiased estimate
                        float lightCount = float(min(gNumLights - 1, (uint)MaxLights - 1));
                        directLight = (diffuse + specular) * L.Strength * flicker * attenuation * NdotL * shadow * lightCount;
                    }
                }
            }
        }
        
        totalRadiance += throughput * ClampFireflies(directLight);
        
        // Setup for next bounce
        rayOrigin = hitPosition;
        V = -rayDir;
        currentN = hitNormal;
        currentAlbedo = hitAlbedo;
        currentFresnelR0 = hitFresnelR0;
        currentRoughness = hitRoughness;
        currentMetallic = hitMetallic;
    }
    
    return totalRadiance;
}

//=============================================================================
// ACES Filmic Tone Mapping
//=============================================================================

float3 ACESFilm(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

//=============================================================================
// Ray Generation Shader
//=============================================================================

[shader("raygeneration")]
void RayGen()
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;
    
    // Calculate normalized device coordinates (0 to 1, then -1 to 1)
	float2 pixelCenter = float2(launchIndex) + float2(0.5f, 0.5f);
	float2 uv = pixelCenter / float2(launchDim);
    
    // Convert to clip space (-1 to 1)
	float2 clipXY = uv * 2.0f - 1.0f;
	clipXY.y = -clipXY.y; // Flip Y for DirectX coordinate system
    
    // Unproject near and far points to get ray
	float4 nearPoint = mul(float4(clipXY, 0.0f, 1.0f), gInvViewProj);
	float4 farPoint = mul(float4(clipXY, 1.0f, 1.0f), gInvViewProj);
    
	nearPoint.xyz /= nearPoint.w;
	farPoint.xyz /= farPoint.w;
    
	float3 rayOrigin = nearPoint.xyz;
	float3 rayDirection = normalize(farPoint.xyz - nearPoint.xyz);
    
    // Trace ray
	RayDesc ray;
	ray.Origin = rayOrigin;
	ray.Direction = rayDirection;
	ray.TMin = 0.01f;
	ray.TMax = 100000.0f;
    
	RayPayload payload;
	payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
	payload.depth = 0;
	payload.isGIRay = false;
	payload.hitT = 100000.0f;
    
	TraceRay(
        gScene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        0xFF,
        0, // Hit group index
        1, // Multiplier for geometry index
        0, // Miss shader index
        ray,
        payload
    );
    
	float3 finalColor = payload.color.rgb;
    // Volumetric Fog Glow (Glare) around Point Lights
	float3 glare = float3(0.0f, 0.0f, 0.0f);
    // Glare constants now defined as macros above
    
	for (uint i = 1; i < min(gNumLights, (uint) MaxLights); ++i)
	{
		Light L = gLights[i];
		if (distance(L.Position, gCameraPos) > GLARE_MAX_DISTANCE)
			continue;
		float3 lightVec = L.Position - rayOrigin;
		float tClosest = dot(lightVec, rayDirection);
		tClosest = clamp(tClosest, 0.0f, payload.hitT);
		float3 closestPoint = rayOrigin + rayDirection * tClosest;
		float distToLight = length(closestPoint - L.Position);
        // Reduce center brightness: lower inner core weight and increase falloff
		float glow = GLARE_INNER_WEIGHT * exp(-distToLight / GLARE_INNER_FALLOFF) + GLARE_OUTER_WEIGHT * exp(-distToLight / GLARE_OUTER_FALLOFF);
		if (glow > GLARE_THRESHOLD)
		{
			float flicker = TorchFlicker(FLICKER_BASE_STRENGTH, gTotalTime, FLICKER_FREQ, FLICKER_AMP, (float) i);
			float falloff = saturate((L.FalloffEnd - distToLight) / L.FalloffEnd);
            // Increase scaling for outer glow
			glare += L.Strength * glow * flicker * falloff * GLARE_SCALE;
		}
	}
	finalColor += glare;
    
    // Apply ACES filmic tone mapping and gamma correction
	finalColor = ACESFilm(finalColor);
	finalColor = pow(finalColor, 1.0f / 2.2f);
    
	gOutput[launchIndex] = float4(finalColor, 1.0f);
}

//=============================================================================
// Miss Shader
//=============================================================================

[shader("miss")]
void Miss(inout RayPayload payload)
{
	float3 rayDir = WorldRayDirection();
    
    // Sample the sunset cube map for the sky (always used for GI bounces)
	float3 skyColor = gCubeMap.SampleLevel(gSampler, rayDir, 0).rgb;
    
    // Apply atmospheric dungeon void vertical gradient to darken the lower part 
    // of the cube if it's too bright for a dungeon, but keep most of it.
	float upFactor = saturate(rayDir.y * 0.5f + 0.5f);
	skyColor *= lerp(0.8f, 1.0f, upFactor);
    
    // For GI bounce rays, keep the raw (brighter) sRGB sample so indirect
    // lighting stays strong.  For primary (display) rays, linearize so the
    // RayGen ACES tone map + gamma correction doesn't double-encode.
    // For display rays indoors (gOutside == 0), show black sky instead of cubemap.
	if (!payload.isGIRay)
	{
		if (gOutside == 0)
		{
			// Indoor dungeon: simple dark cavern gradient.
			float ceilingFactor = smoothstep(-0.30f, 0.80f, rayDir.y);
			float horizonFactor = 1.0f - saturate(abs(rayDir.y));
			float3 dungeonSky = lerp(float3(0.006f, 0.007f, 0.010f),
			                          float3(0.030f, 0.040f, 0.054f),
			                          ceilingFactor);
			dungeonSky += float3(0.005f, 0.006f, 0.005f) * horizonFactor;
			payload.color = float4(dungeonSky, 1.0f);
			payload.hitT = 100000.0f;
			return;
		}
		skyColor = pow(max(skyColor, 0.0f), 2.2f);
	}
    
	payload.color = float4(skyColor, 1.0f);
	payload.hitT = 100000.0f;
}

//=============================================================================
// Closest Hit Shader - Full PBR with Shadows
//=============================================================================

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    // Get primitive index - for triangle list, each triangle has 3 vertices
	uint primIdx = PrimitiveIndex();
	uint vertexIndex = primIdx * 3;
    
    // Load the 3 vertices of this triangle
	Vertex v0 = LoadVertex(vertexIndex);
	Vertex v1 = LoadVertex(vertexIndex + 1);
	Vertex v2 = LoadVertex(vertexIndex + 2);

    // Only cast shadow if at least one vertex is marked
    // bool triangleCastsShadow = (v0.CastShadow == 1) || (v1.CastShadow == 1) || (v2.CastShadow == 1);
    // [MOVED TO ANYHIT] Only affect shadow rays, not main rendering
    /*
    if (payload.isShadowRay != 0) {
        if (!triangleCastsShadow) {
            // If triangle does not cast shadow, treat as transparent to shadow rays
            payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
            return;
        }
    }
    */
    
    // Barycentric coordinates
	float3 bary = float3(1.0f - attribs.barycentrics.x - attribs.barycentrics.y,
                         attribs.barycentrics.x,
                         attribs.barycentrics.y);
    
    // Interpolate vertex attributes
	float3 N = normalize(v0.Normal * bary.x + v1.Normal * bary.y + v2.Normal * bary.z);
	float2 texCoord = v0.TexC * bary.x + v1.TexC * bary.y + v2.TexC * bary.z;
	float3 T = normalize(v0.TangentU * bary.x + v1.TangentU * bary.y + v2.TangentU * bary.z);
    
    // Hit position and ray direction
	float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	float3 rayDir = WorldRayDirection();
    
    // Ensure normal faces the camera
	if (dot(N, -rayDir) < 0.0f)
		N = -N;
    
    // View direction
	float3 V = normalize(gCameraPos - hitPos);
    
    // Sample texture
	uint aliasIndex = gPrimitiveTextureIndices.Load(primIdx * 4);
	AliasData ad = gAliasData[aliasIndex];
	uint texIndex = ad.textureIndex;
	float4 materialDiffuseAlbedo = ad.diffuseAlbedo;
	float3 materialFresnelR0 = ad.fresnelR0;
    
    // --- Compute Ray Cone Mip Level ---
    // 1. Calculate texture to world area ratio
	float3 e1 = v1.Pos - v0.Pos;
	float3 e2 = v2.Pos - v0.Pos;
	float2 duv1 = v1.TexC - v0.TexC;
	float2 duv2 = v2.TexC - v0.TexC;
    
	float worldArea = length(cross(e1, e2));
	float uvArea = abs(duv1.x * duv2.y - duv1.y * duv2.x);
    // areaRatio = uv area / world area
	float areaRatio = uvArea / max(worldArea, 1e-8f);
    
    // 2. Calculate pixel footprint area on the surface
	float rayDist = RayTCurrent();
	float coneWidth = rayDist * gRayConeSpreadAngle;
	float NdotR = max(abs(dot(N, rayDir)), 0.001f);
    // footPrintArea = pixel area in world space
	float footPrintArea = (coneWidth * coneWidth) / NdotR;
    
    // 3. Convert to UV footprint area
	float uvFootprintArea = footPrintArea * areaRatio;
    
    // Normal mapping: sample and apply if this primitive has a normal map
	int normalMapIndex = ad.normalMapIndex;
	if (normalMapIndex >= 0 && normalMapIndex < 550)
	{
		uint texW, texH;
		gTextures[NonUniformResourceIndex((uint) normalMapIndex)].GetDimensions(texW, texH);
		float texelArea = uvFootprintArea * texW * texH;
		float normalMip = max(0.5f * log2(max(texelArea, 1.0f)), 0.0f);
        
		float3 normalMapSample = gTextures[NonUniformResourceIndex((uint) normalMapIndex)].SampleLevel(gSampler, texCoord, normalMip).rgb;
		N = normalize(NormalSampleToWorldSpace(normalMapSample, N, T));
	}
    
	float4 texSample = float4(0.5f, 0.5f, 0.5f, 1.0f);
    
	if (texIndex < 550)
	{
		uint texW, texH;
		gTextures[NonUniformResourceIndex(texIndex)].GetDimensions(texW, texH);
		float texelArea = uvFootprintArea * texW * texH;
		float albedoMip = max(0.5f * log2(max(texelArea, 1.0f)), 0.0f);
        
		texSample = gTextures[NonUniformResourceIndex(texIndex)].SampleLevel(gSampler, texCoord, albedoMip);
	}
    
    // Skip old alpha test block - merged into IsTransparentTexture handling below
    
	float3 albedo;
	if (texIndex < 550)
	{
		albedo = texSample.rgb * materialDiffuseAlbedo.rgb;
	}
	else
	{
		float variation = frac(sin(dot(texCoord, float2(12.9898f, 78.233f))) * 43758.5453f);
		albedo = lerp(float3(0.4f, 0.38f, 0.35f), float3(0.55f, 0.52f, 0.48f), variation) * materialDiffuseAlbedo.rgb;
	}
    
    // Alpha Blending for Transparent textures (torches/flames/translucent effects)
	if (IsTransparentTexture(texIndex))
	{
		float alpha = texSample.a * materialDiffuseAlbedo.a;
		const float alphaTolerance = 0.05f;
		float3 surfaceColor = albedo; // default: opaque surface color

		if (payload.depth < 4)
		{
            // Fire a continuation ray through this surface to get what's behind it
			RayDesc contRay;
			contRay.Origin = hitPos + rayDir * 0.01f;
			contRay.Direction = rayDir;
			contRay.TMin = 0.01f;
			contRay.TMax = 100000.0f;

			RayPayload contPayload;
			contPayload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
			contPayload.depth = payload.depth + 1;
			contPayload.isGIRay = payload.isGIRay;
			contPayload.hitT = 100000.0f;

			TraceRay(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, contRay, contPayload);

			if (alpha < alphaTolerance)
			{
                // Fully transparent pixel: discard this hit and return what lies behind.
				surfaceColor = contPayload.color.rgb;
			}
			else if (alpha < 0.99f)
			{
				if (IsFlameTexture(texIndex))
				{
                    // Emissive flame: return raw texture color unshaded.
					payload.color = float4(albedo, texSample.a);
					payload.hitT = RayTCurrent();
					return;
				}
				else
				{
                    // Standard linear alpha blend: src*alpha + dst*(1-alpha)
					surfaceColor = lerp(contPayload.color.rgb, surfaceColor, alpha);
				}
			}
            // alpha >= 0.99: treat as fully opaque, surfaceColor = albedo (already set)
		}
		else
		{
            // Recursion limit reached – no continuation ray available.
            // For near-transparent pixels return black; for opaque keep albedo.
			if (alpha < alphaTolerance)
				surfaceColor = float3(0.0f, 0.0f, 0.0f);
			else if (alpha < 0.99f && IsFlameTexture(texIndex))
			{
				payload.color = float4(albedo, texSample.a);
				payload.hitT = RayTCurrent();
				return;
			}
            // else surfaceColor = albedo
		}

		payload.color = float4(surfaceColor, 1.0f);
		payload.hitT = RayTCurrent();
		return;
	}
    
    // ---- Material properties ----
	float roughness = ad.roughness;
	float metallic = ad.metallic;
    
    // Wet floor effect: horizontal surfaces get slight glossiness
	float floorFactor = saturate(dot(N, float3(0.0f, 1.0f, 0.0f)));
	if (floorFactor > 0.7f)
	{
		float wetness = (floorFactor - 0.7f) / 0.3f; // 0 to 1
		roughness = lerp(roughness, roughness * 0.5f, wetness * 0.6f);
        // Darken wet albedo slightly (wet surfaces absorb more light)
		albedo *= lerp(1.0f, 0.85f, wetness * 0.4f);
	}
    
    // ---- Lighting accumulation ----
	float3 color = float3(0.0f, 0.0f, 0.0f);
    
    // Ambient: scaled down for dramatic contrast, with hemisphere variation
    // Surfaces facing up get slightly more ambient (indirect sky bounce)
    // Surfaces facing down (ceilings) get less
	float hemiBlend = dot(N, float3(0.0f, 1.0f, 0.0f)) * 0.5f + 0.5f;
	float ambientScale = lerp(0.08f, 0.18f, hemiBlend);
	float3 ambient = gAmbientLight.rgb * ambientScale * albedo;
    // Cool tint in ambient shadows
	ambient *= float3(0.85f, 0.9f, 1.0f);
	color += ambient;
    
    // Shadow ray origin: offset along normal to prevent self-intersection
	float3 shadowOrigin = hitPos + N * SHADOW_BIAS;
    
    // ---- Directional light (gLights[0]) with shadow ----
	if (gNumLights > 0)
	{
		Light L = gLights[0];
		float3 lightDir = -normalize(L.Direction);
		float NdotL = max(dot(N, lightDir), 0.0f);
        
		if (NdotL > 0.001f)
		{
			float shadow = 1.0f;
			shadow = TraceShadowRay(shadowOrigin, lightDir, 10000.0f);
			color += ComputeDirectionalLight(L, albedo, materialFresnelR0, N, V, roughness, metallic) * shadow;
		}
	}
    
    // ---- Point lights (torches + missiles) with shadows ----
	for (uint i = 1; i < min(gNumLights, (uint) MaxLights); ++i)
	{
		Light L = gLights[i];
		float3 lightVec = L.Position - hitPos;
		float d = length(lightVec);
        
		if (d < L.FalloffEnd && d > 0.01f)
		{
			float3 lightDir = lightVec / d;
			float NdotL = max(dot(N, lightDir), 0.0f);
            
			if (NdotL > 0.001f)
			{
                // Shadow ray for nearby lights (skip distant ones for performance)
				float shadow = 1.0f;
				if (i <= MAX_SHADOW_LIGHTS)
				{
					shadow = TraceShadowRay(shadowOrigin, lightDir, d);
				}
                // Spot light logic
				if (L.SpotPower > 0.0f)
				{
					color += ComputeSpotLight(L, hitPos, albedo, materialFresnelR0, N, V, roughness, metallic) * shadow;
				}
				else
				{
					color += ComputePointLight(L, hitPos, albedo, materialFresnelR0, N, V, roughness, metallic, (float) i) * shadow;
				}
			}
		}
	}
    
    // ---- Single-bounce Global Illumination ----
    // Only trace GI ray for primary (camera) rays to avoid runaway recursion.
	if (!payload.isGIRay)
	{
#if PATH_TRACING_ENABLED
        // ============ Full Path Tracing ============
        // Initialize random seed from hit position and time for temporal variance
        uint2 pixelIdx = DispatchRaysIndex().xy;
        uint frameHash = asuint(gTotalTime * 1000.0f);
        uint pathSeed = InitRandomSeed(pixelIdx, frameHash);
        pathSeed = PCGHash(pathSeed + asuint(hitPos.x + hitPos.y * 137.0f + hitPos.z * 59.0f));
        
        // Trace path with multiple bounces
        float3 pathRadiance = TracePathSegment(hitPos, N, V, albedo, materialFresnelR0, 
                                               roughness, metallic, pathSeed, 
                                               float3(1.0f, 1.0f, 1.0f));
        
        // Clamp and add path tracing contribution
        pathRadiance = ClampFireflies(pathRadiance);
        color += pathRadiance * GI_BOUNCE_STRENGTH;
#else
        // ============ Legacy Single-Bounce GI ============
        // --- High-quality 3D hash seeded with time for temporal jitter ---
        // Each frame picks a different hemisphere direction so successive frames
        // average out to smooth, near-noiseless GI without extra samples.
		float3 p = hitPos * 0.1731f + float3(gTotalTime * 0.037f, gTotalTime * 0.053f, gTotalTime * 0.019f);
		float3 p3 = frac(p * float3(443.8975f, 397.2973f, 371.3141f));
		p3 += dot(p3, p3.yzx + 31.432f);
		float seed1 = frac((p3.x + p3.y) * p3.z);
		float seed2 = frac((p3.x + p3.z) * p3.y);

        // Cosine-weighted hemisphere sample (Malley's method)
		float phi = 2.0f * PI * seed1;
		float cosTheta = sqrt(seed2); // cosine-weighted: pdf = cosθ/π cancels with Lambert
		float sinTheta = sqrt(1.0f - seed2);

        // Build orthonormal TBN frame around N
		float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
		float3 giT = normalize(cross(up, N));
		float3 giB = cross(N, giT);
		float3 giDir = normalize(sinTheta * cos(phi) * giT +
                                  sinTheta * sin(phi) * giB +
                                  cosTheta * N);

		RayPayload giPayload;
		giPayload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
		giPayload.depth = 4; // Prevent GI rays from spawning transparency continuations
		giPayload.isGIRay = true;
		giPayload.hitT = 100000.0f;

		RayDesc giRay;
		giRay.Origin = hitPos + N * SHADOW_BIAS;
		giRay.Direction = giDir;
		giRay.TMin = 0.05f;
		giRay.TMax = GI_MAX_DIST;

		TraceRay(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, giRay, giPayload);

        // Clamp fireflies: bright direct-lit secondary hits cause white spikes.
        // Cap luminance to a reasonable maximum for indirect light.
		float3 giRadiance = giPayload.color.rgb;
		float giLum = dot(giRadiance, float3(0.2126f, 0.7152f, 0.0722f));
		if (giLum > 2.0f)
			giRadiance *= 2.0f / giLum;

        // cosine-weighted sampling: Monte-Carlo weight (cosθ/π) and pdf (cosθ/π) cancel,
        // leaving just the radiance scaled by albedo and the GI strength knob.
		float3 giColor = giRadiance * albedo * GI_BOUNCE_STRENGTH;
		color += giColor;
#endif
	}

    // ---- Atmospheric distance fog ----
	float dist = length(hitPos - gCameraPos);
	float fogFactor = 1.0f - exp(-dist * FOG_DENSITY);
	fogFactor = saturate(fogFactor);
    // Fog color: dark blue-gray with slight warmth from nearby torches
	float3 fogColor = float3(0.015f, 0.015f, 0.025f);
	color = lerp(color, fogColor, fogFactor);
    
    // Minimum visibility so geometry silhouettes are faintly visible
	color = max(color, float3(0.012f, 0.01f, 0.008f));
    
	payload.color = float4(color, 1.0f);
	payload.hitT = RayTCurrent();
}
