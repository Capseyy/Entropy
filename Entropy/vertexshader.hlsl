//=============================
// b1: header + instance rows
//=============================
cbuffer cb1 : register(b1)
{
    // cb1[0]
    float4 cb1_header0;          // meshOffset_meshScale: xyz = offset, w = scale
    // cb1[1]
    float4 cb1_header1;          // uvScale_uvOffset: x = uvScaleX, y = uOff, z = vOff, w = extra/bits
    // cb1[2..] : 4 float4 rows per instance (row-major, translation in .w)
    float4 cb1_rows[4 * 63];
}

//=============================
// b12: scope_view (VP etc.)
//=============================
cbuffer scope_view : register(b12)
{
    float4x4 world_to_projective;     // VP  (you upload transpose(V*P) from C++)
    float4x4 camera_to_world;         // V^-1 (row 3 has camera position)
    float4   target;                  // (w,h,1/w,1/h)
    float4   view_miscellaneous;      // x=maxDepth, y=isFirstPerson
    float4   view_unk20;
    float4x4 camera_to_projective;    // P (you upload transpose(P))
}

// If your mesh data are Z-up, set to 1 to rotate to Y-up
#ifndef Z_UP
#define Z_UP 0
#endif

//=============================
// Vertex I/O
//=============================
struct VS_INPUT
{
    float4 pos     : POSITION;
    float4 tangent : TANGENT;     // unused here
    float2 uv      : TEXCOORD0;
    uint   instId  : SV_InstanceID;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

//=============================
// Vertex Shader (DIRECT path)
//=============================
VS_OUTPUT main(VS_INPUT i)
{
    VS_OUTPUT o;

    // Mesh-space position
    float3 p = i.pos.xyz;
#if Z_UP
    // Z-up -> Y-up (?90° about X)
    p = float3(p.x, p.z, -p.y);
#endif

    // Apply header scale/offset: local = p*scale + offset
    float3 local = p * cb1_header0.w + cb1_header0.xyz;
    float4 v     = float4(local, 1.0);

    // Fetch this instance’s 3 row vectors (row-major, .w = translation)
    uint base = 4 * min(i.instId, 62);
    float4 r0 = cb1_rows[base + 0];   // [m00 m01 m02 tx]
    float4 r1 = cb1_rows[base + 1];   // [m10 m11 m12 ty]
    float4 r2 = cb1_rows[base + 2];   // [m20 m21 m22 tz]

    // World position = rows ? local
    float3 wpos = float3(dot(r0, v), dot(r1, v), dot(r2, v));

    // Clip position
    o.pos = mul(float4(wpos, 1.0), world_to_projective);

    // UV transform (scale X, add offsets)
    o.uv = i.uv * cb1_header1.x + cb1_header1.yz;

    return o;
}

//=============================
// Minimal Pixel Shader
//=============================
float4 PSMain(VS_OUTPUT i) : SV_Target
{
    return float4(1,1,1,1);
}
