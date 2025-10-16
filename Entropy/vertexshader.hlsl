#include "view.hlsli"
#include "frame.hlsli"

cbuffer scope_alk_debugshape : register(b0) {
    float4x4 local_to_world;
    float4 color;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput VSMain(float3 in_position : POSITION) {
    VSOutput output;

    output.position = mul(world_to_projective, mul(local_to_world, float4(in_position, 1.0)));

    return output;
}
