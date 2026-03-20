//***************************************************************************************
// Raytracing.hlsl by Mark Longo 2026 All Rights Reserved
// DirectX Raytracing Shader with PBR Lighting
// Ray generation, closest hit, and miss shaders for DXR with physically-based rendering
// Features: inline shadow rays (DXR 1.1), full Cook-Torrance PBR, ACES tone mapping,
//           atmospheric fog, wet floor reflectance, per-light flicker, 2-sample GI
//***************************************************************************************

#define MaxLights 32
#define PI 3.14159265f

// Max point lights that cast shadow rays (performance knob)
#define MAX_SHADOW_LIGHTS 12

// Fog density for dungeon atmosphere
#define FOG_DENSITY 0.0025f

// Shadow ray bias to prevent self-intersection
#define SHADOW_BIAS 0.15f

// Global illumination: single-bounce indirect diffuse
#define GI_BOUNCE_STRENGTH 0.35f   // scale of the GI contribution
#define GI_MAX_DIST        80.0f   // max distance the GI bounce ray travels

// Glare and flicker constants (for RayGen glare loop)
#define GLARE_INNER_WEIGHT      0.6f
#define GLARE_INNER_FALLOFF     0.9f
#define GLARE_OUTER_WEIGHT      0.4f
#define GLARE_OUTER_FALLOFF     6.7f
#define GLARE_THRESHOLD         0.001f
#define GLARE_SCALE             0.45f
#define FLICKER_BASE_STRENGTH   1.0f
#define FLICKER_FREQ            8.0f
#define FLICKER_AMP             0.25f

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
    float3 gPad1;
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

// Per-primitive normal map texture index buffer (-1 = no normal map)
ByteAddressBuffer gPrimitiveNormalMapIndices : register(t1, space1);

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
    uint   depth;    // recursion depth for transparency
    bool   isGIRay;  // true => secondary GI ray, skip further GI recursion
    float  hitT;     // distance to surface (used for volumetric glare limits)
};

// Hard-coded transparent texture ranges (matching CPU-side alpha skip logic)
bool IsTransparentTexture(uint texIdx)
{
    if (texIdx >= 94  && texIdx <= 101) return true;  // flames / effects
    if (texIdx >= 278 && texIdx <= 295) return true;  // portal effects
    if (texIdx >= 205 && texIdx <= 209) return true;  // flare
    if (texIdx >= 370 && texIdx <= 378) return true;  // blood splatter decals
    if (texIdx == 156)                  return true;  // wall crystal
    if (texIdx == 181)                  return true;  // potion
    return false;
}

// Hard-coded player weapon texture range (also treated as transparent for shadow rays to prevent black weapon silhouettes)
bool IsWeaponTexture(uint texIdx)
{
    if (texIdx >= 127  && texIdx <= 137) return true;  // player weapons (swords, bows, etc)
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
    float t = time + offset;
    float flicker = sin(t * freq) * 0.5f + 0.5f;
    flicker += sin(t * (freq * 2.13f)) * 0.25f + 0.25f;
    flicker += sin(t * (freq * 0.77f)) * 0.15f + 0.15f;
    flicker = saturate(flicker);
    return baseStrength * (1.0f - amp * flicker);
}

