//***************************************************************************************
// MotionVectors.hlsl - Compute shader for camera-based motion vector generation
//
// Generates per-pixel motion vectors from depth buffer reprojection using the
// current and previous frame's view-projection matrices. This captures camera
// movement accurately. Per-object motion (e.g., animated monsters) is not captured
// but camera motion is the dominant source in a first-person dungeon crawler.
//
// Used by DLSS for temporal upscaling.
//***************************************************************************************

cbuffer MotionVectorCB : register(b0)
{
    float4x4 gInvViewProj;     // Current frame inverse view-projection
    float4x4 gPrevViewProj;    // Previous frame view-projection
    float    gRenderWidth;     // Render resolution width
    float    gRenderHeight;    // Render resolution height
    float    gInvRenderWidth;  // 1.0 / renderWidth
    float    gInvRenderHeight; // 1.0 / renderHeight
};

Texture2D<float> gDepthBuffer : register(t0);
RWTexture2D<float2> gMotionVectors : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)gRenderWidth || DTid.y >= (uint)gRenderHeight)
        return;

    // Sample depth at current pixel
    float depth = gDepthBuffer[DTid.xy];

    // Skip sky pixels (depth == 1.0 means no geometry)
    if (depth >= 1.0f)
    {
        gMotionVectors[DTid.xy] = float2(0.0f, 0.0f);
        return;
    }

    // Current pixel UV (center of pixel)
    float2 uv = (float2(DTid.xy) + 0.5f) * float2(gInvRenderWidth, gInvRenderHeight);

    // Convert to clip space [-1, 1]
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y; // Flip Y for DirectX convention

    // Reconstruct world position from depth
    float4 clipPos = float4(ndc, depth, 1.0f);
    float4 worldPos = mul(clipPos, gInvViewProj);
    worldPos /= worldPos.w;

    // Reproject to previous frame
    float4 prevClipPos = mul(worldPos, gPrevViewProj);
    prevClipPos /= prevClipPos.w;

    // Previous frame UV
    float2 prevUV = prevClipPos.xy * 0.5f + 0.5f;
    prevUV.y = 1.0f - prevUV.y;

    // Motion vector in pixel space (previous position - current position)
    // DLSS convention: how many pixels this surface moved from previous to current frame
    float2 motion = (prevUV - uv) * float2(gRenderWidth, gRenderHeight);

    gMotionVectors[DTid.xy] = motion;
}
