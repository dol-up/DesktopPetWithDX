#include "Animator.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

void Check(bool, const char*);
namespace {
    void Near(float actual, float expected, const char* message) { Check(std::abs(actual - expected) < 0.001f, message); }
}

void TestAnimatorTransitions() {
    Skeleton skeleton;
    SkeletonNode root;
    root.name = "Root";
    root.localBindTransform = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
    skeleton.nodes.push_back(root);
    root.name = "Child";
    root.parentIndex = 0;
    root.localBindTransform.elements[7] = 2;
    skeleton.nodes.push_back(root);
    skeleton.meshes.resize(1);
    SkeletonBone bone;
    bone.nodeIndex = 1;
    bone.offsetMatrix = skeleton.nodes[0].localBindTransform;
    skeleton.meshes[0].bones.push_back(bone);
    std::vector<AnimationClip> clips(3);
    for (std::size_t c = 0; c < clips.size(); ++c) {
        clips[c].durationSeconds = 2;
        AnimationChannel channel;
        channel.nodeName = "Root";
        channel.positions = { { 0, { static_cast<float>(c * 10), 0, 0 } } };
        channel.preBehavior = channel.postBehavior = AnimationBoundaryBehavior::Constant;
        clips[c].channels.push_back(channel);
    }
    // A 180-degree rotation with the opposite quaternion sign still follows the shortest arc.
    clips[1].channels[0].rotations = { { 0, { 0, 0, -1, 0 } } };
    const auto bindings = BindAnimationChannels(skeleton, clips);
    Animator animator(skeleton, clips, bindings);
    animator.Play(0);
    animator.TransitionTo(1, true, 1);
    Near(animator.GetPose().localTransforms[0].elements[3], 0, "transition starts at visible source");
    animator.Update(0.5);
    auto pose = animator.GetPose();
    Near(pose.localTransforms[0].elements[3], 5, "midpoint translation");
    Near(pose.localTransforms[0].elements[0], 0, "rotation midpoint preserves orthogonality");
    Near(std::abs(pose.localTransforms[0].elements[1]), 1, "rotation midpoint uses slerp");
    Near(pose.globalTransforms[1].elements[3], pose.boneTransforms[0][0].elements[3], "bone palette follows blended hierarchy");
    const auto visible = pose.localTransforms[0];
    animator.TransitionTo(2, true, 1);
    for (std::size_t i = 0; i < 16; ++i)
        Near(animator.GetPose().localTransforms[0].elements[i], visible.elements[i], "interrupted fade has no pose jump");
    animator.Pause();
    const auto revision = animator.GetPoseRevision();
    animator.Update(1);
    Check(animator.GetPoseRevision() == revision, "pause freezes fade and clip");
    animator.Resume();
    animator.Update(1);
    Check(!animator.IsTransitioning(), "fade finishes");
    Near(animator.GetPose().localTransforms[0].elements[3], 20, "finished fade exact target");
    animator.TransitionTo(InvalidSkeletonNode, true, 1);
    animator.Update(0.5);
    Near(animator.GetPose().localTransforms[0].elements[3], 10, "blend back to bind pose");
    animator.Update(0.5);
    Near(animator.GetPose().localTransforms[0].elements[3], 0, "bind fade completes without a playing clip");
    animator.TransitionTo(2, true, 0);
    Check(!animator.IsTransitioning(), "zero-duration immediate switch");
    Near(animator.GetPose().localTransforms[0].elements[3], 20, "immediate switch target");
    animator.TransitionTo(1, false, 1);
    animator.Seek(0.5);
    Check(!animator.IsTransitioning(), "seeking cancels fade for precise preview");
    Near(animator.GetPose().localTransforms[0].elements[3], 10, "seek samples exact target pose");
    const auto before = animator.GetPoseRevision();
    bool rejected = false;
    try { animator.TransitionTo(0, true, std::numeric_limits<double>::quiet_NaN()); }
    catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && animator.GetPoseRevision() == before, "invalid fade duration does not mutate playback");
}
