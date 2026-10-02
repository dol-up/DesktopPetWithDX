#include "Animator.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

void Check(bool condition, const char* message);
namespace {
    SkeletonMatrix Identity() { return { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } }; }
    void Near(double actual, double expected, const char* message) {
        Check(std::abs(actual - expected) < 0.0001, message);
    }

    double PositionAt(std::vector<AnimationKey<AnimationVector3>> keys,
        AnimationBoundaryBehavior behavior, double time) {
        Skeleton skeleton;
        SkeletonNode node;
        node.name = "Root";
        node.localBindTransform = Identity();
        node.localBindTransform.elements[3] = 7;
        skeleton.nodes.push_back(node);
        std::vector<AnimationClip> clips(1);
        clips[0].durationSeconds = 3;
        AnimationChannel channel;
        channel.nodeName = "Root";
        channel.positions = std::move(keys);
        channel.preBehavior = channel.postBehavior = behavior;
        clips[0].channels.push_back(channel);
        auto bindings = BindAnimationChannels(skeleton, clips);
        Animator animator(skeleton, clips, bindings);
        animator.Play(0, false);
        animator.Seek(time);
        return animator.GetPose().localTransforms[0].elements[3];
    }
}

void TestAnimator() {
    Skeleton skeleton;
    SkeletonNode root;
    root.name = "Root";
    root.localBindTransform = { { 0, -1, 0, 10, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
    skeleton.nodes.push_back(root);
    SkeletonNode child;
    child.name = "Child";
    child.parentIndex = 0;
    child.localBindTransform = Identity();
    child.localBindTransform.elements[7] = 2;
    skeleton.nodes.push_back(child);
    skeleton.meshes.resize(1);
    SkeletonBone bone;
    bone.nodeIndex = 1;
    bone.offsetMatrix = Identity();
    bone.offsetMatrix.elements[3] = -1;
    skeleton.meshes[0].bones.push_back(bone);
    std::vector<AnimationClip> clips(2);
    clips[0].durationSeconds = 2;
    AnimationChannel channel;
    channel.nodeName = "Child";
    channel.positions = { { 0, { 0, 2, 0 } }, { 2, { 4, 2, 0 } } };
    channel.rotations = { { 0, { 0, 0, 0, 1 } }, { 2, { 0, 0, 1, 0 } } };
    channel.scales = { { 0, { 2, 3, 4 } } };
    channel.preBehavior = channel.postBehavior = AnimationBoundaryBehavior::Constant;
    clips[0].channels.push_back(channel);
    auto bindings = BindAnimationChannels(skeleton, clips);
    Animator animator(skeleton, clips, bindings);
    Near(animator.GetPose().globalTransforms[1].elements[3], 8, "bind parent rotation");
    animator.Play(0, false);
    animator.Update(1);
    const auto& pose = animator.GetPose();
    Near(pose.localTransforms[1].elements[3], 2, "position interpolation");
    Near(pose.localTransforms[1].elements[4], 2, "slerp and non-uniform scale column 0");
    Near(pose.localTransforms[1].elements[1], -3, "slerp and non-uniform scale column 1");
    Near(pose.globalTransforms[1].elements[3], 8, "parent rotation applied to child translation");
    Near(pose.globalTransforms[1].elements[7], 2, "parent transform order");
    Near(pose.boneTransforms[0][0].elements[3], 10, "bone offset multiplication");
    animator.Play(0, false);
    Near(animator.GetTimeSeconds(), 1, "same clip does not restart");
    animator.Pause();
    animator.Update(0.5);
    Near(animator.GetTimeSeconds(), 1, "pause");
    animator.Resume();
    animator.SetSpeed(2);
    animator.Update(1);
    Near(animator.GetTimeSeconds(), 2, "one-shot clamps endpoint");
    Check(animator.IsFinished() && !animator.IsPlaying(), "one-shot completion");
    animator.Seek(0.5);
    Check(!animator.IsFinished() && !animator.IsPlaying(), "seek clears completion without resuming");
    animator.Resume();
    animator.Update(0.25);
    Near(animator.GetTimeSeconds(), 1, "resume after seek and speed");
    animator.Play(0, true, true);
    animator.Update(3.25);
    Near(animator.GetTimeSeconds(), 0.5, "multiple loop wrap");
    animator.Seek(2);
    Near(animator.GetTimeSeconds(), 2, "loop endpoint seek is exact");
    animator.Update(0.25);
    Near(animator.GetTimeSeconds(), 0.5, "loop resumes after endpoint");
    animator.SetSpeed(0);
    animator.Update(1);
    Near(animator.GetTimeSeconds(), 0.5, "zero speed");
    animator.Stop();
    Check(animator.GetPose().localTransforms[0].elements == root.localBindTransform.elements,
        "stop restores exact bind matrix");
    Check(animator.GetClipIndex() == InvalidSkeletonNode && !animator.IsFinished(), "stop clears selection");
    animator.Play(1, false);
    Check(animator.IsFinished() && !animator.IsPlaying(), "zero duration one-shot");
    animator.Play(1, true);
    animator.Update(1);
    Check(!animator.IsFinished() && !animator.IsPlaying(), "zero duration loop holds pose");
    bool rejected = false;
    try { animator.SetSpeed(-1); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected, "negative speed rejected");
    rejected = false;
    try { animator.Update(std::numeric_limits<double>::quiet_NaN()); }
    catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected, "NaN delta rejected");
    rejected = false;
    try { animator.Play(99); } catch (const std::out_of_range&) { rejected = true; }
    Check(rejected, "invalid clip rejected");

    const std::vector<AnimationKey<AnimationVector3>> keys = { { 1, { 10, 0, 0 } }, { 2, { 20, 0, 0 } } };
    Near(PositionAt(keys, AnimationBoundaryBehavior::DefaultPose, 0), 7, "default pre-state");
    Near(PositionAt(keys, AnimationBoundaryBehavior::DefaultPose, 3), 7, "default post-state");
    Near(PositionAt(keys, AnimationBoundaryBehavior::Constant, 0), 10, "constant pre-state");
    Near(PositionAt(keys, AnimationBoundaryBehavior::Constant, 3), 20, "constant post-state");
    Near(PositionAt(keys, AnimationBoundaryBehavior::Linear, 0), 0, "linear extrapolation before");
    Near(PositionAt(keys, AnimationBoundaryBehavior::Linear, 3), 30, "linear extrapolation after");
    Near(PositionAt(keys, AnimationBoundaryBehavior::Repeat, 0.25), 12.5, "repeat before");
    Near(PositionAt(keys, AnimationBoundaryBehavior::Repeat, 2.25), 12.5, "repeat after");
    Near(PositionAt({}, AnimationBoundaryBehavior::Constant, 1), 7, "missing keys use bind");
    Near(PositionAt({ { 0, { 1, 0, 0 } }, { 1, { 2, 0, 0 } }, { 1, { 3, 0, 0 } },
        { 2, { 4, 0, 0 } } }, AnimationBoundaryBehavior::Constant, 1), 3, "duplicate timestamps");
}
