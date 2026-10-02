#include "AnimationClip.hpp"
#include "Skeleton.hpp"
#include "Animator.hpp"
#include "Skinning.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <objbase.h>

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void TestCopiesAndTiming() {
    std::vector<AnimationClip> clips;
    {
        aiScene scene;
        scene.mNumAnimations = 3;
        scene.mAnimations = new aiAnimation*[3]{ new aiAnimation, new aiAnimation, new aiAnimation };
        aiAnimation& first = *scene.mAnimations[0];
        first.mName.Set("Idle");
        first.mDuration = 50;
        first.mTicksPerSecond = 25;
        first.mNumChannels = 1;
        first.mChannels = new aiNodeAnim*[1]{ new aiNodeAnim };
        aiNodeAnim& channel = *first.mChannels[0];
        channel.mNodeName.Set("Hip");
        channel.mPreState = aiAnimBehaviour_CONSTANT;
        channel.mPostState = aiAnimBehaviour_REPEAT;
        channel.mNumPositionKeys = 2;
        channel.mPositionKeys = new aiVectorKey[2]{
            aiVectorKey(0, aiVector3D(1, 2, 3)), aiVectorKey(25, aiVector3D(4, 5, 6)) };
        channel.mNumRotationKeys = 1;
        channel.mRotationKeys = new aiQuatKey[1]{ aiQuatKey(0, aiQuaternion(0.5f, 0.1f, 0.2f, 0.3f)) };
        channel.mNumScalingKeys = 1;
        channel.mScalingKeys = new aiVectorKey[1]{ aiVectorKey(75, aiVector3D(2, 2, 2)) };
        scene.mAnimations[1]->mDuration = 25;
        scene.mAnimations[2]->mName.Set("Idle");
        scene.mAnimations[2]->mNumMorphMeshChannels = 1;
        scene.mAnimations[2]->mMorphMeshChannels = new aiMeshMorphAnim*[1]{ new aiMeshMorphAnim };
        clips = LoadAnimationClips(scene);
        channel.mPositionKeys[0].mValue.x = 99;
    } // All Assimp memory freed before assertions.
    Check(clips.size() == 3, "clip count");
    Check(clips[0].name == "Idle" && clips[2].sourceIndex == 2, "duplicate names retain IDs");
    Check(clips[1].name.empty() && clips[1].displayName == "Animation 2", "unnamed clip");
    Check(clips[1].usedDefaultTicksPerSecond && clips[1].durationSeconds == 1, "default ticks/s");
    Check(!clips[0].usedDefaultTicksPerSecond && clips[0].durationSeconds == 3, "duration includes final key");
    const auto& channel = clips[0].channels[0];
    Check(channel.nodeName == "Hip" && channel.positions[0].value.x == 1, "deep copy lifetime");
    Check(channel.positions[1].timeSeconds == 1 && channel.positions[1].value.z == 6, "position timing");
    Check(channel.rotations[0].value.w == 0.5f && channel.rotations[0].value.x == 0.1f, "quaternion order");
    Check(channel.scales[0].timeSeconds == 3 && channel.scales[0].value.x == 2, "scale keys");
    Check(channel.preBehavior == AnimationBoundaryBehavior::Constant &&
        channel.postBehavior == AnimationBoundaryBehavior::Repeat, "boundary behaviors");
    Check(clips[2].unsupportedMorphChannelCount == 1, "unsupported payload is visible");
    Check(clips[2].durationSeconds == 0 && clips[2].channels.empty(), "empty clip");
}

void TestInvalidData() {
    aiScene scene;
    Check(LoadAnimationClips(scene).empty(), "static model");
    scene.mNumAnimations = 1;
    scene.mAnimations = new aiAnimation*[1]{ new aiAnimation };
    auto& animation = *scene.mAnimations[0];
    animation.mTicksPerSecond = std::numeric_limits<double>::quiet_NaN();
    bool rejected = false;
    try { LoadAnimationClips(scene); } catch (const std::runtime_error&) { rejected = true; }
    Check(rejected, "non-finite timing rejected");
    animation.mTicksPerSecond = 1;
    animation.mNumChannels = 1;
    animation.mChannels = new aiNodeAnim*[1]{ new aiNodeAnim };
    auto& channel = *animation.mChannels[0];
    channel.mNumPositionKeys = 2;
    channel.mPositionKeys = new aiVectorKey[2]{
        aiVectorKey(2, aiVector3D()), aiVectorKey(1, aiVector3D()) };
    rejected = false;
    try { LoadAnimationClips(scene); } catch (const std::runtime_error&) { rejected = true; }
    Check(rejected, "unordered keys rejected");
}

