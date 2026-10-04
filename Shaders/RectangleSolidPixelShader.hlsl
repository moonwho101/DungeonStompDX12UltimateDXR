// Solid color pixel shader for untextured HUD rectangles (health bar)

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
	float2 texCoord : TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return input.color;
}