// PBR lighting for directional light
float3 ComputeDirectionalLight(Light L, float3 albedo, float3 N, float3 V, float roughness, float metallic)
{
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
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
float3 ComputePointLight(Light L, float3 pos, float3 albedo, float3 N, float3 V, float roughness, float metallic, float lightIndex)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    
    if (d > L.FalloffEnd)
        return float3(0.0f, 0.0f, 0.0f);
    
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
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
float3 ComputeSpotLight(Light L, float3 pos, float3 albedo, float3 N, float3 V, float roughness, float metallic)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    
    if (d > L.FalloffEnd)
        return float3(0.0f, 0.0f, 0.0f);
    
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
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

    RayQuery<RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_NON_OPAQUE> q;
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
    payload.color  = float4(0.0f, 0.0f, 0.0f, 1.0f);
    payload.depth  = 0;
    payload.isGIRay = false;
    payload.hitT   = 100000.0f;
    
    TraceRay(
        gScene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        0xFF,
        0,  // Hit group index
        1,  // Multiplier for geometry index
        0,  // Miss shader index
        ray,
        payload
    );
    
    float3 finalColor = payload.color.rgb;
    // Volumetric Fog Glow (Glare) around Point Lights
    float3 glare = float3(0.0f, 0.0f, 0.0f);
    // Glare constants now defined as macros above
    
    for (uint i = 1; i < min(gNumLights, (uint)MaxLights); ++i)
    {
        Light L = gLights[i];
        if (distance(L.Position, gCameraPos) > 1500.0f) continue;
        float3 lightVec = L.Position - rayOrigin;
        float tClosest = dot(lightVec, rayDirection);
        tClosest = clamp(tClosest, 0.0f, payload.hitT);
        float3 closestPoint = rayOrigin + rayDirection * tClosest;
        float distToLight = length(closestPoint - L.Position);
        // Reduce center brightness: lower inner core weight and increase falloff
        float glow = GLARE_INNER_WEIGHT * exp(-distToLight / GLARE_INNER_FALLOFF) + GLARE_OUTER_WEIGHT * exp(-distToLight / GLARE_OUTER_FALLOFF);
        if (glow > GLARE_THRESHOLD)
        {
            float flicker = TorchFlicker(FLICKER_BASE_STRENGTH, gTotalTime, FLICKER_FREQ, FLICKER_AMP, (float)i);
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
    
    // Sample the sunset cube map for the sky
    // We use SampleLevel with LOD 0 for the sharpest background
    float3 skyColor = gCubeMap.SampleLevel(gSampler, rayDir, 0).rgb;
    
    // Apply atmospheric dungeon void vertical gradient to darken the lower part 
    // of the cube if it's too bright for a dungeon, but keep most of it.
    float upFactor = saturate(rayDir.y * 0.5f + 0.5f);
    skyColor *= lerp(0.8f, 1.0f, upFactor);
    
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
    uint texIndex = gPrimitiveTextureIndices.Load(primIdx * 4);
    
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
    int normalMapIndex = asint(gPrimitiveNormalMapIndices.Load(primIdx * 4));
    if (normalMapIndex >= 0 && normalMapIndex < 550)
    {
        uint texW, texH;
        gTextures[NonUniformResourceIndex((uint)normalMapIndex)].GetDimensions(texW, texH);
        float texelArea = uvFootprintArea * texW * texH;
        float normalMip = max(0.5f * log2(max(texelArea, 1.0f)), 0.0f);
        
        float3 normalMapSample = gTextures[NonUniformResourceIndex((uint)normalMapIndex)].SampleLevel(gSampler, texCoord, normalMip).rgb;
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
        albedo = texSample.rgb;
    }
    else
    {
        float variation = frac(sin(dot(texCoord, float2(12.9898f, 78.233f))) * 43758.5453f);
        albedo = lerp(float3(0.4f, 0.38f, 0.35f), float3(0.55f, 0.52f, 0.48f), variation);
    }
    
    // Alpha Blending for Transparent textures (torches/flames/translucent effects)
    if (IsTransparentTexture(texIndex))
    {
        float alpha = texSample.a;
        const float alphaTolerance = 0.05f;
        float3 surfaceColor = albedo * 1.0f; // Boost brightness for transparent effects (fire, flares, etc)
        
        if (payload.depth < 4)
        {
            // Fire a continuation ray through this surface to get what's behind it
            RayDesc contRay;
            contRay.Origin = hitPos + rayDir * 0.01f;
            contRay.Direction = rayDir;
            contRay.TMin = 0.01f;
            contRay.TMax = 100000.0f;
            
            RayPayload contPayload;
            contPayload.color       = float4(0.0f, 0.0f, 0.0f, 1.0f);
            contPayload.depth       = payload.depth + 1;
            contPayload.isGIRay     = payload.isGIRay;
            contPayload.hitT        = 100000.0f;
            
            TraceRay(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, contRay, contPayload);
            
            if (alpha < alphaTolerance)
            {
                // Alpha Test: discard this hit part and return what's behind it
                surfaceColor = contPayload.color.rgb;
            }
            else if (alpha < 0.99f)
            {
                // Standard linear alpha blending: src * alpha + dst * (1 - alpha)
                surfaceColor = lerp(contPayload.color.rgb, surfaceColor, alpha);
            }
        }
        
        payload.color = float4(surfaceColor, 1.0f);
        payload.hitT = RayTCurrent();
        return;
    }
    
    // ---- Material properties ----
    float roughness = gRoughness;
    float metallic = gMetallic;
    
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
            color += ComputeDirectionalLight(L, albedo, N, V, roughness, metallic) * shadow;
        }
    }
    
    // ---- Point lights (torches + missiles) with shadows ----
    for (uint i = 1; i < min(gNumLights, (uint)MaxLights); ++i)
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
                //if (i <= MAX_SHADOW_LIGHTS)
                //{
                   shadow = TraceShadowRay(shadowOrigin, lightDir, d);
                //}
                // Spot light logic
                if (L.SpotPower > 0.0f)
                {
                    color += ComputeSpotLight(L, hitPos, albedo, N, V, roughness, metallic) * shadow;
                }
                else
                {
                    color += ComputePointLight(L, hitPos, albedo, N, V, roughness, metallic, (float)i) * shadow;
                }
            }
        }
    }
    
    // ---- Single-bounce Global Illumination (2 Samples) ----
    // Only trace GI ray for primary (camera) rays to avoid runaway recursion.
    if (!payload.isGIRay)
    {
        float3 giAccum = float3(0.0f, 0.0f, 0.0f);
        const uint GI_SAMPLES = 1;

        for (uint s = 0; s < GI_SAMPLES; ++s)
        {
            // Build a cosine-weighted random direction in the hemisphere around N.
            // We use the hit position and loop index as a cheap deterministic seed.
            float seed1 = frac(sin(dot(hitPos.xy + hitPos.z, float2(12.9898f + (float)s * 1.618f, 78.233f))) * 43758.5453f);
            float seed2 = frac(sin(dot(hitPos.yz + hitPos.x, float2(26.9511f + (float)s * 1.618f, 18.337f))) * 43758.5453f);

            // Map to cosine-weighted hemisphere sample
            float phi      = 2.0f * PI * seed1;
            float cosTheta = sqrt(seed2);
            float sinTheta = sqrt(1.0f - seed2);

            // Build a local TBN frame aligned with N so the sample points into the hemisphere
            float3 up   = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
            float3 giT  = normalize(cross(up, N));
            float3 giB  = cross(N, giT);
            float3 giDir = normalize(sinTheta * cos(phi) * giT +
                                      sinTheta * sin(phi) * giB +
                                      cosTheta             * N);

            RayPayload giPayload;
            giPayload.color       = float4(0.0f, 0.0f, 0.0f, 1.0f);
            giPayload.depth       = 4; // Prevent GI rays from spawning transparency continuations (depth < 4 check)
            giPayload.isGIRay     = true;
            giPayload.hitT        = 100000.0f;

            RayDesc giRay;
            giRay.Origin    = hitPos + N * SHADOW_BIAS;
            giRay.Direction = giDir;
            giRay.TMin      = 0.05f;
            giRay.TMax      = GI_MAX_DIST;

            TraceRay(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, giRay, giPayload);

            // The returned color is the lit radiance of the secondary surface.
            // Weight by cosTheta (Lambert) — already baked in via cosine-weighted sampling so
            // Monte-Carlo weight cancels and we just scale by the strength knob * albedo tint.
            giAccum += giPayload.color.rgb;
        }

        float3 giColor = (giAccum / (float)GI_SAMPLES) * albedo * GI_BOUNCE_STRENGTH;
        color += giColor;
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