void TestSkeleton();
void TestPetBehavior();
void TestAnimationSettings();
void TestAnimatorTransitions();
void TestSettingsWindow(const char*);
void TestLoadedBehavior(const Skeleton&, const std::vector<AnimationClip>&, const AnimationNodeBindings&);
void TestAnimator();
void TestSkinning();
void TestGpuSkinning(const SkinningGeometry&, const std::vector<SkeletonMatrix>&);
void TestModelRendering(const char*, int);
void TestWanderController();
void TestWalking();
void TestWindowPhysicsMotion();
void TestAutonomousMotion(const char*);

int main(int argc, char** argv) {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) return 1;
    struct ComScope { ~ComScope() { CoUninitialize(); } } comScope;
    try {
        TestCopiesAndTiming();
        TestInvalidData();
        TestSkeleton();
        TestAnimator();
        TestSkinning();
        TestPetBehavior();
        TestAnimationSettings();
        TestAnimatorTransitions();
        TestWanderController();
        TestWalking();
        TestWindowPhysicsMotion();
        std::cout << "Wander timing, screen boundaries and combined window physics tests passed\n";
        std::cout << "Settings persistence, preview and transition tests passed\n";
        std::cout << "Behavior transitions and gravity contact tests passed\n";
        std::cout << "Skinning, picking and WARP shader tests passed\n";
        std::cout << "Synthetic animator tests passed\n";
        std::cout << "Synthetic skeleton tests passed\n";
        std::cout << "Synthetic animation tests passed\n";
        bool checkedSettingsWindow = false;
        bool checkedStaticSettingsWindow = false;
        for (int i = 1; i < argc; ++i) {
            std::vector<AnimationClip> clips;
            Skeleton skeleton;
            AnimationNodeBindings bindings;
            SkinningGeometry geometry;
            {
                Assimp::Importer importer;
                const aiScene* scene = importer.ReadFile(argv[i], aiProcess_Triangulate |
                    aiProcess_ConvertToLeftHanded | aiProcess_JoinIdenticalVertices);
                if (!scene) throw std::runtime_error(importer.GetErrorString());
                clips = LoadAnimationClips(*scene);
                skeleton = LoadSkeleton(*scene);
                bindings = BindAnimationChannels(skeleton, clips);
                geometry = BuildSkinningGeometry(*scene, skeleton);
                Check(clips.size() == scene->mNumAnimations, "imported clip count");
                Check(skeleton.meshes.size() == scene->mNumMeshes, "imported mesh count");
                for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
                    Check(skeleton.meshes[m].bones.size() == scene->mMeshes[m]->mNumBones, "imported bones");
                    Check(skeleton.meshes[m].vertexInfluences.size() == scene->mMeshes[m]->mNumVertices,
                        "imported vertex influence count");
                }
            }
            std::size_t boneCount = 0;
            TestLoadedBehavior(skeleton, clips, bindings);
            if (!checkedSettingsWindow && !clips.empty()) {
                TestSettingsWindow(argv[i]);
                checkedSettingsWindow = true;
            }
            if (!checkedStaticSettingsWindow && clips.empty()) {
                TestSettingsWindow(argv[i]);
                checkedStaticSettingsWindow = true;
            }
            Animator animator(skeleton, clips, bindings);
            for (std::size_t c = 0; c < clips.size(); ++c) {
                if (clips[c].unsupportedMeshChannelCount || clips[c].unsupportedMorphChannelCount) continue;
                animator.Play(c, false, true);
                for (double fraction : { 0.0, 0.25, 0.5, 0.75, 1.0 }) {
                    animator.Seek(clips[c].durationSeconds * fraction);
                    for (const auto& matrix : animator.GetPose().globalTransforms)
                        for (float value : matrix.elements) Check(std::isfinite(value), "finite sampled pose");
                    for (const auto& mesh : animator.GetPose().boneTransforms)
                        for (const auto& matrix : mesh)
                            for (float value : matrix.elements) Check(std::isfinite(value), "finite bone pose");
                }
            }
            for (const auto& mesh : skeleton.meshes) boneCount += mesh.bones.size();
            TestGpuSkinning(geometry, BuildSkinningPalette(skeleton, animator.GetPose(), geometry));
            TestModelRendering(argv[i], i);
            TestAutonomousMotion(argv[i]);
            std::cout << "  nodes=" << skeleton.nodes.size() << " mesh-bones=" << boneCount
                << " bound-clips=" << bindings.size() << '\n';
            std::cout << argv[i] << ": " << clips.size() << " clips\n";
            for (const auto& clip : clips) {
                std::cout << "  [" << clip.sourceIndex << "] " << clip.displayName
                    << " seconds=" << clip.durationSeconds << " channels=" << clip.channels.size() << '\n';
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
