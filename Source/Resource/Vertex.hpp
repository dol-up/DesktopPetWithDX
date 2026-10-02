#pragma once
#include <cstdint>

struct Vertex {
    float x, y, z;
    float r, g, b, a;
    float u, v;
    std::uint32_t boneIndices[4] = {};
    float boneWeights[4] = {};
};
static_assert(sizeof(Vertex) == 68, "Vertex must match the shader input layout");
