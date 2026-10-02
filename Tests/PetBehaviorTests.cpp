#include "PetBehavior.hpp"
#include "GravitySimulation.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

void Check(bool condition, const char* message);

namespace {
    Skeleton MakeSkeleton() {
        Skeleton skeleton;
        SkeletonNode root;
        root.name = "Root";
        root.localBindTransform = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
        skeleton.nodes.push_back(root);
        return skeleton;
    }
    AnimationClip MakeClip(const char* name) {
        AnimationClip clip;
        clip.name = name;
        clip.durationSeconds = 1;
        AnimationChannel channel;
        channel.nodeName = "Root";
        channel.positions = { { 0, { 0, 0, 0 } }, { 1, { 1, 0, 0 } } };
        channel.preBehavior = channel.postBehavior = AnimationBoundaryBehavior::Constant;
        clip.channels.push_back(channel);
        return clip;
    }
    PetBehaviorInput Grounded(bool justLanded = false) {
        PetBehaviorInput input;
        input.physicsValid = input.grounded = true;
        input.justLanded = justLanded;
        return input;
    }
    PetBehaviorInput Airborne() {
        PetBehaviorInput input;
        input.physicsValid = true;
        return input;
    }
    PetBehaviorInput Dragging() {
        PetBehaviorInput input;
        input.dragging = input.physicsSuspended = true;
        return input;
    }
    void Near(double value, double expected, const char* message) {
        Check(std::abs(value - expected) < 0.0001, message);
    }
}

void TestLoadedBehavior(const Skeleton& skeleton, const std::vector<AnimationClip>& clips,
    const AnimationNodeBindings& bindings) {
    Animator animator(skeleton, clips, bindings);
    PetBehavior behavior(animator, clips);
    behavior.Update(0.05, Dragging());
    Check(behavior.GetState() == PetState::Dragged, "loaded model drag state");
    const auto held = behavior.GetAssignedClip(PetState::Dragged);
    const auto idle = behavior.GetAssignedClip(PetState::Idle);
    Check(animator.GetClipIndex() == (held == InvalidSkeletonNode ? idle : held), "loaded held clip or fallback");
    behavior.Update(0.05, Airborne());
    Check(behavior.GetState() == PetState::Falling, "loaded model release state");
    behavior.Update(0, Grounded(true));
    if (behavior.GetAssignedClip(PetState::Landing) == InvalidSkeletonNode)
        Check(behavior.GetState() == PetState::Idle, "loaded model skips missing landing clip");
    else Check(behavior.GetState() == PetState::Landing, "loaded model one-shot landing");
}

