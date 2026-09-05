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

	// Transform local vertex position by gTexTransform for cubemap lookup vector.
	vout.PosL = mul(float4(vin.PosL, 1.0f), gTexTransform).xyz;
	
	// Transform to world space using gWorld.
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

	// Always center sky about camera.
	posW.xyz += gEyePosW;

	// Transform to clip space using gViewProj so gWorld scale and distance dictate clip space position.
	vout.PosH = mul(posW, gViewProj);
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}

