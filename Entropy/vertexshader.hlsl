cbuffer mycBuffer : register(b0)
{
    float4x4 mat;
};

struct VS_INPUT
{
	float4 pos : POSITION;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 outPosition : SV_POSITION;
    float2 outTexCoord : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 p = float4(input.pos.xyz, 1.0);
    output.outPosition = mul(mat, p);
    output.outTexCoord = input.uv;
    return output;
};
