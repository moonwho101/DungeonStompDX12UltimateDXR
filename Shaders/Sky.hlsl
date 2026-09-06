//=============================================================================
// Sky.fx by Frank Luna (C) 2011 All Rights Reserved.
// Modified by Mark Longo 2020
//=============================================================================

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};
 
VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Use local vertex position as cubemap lookup vector.
	vout.PosL = vin.PosL;
	
	// Transform local vertex position by world matrix (w=0.0 ignores translation).
	float4 posW = mul(float4(vin.PosL, 0.0f), gWorld);

	// Multiply by View matrix with w=0.0f to apply camera rotation ONLY,
	// avoiding adding and subtracting gEyePosW in float32 which causes jitter at large coordinates.
	float4 posV = mul(float4(posW.xyz, 0.0f), gView);

	// Project view-space position to clip space and force z = w (skydome always on far plane).
	vout.PosH = mul(float4(posV.xyz, 1.0f), gProj).xyww;
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float3 dir = normalize(pin.PosL);
	return gCubeMap.Sample(gsamLinearWrap, dir);
}

