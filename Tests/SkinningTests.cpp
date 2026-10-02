#include "Skinning.hpp"
#include "ModelPicker.hpp"
#include <assimp/scene.h>
#include <cmath>

void Check(bool condition, const char* message);
void TestGpuSkinning(const SkinningGeometry&, const std::vector<SkeletonMatrix>&);
namespace {
    SkeletonMatrix Identity() { return { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } }; }
}

void TestSkinning() {
    aiScene scene;
    scene.mRootNode = new aiNode("Root");
    auto& root = *scene.mRootNode;
    root.mNumChildren = 5;
    root.mChildren = new aiNode*[5]{};
    root.mNumMeshes = 1;
    root.mMeshes = new unsigned int[1]{ 0 };
    scene.mNumMeshes = 1;
    scene.mMeshes = new aiMesh*[1]{ new aiMesh };
    auto& mesh = *scene.mMeshes[0];
    mesh.mNumVertices = 65537;
    mesh.mVertices = new aiVector3D[mesh.mNumVertices];
    mesh.mNumFaces = 1;
    mesh.mFaces = new aiFace[1];
    mesh.mFaces[0].mNumIndices = 3;
    mesh.mFaces[0].mIndices = new unsigned int[3]{ 65534, 65535, 65536 };
    mesh.mNumBones = 5;
    mesh.mBones = new aiBone*[5]{};
    for (unsigned int b = 0; b < 5; ++b) {
        root.mChildren[b] = new aiNode("Bone" + std::to_string(b));
        root.mChildren[b]->mParent = &root;
        auto* bone = mesh.mBones[b] = new aiBone;
        bone->mName.Set("Bone" + std::to_string(b));
        bone->mNumWeights = 1;
        bone->mWeights = new aiVertexWeight[1]{ aiVertexWeight(0, (b + 1) * 0.1f) };
    }
    // Additional rigid instance of the mesh.
    root.mChildren[0]->mNumMeshes = 1;
    root.mChildren[0]->mMeshes = new unsigned int[1]{ 0 };
    root.mChildren[0]->mTransformation.a4 = 2;
    auto skeleton = LoadSkeleton(scene);
    auto geometry = BuildSkinningGeometry(scene, skeleton);
    Check(geometry.indices[2] == 65536 && geometry.draws.size() == 2, "32-bit indices and mesh instances");
    const auto& vertex = geometry.vertices[0];
    Check(vertex.boneIndices[0] == 4 && vertex.boneIndices[3] == 1, "largest four influences");
    Check(std::abs(vertex.boneWeights[0] - 0.5f / 1.4f) < 0.00001f, "normalized weights");
    Check(geometry.vertices[1].boneIndices[0] == 5 && geometry.vertices[1].boneWeights[0] == 1,
        "unweighted fallback matrix");
    const std::vector<AnimationClip> clips;
    const AnimationNodeBindings bindings;
    Animator animator(skeleton, clips, bindings);
    auto palette = BuildSkinningPalette(skeleton, animator.GetPose(), geometry);
    auto skinned = SkinVertices(geometry, palette);
    Check(skinned[1].x == 0 && skinned[65538].x == 2, "rigid instance transforms");
    TestGpuSkinning(geometry, palette);

    SkinningGeometry triangle;
    for (auto position : { AnimationVector3{ -0.15f, -0.15f, 0.5f },
        AnimationVector3{ 0.15f, -0.15f, 0.5f }, AnimationVector3{ 0, 0.15f, 0.5f } }) {
        Vertex v{};
        v.x = position.x; v.y = position.y; v.z = position.z;
        v.boneWeights[0] = 1; v.boneIndices[0] = 300;
        triangle.vertices.push_back(v);
    }
    triangle.indices = { 0, 1, 2 };
    std::vector<SkeletonMatrix> manyMatrices(301, Identity());
    const auto bindVertices = SkinVertices(triangle, manyMatrices);
    manyMatrices[300].elements[3] = 0.5f;
    const auto animatedVertices = SkinVertices(triangle, manyMatrices);
    TestGpuSkinning(triangle, manyMatrices);
    const auto identity = DirectX::XMMatrixIdentity();
    Check(ModelPicker::HitTest(75, 50, 100, 100, identity, identity, identity, animatedVertices,
        triangle.indices, { 0.35f, -0.15f, 0.5f }, { 0.65f, 0.15f, 0.5f }), "animated triangle hit");
    Check(!ModelPicker::HitTest(50, 50, 100, 100, identity, identity, identity, animatedVertices,
        triangle.indices, { 0.35f, -0.15f, 0.5f }, { 0.65f, 0.15f, 0.5f }), "old triangle location misses");
    Check(bindVertices[0].x == -0.15f, "bind geometry remains unchanged");
}
