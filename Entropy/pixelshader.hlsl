Texture2D    tex0  : register(t0);
SamplerState samp1 : register(s1); // bind sampler to s1 on CPU

struct PS_INPUT
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PS_INPUT i) : SV_Target
{
    return tex0.Sample(samp1, i.uv);
}
