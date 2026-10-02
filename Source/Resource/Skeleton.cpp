#include "Skeleton.hpp"
#include <assimp/scene.h>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
    void Require(bool valid, const char* message) {
        if (!valid) throw std::runtime_error(message);
    }

    SkeletonMatrix CopyMatrix(const aiMatrix4x4& matrix) {
        SkeletonMatrix result{ { matrix.a1, matrix.a2, matrix.a3, matrix.a4,
            matrix.b1, matrix.b2, matrix.b3, matrix.b4,
            matrix.c1, matrix.c2, matrix.c3, matrix.c4,
            matrix.d1, matrix.d2, matrix.d3, matrix.d4 } };
        for (float value : result.elements) Require(std::isfinite(value), "Non-finite skeleton matrix");
        return result;
    }

    using NodeLookup = std::unordered_map<std::string, std::size_t>;

    NodeLookup MakeLookup(const Skeleton& skeleton) {
        NodeLookup lookup;
        for (std::size_t i = 0; i < skeleton.nodes.size(); ++i) {
            auto inserted = lookup.emplace(skeleton.nodes[i].name, i);
            if (!inserted.second) inserted.first->second = InvalidSkeletonNode;
        }
        return lookup;
    }

    std::size_t ResolveNode(const NodeLookup& lookup, const std::string& name) {
        auto found = lookup.find(name);
        if (found == lookup.end()) throw std::runtime_error("Missing skeleton node: " + name);
        if (found->second == InvalidSkeletonNode)
            throw std::runtime_error("Ambiguous skeleton node: " + name);
        return found->second;
    }
}

Skeleton LoadSkeleton(const aiScene& scene) {
    Require(scene.mRootNode != nullptr, "Missing skeleton root");
    Require(scene.mNumMeshes == 0 || scene.mMeshes != nullptr, "Missing mesh array");
    Skeleton skeleton;
    skeleton.meshes.resize(scene.mNumMeshes);
    std::vector<std::pair<const aiNode*, std::size_t>> pending;
    pending.emplace_back(scene.mRootNode, InvalidSkeletonNode);
    std::unordered_set<const aiNode*> visited;
    while (!pending.empty()) {
        const auto entry = pending.back();
        pending.pop_back();
        Require(entry.first != nullptr, "Null skeleton node");
        Require(visited.insert(entry.first).second, "Repeated/cyclic skeleton node");
        const aiNode& source = *entry.first;
        Require(source.mNumChildren == 0 || source.mChildren != nullptr, "Missing child array");
        Require(source.mNumMeshes == 0 || source.mMeshes != nullptr, "Missing node mesh array");
        const std::size_t nodeIndex = skeleton.nodes.size();
        SkeletonNode node;
        node.name = source.mName.C_Str();
        node.parentIndex = entry.second;
        node.localBindTransform = CopyMatrix(source.mTransformation);
        for (unsigned int m = 0; m < source.mNumMeshes; ++m) {
            const unsigned int meshIndex = source.mMeshes[m];
            Require(meshIndex < scene.mNumMeshes, "Invalid node mesh index");
            node.meshIndices.push_back(meshIndex);
            skeleton.meshes[meshIndex].nodeIndices.push_back(nodeIndex);
        }
        skeleton.nodes.push_back(std::move(node));
        for (unsigned int c = source.mNumChildren; c > 0; --c)
            pending.emplace_back(source.mChildren[c - 1], nodeIndex);
    }

    const NodeLookup lookup = MakeLookup(skeleton);
    for (unsigned int m = 0; m < scene.mNumMeshes; ++m) {
        Require(scene.mMeshes[m] != nullptr, "Null mesh");
        const aiMesh& source = *scene.mMeshes[m];
        Require(source.mNumBones == 0 || source.mBones != nullptr, "Missing bone array");
        MeshSkin& mesh = skeleton.meshes[m];
        mesh.vertexInfluences.resize(source.mNumVertices);
        mesh.bones.reserve(source.mNumBones);
        for (unsigned int b = 0; b < source.mNumBones; ++b) {
            Require(source.mBones[b] != nullptr, "Null bone");
            const aiBone& sourceBone = *source.mBones[b];
            Require(sourceBone.mNumWeights == 0 || sourceBone.mWeights != nullptr, "Missing weights");
            SkeletonBone bone;
            bone.name = sourceBone.mName.C_Str();
            bone.nodeIndex = ResolveNode(lookup, bone.name);
            bone.offsetMatrix = CopyMatrix(sourceBone.mOffsetMatrix);
            mesh.bones.push_back(std::move(bone));
            for (unsigned int w = 0; w < sourceBone.mNumWeights; ++w) {
                const aiVertexWeight& influence = sourceBone.mWeights[w];
                Require(influence.mVertexId < source.mNumVertices, "Invalid bone vertex index");
                Require(std::isfinite(influence.mWeight) && influence.mWeight >= 0.0f,
                    "Invalid bone weight");
                if (influence.mWeight > 0.0f)
                    mesh.vertexInfluences[influence.mVertexId].push_back({ b, influence.mWeight });
            }
        }
    }
    return skeleton;
}

AnimationNodeBindings BindAnimationChannels(
    const Skeleton& skeleton, const std::vector<AnimationClip>& clips) {
    const NodeLookup lookup = MakeLookup(skeleton);
    AnimationNodeBindings bindings;
    bindings.reserve(clips.size());
    for (const AnimationClip& clip : clips) {
        std::vector<std::size_t> nodes;
        std::unordered_set<std::size_t> boundNodes;
        nodes.reserve(clip.channels.size());
        for (const AnimationChannel& channel : clip.channels) {
            const std::size_t node = ResolveNode(lookup, channel.nodeName);
            Require(boundNodes.insert(node).second, "Multiple channels target the same node");
            nodes.push_back(node);
        }
        bindings.push_back(std::move(nodes));
    }
    return bindings;
}
