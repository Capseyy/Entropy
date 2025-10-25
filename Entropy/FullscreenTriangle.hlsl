struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

VSOut VS(uint id : SV_VertexID) {
    // Fullscreen triangle: (-1,-1), (3,-1), (-1,3)
    float2 p = float2((id==1)?3.0f:-1.0f, (id==2)?3.0f:-1.0f);
    VSOut o; o.pos = float4(p, 0, 1); o.uv = 0.5f * (p + 1.0f); return o;
}

float4 PS() : SV_Target { return float4(1,1,1,1); } // white