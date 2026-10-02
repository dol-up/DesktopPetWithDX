#include "Skinning.hpp"
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
    unsigned int CheckedSize(std::size_t size) {
        if (size > (std::numeric_limits<unsigned int>::max)())
            throw std::runtime_error("Model exceeds 32-bit geometry limits");
        return static_cast<unsigned int>(size);
    }
    aiMatrix4x4 ToMatrix(const SkeletonMatrix& matrix) {
        const auto& v = matrix.elements;
        return aiMatrix4x4(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
            v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]);
    }
    SkeletonMatrix FromMatrix(const aiMatrix4x4& m) {
        return { { m.a1, m.a2, m.a3, m.a4, m.b1, m.b2, m.b3, m.b4,
            m.c1, m.c2, m.c3, m.c4, m.d1, m.d2, m.d3, m.d4 } };
    }
}

SkinningGeometry BuildSkinningGeometry(const aiScene& scene, const Skeleton& skeleton) {
    if (skeleton.meshes.size() != scene.mNumMeshes) throw std::runtime_error("Mesh/skeleton mismatch");
    SkinningGeometry result;
    for (unsigned int m = 0; m < scene.mNumMeshes; ++m) {
        const aiMesh& source = *scene.mMeshes[m];
        const auto& mesh = skeleton.meshes[m];
        for (auto node : mesh.nodeIndices) {
            SkinningDraw draw{};
            draw.startIndex = CheckedSize(result.indices.size());
            draw.materialIndex = source.mMaterialIndex;
            draw.meshIndex = m;
            draw.nodeIndex = node;
            draw.paletteOffset = result.paletteSize;
            result.paletteSize = CheckedSize(static_cast<std::size_t>(result.paletteSize) + mesh.bones.size() + 1);
            const auto baseVertex = CheckedSize(result.vertices.size());
            CheckedSize(result.vertices.size() + source.mNumVertices);
            for (unsigned int v = 0; v < source.mNumVertices; ++v) {
                Vertex vertex{};
                vertex.x = source.mVertices[v].x;
                vertex.y = source.mVertices[v].y;
                vertex.z = source.mVertices[v].z;
                vertex.r = vertex.g = vertex.b = vertex.a = 1;
                if (source.HasTextureCoords(0)) {
                    vertex.u = source.mTextureCoords[0][v].x;
                    vertex.v = source.mTextureCoords[0][v].y;
                }
                // Merge any duplicate bone entries before selecting the largest four.
                std::vector<BoneInfluence> influences;
                for (const auto& influence : mesh.vertexInfluences[v]) {
                    auto same = std::find_if(influences.begin(), influences.end(), [&](const BoneInfluence& entry) {
                        return entry.boneIndex == influence.boneIndex;
                    });
                    if (same == influences.end()) influences.push_back(influence);
                    else same->weight += influence.weight;
                }
                std::stable_sort(influences.begin(), influences.end(), [](const BoneInfluence& a, const BoneInfluence& b) {
                    return a.weight > b.weight;
                });
                if (influences.size() > 4) influences.resize(4);
                double sum = 0;
                for (const auto& influence : influences) sum += influence.weight;
                if (!std::isfinite(sum)) throw std::runtime_error("Invalid combined skin weight");
                if (sum > 0) {
                    for (std::size_t i = 0; i < influences.size(); ++i) {
                        vertex.boneIndices[i] = draw.paletteOffset + influences[i].boneIndex;
                        vertex.boneWeights[i] = static_cast<float>(influences[i].weight / sum);
                    }
                } else {
                    // Rigid and unweighted vertices follow the mesh node, not bone zero.
                    vertex.boneIndices[0] = draw.paletteOffset + CheckedSize(mesh.bones.size());
                    vertex.boneWeights[0] = 1;
                }
                result.vertices.push_back(vertex);
            }
            for (unsigned int f = 0; f < source.mNumFaces; ++f) {
                const auto& face = source.mFaces[f];
                if (face.mNumIndices != 3) continue; // Only triangle primitives are rendered/picked.
                for (unsigned int i = 0; i < 3; ++i) {
                    if (face.mIndices[i] >= source.mNumVertices) throw std::runtime_error("Invalid triangle index");
                    result.indices.push_back(baseVertex + face.mIndices[i]);
                }
            }
            draw.indexCount = CheckedSize(result.indices.size() - draw.startIndex);
            result.draws.push_back(draw);
        }
    }
    return result;
}

std::vector<SkeletonMatrix> BuildSkinningPalette(const Skeleton& skeleton,
    const AnimationPose& pose, const SkinningGeometry& geometry) {
    std::vector<SkeletonMatrix> palette(geometry.paletteSize);
    for (const auto& draw : geometry.draws) {
        const auto& mesh = skeleton.meshes[draw.meshIndex];
        aiMatrix4x4 instanceCorrection;
        if (!mesh.bones.empty() && draw.nodeIndex != mesh.nodeIndices.front()) {
            auto reference = ToMatrix(pose.globalTransforms[mesh.nodeIndices.front()]);
            if (std::abs(reference.Determinant()) < 1e-12f)
                throw std::runtime_error("Singular skinned mesh instance transform");
            instanceCorrection = ToMatrix(pose.globalTransforms[draw.nodeIndex]) * reference.Inverse();
        }
        for (std::size_t b = 0; b < mesh.bones.size(); ++b)
            palette[draw.paletteOffset + b] = FromMatrix(instanceCorrection * ToMatrix(pose.boneTransforms[draw.meshIndex][b]));
        palette[draw.paletteOffset + mesh.bones.size()] = pose.globalTransforms[draw.nodeIndex];
    }
    return palette;
}

std::vector<DirectX::XMFLOAT3> SkinVertices(const SkinningGeometry& geometry,
    const std::vector<SkeletonMatrix>& palette) {
    std::vector<DirectX::XMFLOAT3> positions;
    positions.reserve(geometry.vertices.size());
    for (const auto& vertex : geometry.vertices) {
        DirectX::XMFLOAT3 position{};
        for (unsigned int i = 0; i < 4; ++i) {
            const float w = vertex.boneWeights[i];
            if (w == 0) continue;
            const auto& m = palette.at(vertex.boneIndices[i]).elements;
            position.x += w * (m[0] * vertex.x + m[1] * vertex.y + m[2] * vertex.z + m[3]);
            position.y += w * (m[4] * vertex.x + m[5] * vertex.y + m[6] * vertex.z + m[7]);
            position.z += w * (m[8] * vertex.x + m[9] * vertex.y + m[10] * vertex.z + m[11]);
        }
        positions.push_back(position);
    }
    return positions;
}
