#pragma once
#include "Animator.hpp"
#include "Vertex.hpp"
#include <DirectXMath.h>

struct SkinningDraw {
    unsigned int startIndex;
    unsigned int indexCount;
    unsigned int materialIndex;
    unsigned int meshIndex;
    std::size_t nodeIndex;
    unsigned int paletteOffset;
};

struct SkinningGeometry {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<SkinningDraw> draws;
    unsigned int paletteSize = 0;
};

SkinningGeometry BuildSkinningGeometry(const aiScene& scene, const Skeleton& skeleton);
std::vector<SkeletonMatrix> BuildSkinningPalette(const Skeleton& skeleton,
    const AnimationPose& pose, const SkinningGeometry& geometry);
// Identical four-influence policy and column-vector math to the vertex shader.
std::vector<DirectX::XMFLOAT3> SkinVertices(const SkinningGeometry& geometry,
    const std::vector<SkeletonMatrix>& palette);
