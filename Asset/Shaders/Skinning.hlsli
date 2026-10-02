struct SkinMatrix {
    row_major float4x4 transform;
};
StructuredBuffer<SkinMatrix> skinMatrices : register(t0);

float4 SkinPosition(float3 position, uint4 indices, float4 weights) {
    float4 result = 0;
    [unroll]
    for (uint i = 0; i < 4; ++i) {
        if (weights[i] > 0)
            result += weights[i] * mul(skinMatrices[indices[i]].transform, float4(position, 1));
    }
    return result;
}