void TestPetBehavior() {
    auto skeleton = MakeSkeleton();
    std::vector<AnimationClip> clips = { MakeClip("Rig|FaLl"), MakeClip("Rig|IDLE"),
        MakeClip("Rig|Pickup"), MakeClip("Rig|Land") };
    auto bindings = BindAnimationChannels(skeleton, clips);
    Animator animator(skeleton, clips, bindings);
    PetBehavior behavior(animator, clips);
    Check(behavior.GetState() == PetState::Idle && animator.GetClipIndex() == 1, "initial idle name mapping");
    behavior.Update(0.1, Grounded());
    Check(behavior.GetState() == PetState::Idle, "startup contact does not land");
    behavior.Update(0.1, Dragging());
    Check(behavior.GetState() == PetState::Dragged && animator.GetClipIndex() == 2, "drag takes priority over suspended physics");
    behavior.Update(0.1, Dragging());
    Near(animator.GetTimeSeconds(), 0.2, "held clip continues without per-frame restart");
    behavior.Update(0.1, Airborne());
    Check(behavior.GetState() == PetState::Falling && animator.GetClipIndex() == 0, "release above floor selects falling");
    behavior.Update(0.1, Grounded(true));
    Check(behavior.GetState() == PetState::Landing && animator.GetClipIndex() == 3 && !animator.IsLooping(), "landing plays once");
    behavior.Update(0.1, Grounded());
    Near(animator.GetTimeSeconds(), 0.2, "ground contact preserves unfinished landing");
    behavior.Update(0.85, Grounded());
    Check(behavior.GetState() == PetState::Idle && animator.GetClipIndex() == 1 && animator.IsLooping(), "landing completion returns to idle");
    behavior.Update(0.1, Airborne());
    behavior.Update(0.1, Grounded(true));
    behavior.Update(0.1, Dragging());
    Check(behavior.GetState() == PetState::Dragged, "regrab interrupts landing");
    behavior.Update(0.1, Airborne());
    PetBehaviorInput rotation;
    rotation.physicsSuspended = true;
    behavior.Update(0.1, rotation);
    Check(behavior.GetState() == PetState::Idle, "rotation and manual movement suspend behavior physics");
    behavior.Update(0.1, {});
    Check(behavior.GetState() == PetState::Idle, "failed physics query preserves state");
    const double oldTime = animator.GetTimeSeconds();
    bool rejected = false;
    try { behavior.Update(std::numeric_limits<double>::quiet_NaN(), Dragging()); }
    catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && behavior.GetState() == PetState::Idle, "invalid frame time cannot mutate state");
    Near(animator.GetTimeSeconds(), oldTime, "invalid frame time cannot advance animation");

    std::vector<AnimationClip> fallbackClips = { MakeClip("Jump"), MakeClip("Rig|Idle") };
    auto fallbackBindings = BindAnimationChannels(skeleton, fallbackClips);
    Animator fallbackAnimator(skeleton, fallbackClips, fallbackBindings);
    PetBehavior fallback(fallbackAnimator, fallbackClips);
    fallback.Update(0.1, Grounded());
    fallback.Update(0.1, Dragging());
    fallback.Update(0.1, Airborne());
    Check(fallback.GetState() == PetState::Falling && fallbackAnimator.GetClipIndex() == 1, "missing fall uses idle, never jump");
    Near(fallbackAnimator.GetTimeSeconds(), 0.3, "same fallback clip preserves time across states");
    fallback.Update(0.1, Grounded(true));
    Check(fallback.GetState() == PetState::Idle, "no landing clip skips landing state");

    std::vector<AnimationClip> unsupported = { MakeClip("Idle"), MakeClip("Wave"), MakeClip("Land") };
    unsupported[0].unsupportedMorphChannelCount = 1;
    unsupported[2].durationSeconds = 0;
    auto unsupportedBindings = BindAnimationChannels(skeleton, unsupported);
    Animator unsupportedAnimator(skeleton, unsupported, unsupportedBindings);
    PetBehavior supported(unsupportedAnimator, unsupported);
    Check(unsupportedAnimator.GetClipIndex() == 1, "first supported clip is idle fallback");
    Check(supported.GetAssignedClip(PetState::Landing) == InvalidSkeletonNode, "zero-duration landing excluded");

    const std::vector<AnimationClip> noClips;
    const AnimationNodeBindings noBindings;
    Animator staticAnimator(skeleton, noClips, noBindings);
    PetBehavior staticBehavior(staticAnimator, noClips);
    staticBehavior.Update(0.1, Dragging());
    staticBehavior.Update(0.1, Airborne());
    Check(staticBehavior.GetState() == PetState::Falling && staticAnimator.GetClipIndex() == InvalidSkeletonNode,
        "static model keeps bind pose through physics states");
    Near(staticAnimator.GetPose().localTransforms[0].elements[0], 1, "static bind pose preserved");

    GravitySimulation gravity;
    auto step = gravity.Step(0.4f, 0.01f, false);
    Check(step.result.valid && step.result.grounded && !step.result.justLanded && step.pixelMovement == 0,
        "fractional startup contact is stable");
    step = gravity.Step(100, 0.01f, false);
    Check(!step.result.grounded && step.result.verticalVelocity > 0, "fall accelerates above ground");
    step = gravity.Step(0.4f, 0.01f, false);
    Check(step.result.grounded && step.result.justLanded, "contact after falling emits landing");
    Check(!gravity.Step(0.4f, 0.01f, false).result.justLanded, "landing is a single pulse");
    step = gravity.Step(-0.4f, 0.01f, false);
    Check(step.pixelMovement == -1 && !step.result.justLanded, "overlap correction does not create landing");
    gravity.Step(100, 0.01f, false);
    step = gravity.Step(0, 0.01f, true);
    Check(step.result.valid && !step.result.grounded && step.result.verticalVelocity == 0, "suspension resets velocity");
    Check(!gravity.Step(0, 0.01f, false).result.justLanded, "release at ground does not create landing");
    step = gravity.Step(2.4f, 0.05f, false);
    Check(step.pixelMovement == 2 && step.result.grounded && step.result.justLanded, "single-frame fall reports applied contact");
    Check(!gravity.Step(2, 0, false).result.valid, "zero frame rejected");
    Check(!gravity.Step(std::numeric_limits<float>::quiet_NaN(), 0.01f, false).result.valid, "non-finite floor rejected");

    gravity.Reset();
    float distance = 100.4f;
    int landingCount = 0;
    for (int i = 0; i < 240; ++i) {
        step = gravity.Step(distance, 1.0f / 60, false);
        distance -= static_cast<float>(step.pixelMovement);
        landingCount += step.result.justLanded ? 1 : 0;
    }
    Check(distance >= 0 && distance <= 1 && landingCount == 1, "fall settles without repeated fractional landings");
}
