// Fullscreen VS + PBR-ish directional light PS for your lighting pass.
// Assumes:
//  t0 = GBuffer Albedo  (sRGB)
//  t1 = GBuffer NormalRoughness (rgb = normal in 0..1, a = roughness 0..1)
//  t2 = Depth (bound but unused here)
//  s0 = point clamp, s1 = linear clamp (matches your binding order)

cbuffer LightCB : register(b0)
{
    float3 LightDirVS;      // direction *towards* the light, in VIEW space (normalize!)
    float  LightIntensity;  // e.g. 5.0

    float3 LightColor;      // e.g. (1,0.95,0.9)
    float  AmbientIntensity;// e.g. 0.03

    float3 AmbientColor;    // e.g. (0.5,0.55,0.6)
    float  MetalnessDefault;// fallback if albedo.a has no metalness; e.g. 0.0
};

Texture2D    gAlbedo             : register(t0);
Texture2D    gNormalRoughness    : register(t1);
Texture2D    gDepth              : register(t2); // not used in this minimal shader

SamplerState sPointClamp         : register(s0);
SamplerState sLinearClamp        : register(s1);

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

// Fullscreen triangle-strip (4 verts) VS with no inputs.
VSOut VS_Fullscreen(uint vid : SV_VertexID)
{
    VSOut o;
    // Strip order: (0,1,2,3) ? cover screen
    float2 uv = float2((vid & 1) ? 1.0 : 0.0, (vid & 2) ? 1.0 : 0.0);
    o.uv  = uv;
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

// Helpers
float3 SRGBToLinear(float3 c)
{
    // Simple pow approximation; replace with a proper curve if you prefer
    return pow(c, 2.2);
}

float3 DecodeNormal(float3 n01)
{
    float3 n = n01 * 2.0 - 1.0;
    return normalize(n);
}

// GGX / Schlick
float  D_GGX(float NdotH, float a)
{
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * d * d + 1e-7);
}

float  V_SmithGGXCorrelated(float NdotV, float NdotL, float a)
{
    float a2 = a * a;
    float gv = NdotL * sqrt(max(0.0, NdotV * (NdotV * (1.0 - a2) + a2)));
    float gl = NdotV * sqrt(max(0.0, NdotL * (NdotL * (1.0 - a2) + a2)));
    return 0.5 / max(gv + gl, 1e-6);
}

float3 F_Schlick(float3 F0, float VdotH)
{
    float Fc = pow(saturate(1.0 - VdotH), 5.0);
    return F0 + (1.0 - F0) * Fc;
}

struct PSOut
{
    float4 diffuse  : SV_Target0;
    float4 specular : SV_Target1;
    float4 combined : SV_Target2;
};

PSOut PS_GlobalLighting(VSOut i)
{
    PSOut o;

    // GBuffer fetch
    float4 albedoSRGB = gAlbedo.Sample(sLinearClamp, i.uv);
    float3 albedo     = SRGBToLinear(albedoSRGB.rgb);

    float4 nr         = gNormalRoughness.Sample(sPointClamp, i.uv);
    float3 N          = DecodeNormal(nr.rgb);
    float  roughness  = saturate(nr.a);
    float  metalness  = (albedoSRGB.a > 0.0) ? saturate(albedoSRGB.a) : saturate(MetalnessDefault);

    // Directions in VIEW space
    float3 L = normalize(LightDirVS);   // towards light
    float3 V = float3(0.0, 0.0, 1.0);   // view dir in view space (camera looks +Z)

    // Dot terms
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float3 H    = normalize(V + L);
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    // BRDF params
    float  a   = max(roughness * roughness, 0.001);
    float3 F0  = lerp(float3(0.04, 0.04, 0.04), albedo, metalness);

    float  D   = D_GGX(NdotH, a);
    float  Vg  = V_SmithGGXCorrelated(NdotV, NdotL, a);
    float3 F   = F_Schlick(F0, VdotH);

    float3 specBRDF = (D * Vg) * F;                     // Cook-Torrance (no 1/pi here)
    float3 kd       = (1.0 - F) * (1.0 - metalness);    // energy-conserving diffuse
    float3 diffBRDF = kd * (albedo / 3.14159265);       // Lambert

    float3 direct   = (diffBRDF + specBRDF) * (LightColor * LightIntensity) * NdotL;
    float3 ambient  = AmbientColor * AmbientIntensity * (albedo * (1.0 - metalness));

    float3 diffuseOut  = diffBRDF * (LightColor * LightIntensity) * NdotL + ambient;
    float3 specularOut = specBRDF * (LightColor * LightIntensity) * NdotL;

    // Outputs
    o.diffuse  = float4(diffuseOut,  0.0);
    o.specular = float4(specularOut, 0.0);
    o.combined = float4(diffuseOut + specularOut, 0.0);

    return o;
}
