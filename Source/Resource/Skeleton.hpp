#pragma once
#include "AnimationClip.hpp"
#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

constexpr std::size_t InvalidSkeletonNode = std::numeric_limits<std::size_t>::max();

// Assimp row-major storage with column-vector semantics, translation at 3/7/11.
// No DirectX transpose or model normalization has been applied.
struct SkeletonMatrix {
    std::array<float, 16> elements;
};

struct SkeletonNode {
    std::string name;
    std::size_t parentIndex = InvalidSkeletonNode;
    SkeletonMatrix localBindTransform;
    std::vector<unsigned int> meshIndices;
};

struct SkeletonBone {
    std::string name;
    std::size_t nodeIndex = InvalidSkeletonNode;
    // Mesh-local position -> bone-local bind space. Retained separately per mesh.
    SkeletonMatrix offsetMatrix;
};

struct BoneInfluence {
    unsigned int boneIndex; // Index into this mesh's bones, not Skeleton::nodes.
    float weight;
};

struct MeshSkin {
    std::vector<std::size_t> nodeIndices; // All instances of this mesh in the hierarchy.
    std::vector<SkeletonBone> bones;
    // Mesh-local vertex index. Preserve all positive source weights without truncation.
    // Empty means unweighted; do not implicitly attach it to bone 0.
    std::vector<std::vector<BoneInfluence>> vertexInfluences;
};

struct Skeleton {
    // Parent before child; includes non-bone nodes needed for transform inheritance.
    std::vector<SkeletonNode> nodes;
    std::vector<MeshSkin> meshes; // Same order as aiScene::mMeshes / Model submeshes.
};

using AnimationNodeBindings = std::vector<std::vector<std::size_t>>;

// Owns all returned data; does not retain Assimp pointers. Invalid data throws.
Skeleton LoadSkeleton(const aiScene& scene);
// [clip index][channel index] -> node index. Missing/ambiguous names throw.
AnimationNodeBindings BindAnimationChannels(
    const Skeleton& skeleton, const std::vector<AnimationClip>& clips);
