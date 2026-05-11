cbuffer TransformBuffer : register(b0) {
    matrix transform;
};

Texture2D objTexture : register(t0);
SamplerState objSampler : register(s0);

struct VS_IN {
    float3 pos : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct PS_IN {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

// 정점 셰이더 (Vertex Shader)
PS_IN VSMain(VS_IN input) {
    PS_IN output;
    output.pos = mul(float4(input.pos, 1.0f), transform);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

// 픽셀 셰이더 (Pixel Shader)
float4 PSMain(PS_IN input) : SV_TARGET{
    //  색상과 투명도 뽑아오기
    float4 texColor = objTexture.Sample(objSampler, input.uv);

    clip(texColor.a - 0.1f);

    texColor.a = 1.0f;

    return texColor;

    //알파 블렌딩 꼭 넣어야함 ㅁㅊ
}