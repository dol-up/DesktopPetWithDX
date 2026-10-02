#include "Skinning.hlsli"

cbuffer TransformBuffer : register(b0) {
    matrix transform;
};

cbuffer MaterialBuffer : register(b0) {
    int hasTexture;
    float3 padding;
};

Texture2D objTexture : register(t0);
SamplerState objSampler : register(s0);

struct VS_IN {
    float3 pos : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    uint4 boneIndices : BLENDINDICES0;
    float4 boneWeights : BLENDWEIGHT0;
};

struct PS_IN {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

// 정점 셰이더 (Vertex Shader)
PS_IN VSMain(VS_IN input) {
    PS_IN output;
    output.pos = mul(SkinPosition(input.pos, input.boneIndices, input.boneWeights), transform);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

// 픽셀 셰이더 (Pixel Shader)
float4 PSMain(PS_IN input) : SV_TARGET
{
    float4 texColor;

    if (hasTexture == 1) {
        texColor = objTexture.Sample(objSampler, input.uv);

        clip(texColor.a - 0.1f);
        texColor.rgb *= texColor.a;

        return texColor;
    } else {
        texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    return texColor;
}
