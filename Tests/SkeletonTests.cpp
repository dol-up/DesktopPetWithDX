#include "Skeleton.hpp"
#include <assimp/scene.h>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>

void Check(bool condition, const char* message);

namespace {
    std::unique_ptr<aiScene> MakeScene() {
        auto scene = std::make_unique<aiScene>();
        scene->mRootNode = new aiNode("Root");
        auto& root = *scene->mRootNode;
        root.mTransformation.a4 = 10;
        root.mNumChildren = 1;
        root.mChildren = new aiNode*[1]{ new aiNode("Bridge") };
        auto& bridge = *root.mChildren[0];
        bridge.mParent = &root;
        bridge.mTransformation.b4 = 20;
        bridge.mNumChildren = 5;
        bridge.mChildren = new aiNode*[5]{};
        for (unsigned int i = 0; i < 5; ++i) {
            bridge.mChildren[i] = new aiNode("Bone" + std::to_string(i));
            bridge.mChildren[i]->mParent = &bridge;
        }
        root.mNumMeshes = 2;
        root.mMeshes = new unsigned int[2]{ 0, 1 };
        bridge.mNumMeshes = 1;
        bridge.mMeshes = new unsigned int[1]{ 0 }; // second instance of mesh 0
        scene->mNumMeshes = 2;
        scene->mMeshes = new aiMesh*[2]{ new aiMesh, new aiMesh };
        for (unsigned int m = 0; m < 2; ++m) {
            auto& mesh = *scene->mMeshes[m];
            mesh.mNumVertices = 3;
            mesh.mNumBones = 5;
            mesh.mBones = new aiBone*[5]{};
            for (unsigned int b = 0; b < 5; ++b) {
                auto* bone = new aiBone;
                mesh.mBones[b] = bone;
                bone->mName.Set("Bone" + std::to_string(b));
                bone->mOffsetMatrix.c4 = static_cast<float>(m + 1);
                bone->mNumWeights = 2;
                bone->mWeights = new aiVertexWeight[2]{ aiVertexWeight(0, 0.2f), aiVertexWeight(1, 0) };
            }
        }
        return scene;
    }

    void ExpectError(const std::function<void()>& operation, const char* message) {
        bool rejected = false;
        try { operation(); } catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, message);
    }
}

void TestSkeleton() {
    Skeleton skeleton;
    {
        auto scene = MakeScene();
        skeleton = LoadSkeleton(*scene);
        scene->mRootNode->mTransformation.a4 = 999;
        scene->mMeshes[0]->mBones[0]->mWeights[0].mWeight = 1;
    }
    Check(skeleton.nodes.size() == 7, "all hierarchy nodes copied");
    Check(skeleton.nodes[0].parentIndex == InvalidSkeletonNode, "root sentinel");
    Check(skeleton.nodes[1].parentIndex == 0 && skeleton.nodes[2].parentIndex == 1,
        "non-bone parent preserved");
    Check(skeleton.nodes[0].localBindTransform.elements[3] == 10 &&
        skeleton.nodes[1].localBindTransform.elements[7] == 20, "matrix layout and lifetime");
    Check(skeleton.meshes[0].nodeIndices == std::vector<std::size_t>({ 0, 1 }), "mesh instances");
    Check(skeleton.meshes[0].bones[0].nodeIndex == 2 && skeleton.meshes[1].bones[0].nodeIndex == 2,
        "shared bone node");
    Check(skeleton.meshes[0].bones[0].offsetMatrix.elements[11] == 1 &&
        skeleton.meshes[1].bones[0].offsetMatrix.elements[11] == 2, "per-mesh offsets preserved");
    const auto& influences = skeleton.meshes[0].vertexInfluences;
    Check(influences[0].size() == 5 && influences[0][4].boneIndex == 4 &&
        influences[0][0].weight == 0.2f, "all influences deep copied without truncation");
    Check(influences[1].empty() && influences[2].empty(), "zero and unweighted vertices");

    AnimationClip clip;
    AnimationChannel channel;
    channel.nodeName = "Bone0";
    clip.channels.push_back(channel);
    Check(BindAnimationChannels(skeleton, { clip })[0][0] == 2, "channel binding");
    clip.channels[0].nodeName = "Missing";
    ExpectError([&] { BindAnimationChannels(skeleton, { clip }); }, "missing channel node");
    clip.channels[0].nodeName = "Bone0";
    clip.channels.push_back(clip.channels[0]);
    ExpectError([&] { BindAnimationChannels(skeleton, { clip }); }, "duplicate channel target");
    clip.channels.pop_back();
    skeleton.nodes[3].name = "Bone0";
    ExpectError([&] { BindAnimationChannels(skeleton, { clip }); }, "ambiguous channel node");

    auto scene = MakeScene();
    auto& weight = scene->mMeshes[0]->mBones[0]->mWeights[0];
    weight.mVertexId = 3;
    ExpectError([&] { LoadSkeleton(*scene); }, "out of range vertex");
    weight.mVertexId = 0;
    weight.mWeight = -1;
    ExpectError([&] { LoadSkeleton(*scene); }, "negative weight");
    weight.mWeight = std::numeric_limits<float>::quiet_NaN();
    ExpectError([&] { LoadSkeleton(*scene); }, "non-finite weight");
    weight.mWeight = 0.2f;
    scene->mMeshes[0]->mBones[0]->mName.Set("Missing");
    ExpectError([&] { LoadSkeleton(*scene); }, "missing bone node");
    scene->mMeshes[0]->mBones[0]->mName.Set("Bone0");
    scene->mRootNode->mChildren[0]->mChildren[1]->mName.Set("Bone0");
    ExpectError([&] { LoadSkeleton(*scene); }, "ambiguous bone node");

    aiScene staticScene;
    staticScene.mRootNode = new aiNode("Static");
    Check(LoadSkeleton(staticScene).nodes.size() == 1, "static scene supported");
    Check(BindAnimationChannels(LoadSkeleton(staticScene), {}).empty(), "no clips supported");
}
