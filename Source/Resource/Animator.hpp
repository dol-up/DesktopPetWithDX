#pragma once
#include "Skeleton.hpp"
#include <cstdint>

struct AnimationPose {
    std::vector<SkeletonMatrix> localTransforms;
    std::vector<SkeletonMatrix> globalTransforms;
    // [mesh][bone]: mesh-local vertex -> scene space, global(node) * offset.
    // Root transform is retained. No normalization or DirectX transpose applied.
    std::vector<std::vector<SkeletonMatrix>> boneTransforms;
};

class Animator {
public:
    // Resources must outlive the animator and remain unchanged.
    Animator(const Skeleton& skeleton, const std::vector<AnimationClip>& clips,
        const AnimationNodeBindings& bindings);
    Animator(const Animator&) = delete;
    Animator& operator=(const Animator&) = delete;

    // Repeated Play on the same clip preserves time unless restart is true.
    void Play(std::size_t clipIndex, bool loop = true, bool restart = false);
    // InvalidSkeletonNode selects the bind pose. Interrupted fades start at the visible pose.
    void TransitionTo(std::size_t clipIndex, bool loop, double seconds, bool restart = true);
    void Pause() { playing = false; paused = true; }
    void Resume();
    void Stop(); // Clear selection and restore exact bind transforms.
    void Seek(double seconds); // Clamp to [0, duration], including the endpoint.
    void SetSpeed(double multiplier); // Finite, non-negative; 0 freezes time.
    void Update(double deltaSeconds);

    const AnimationPose& GetPose() const { return pose; }
    std::uint64_t GetPoseRevision() const { return poseRevision; }
    std::size_t GetClipIndex() const { return clipIndex; }
    double GetTimeSeconds() const { return timeSeconds; }
    double GetSpeed() const { return speed; }
    bool IsPlaying() const { return playing; }
    bool IsFinished() const { return finished; }
    bool IsLooping() const { return looping; }
    bool IsTransitioning() const { return !transitionSource.empty(); }
    // Checks net horizontal displacement of skeletal roots without changing playback.
    bool HasPlanarRootMotion(std::size_t clip) const;

private:
    void Evaluate();
    const Skeleton& skeleton;
    const std::vector<AnimationClip>& clips;
    const AnimationNodeBindings& bindings;
    AnimationPose pose;
    std::uint64_t poseRevision = 0;
    std::size_t clipIndex = InvalidSkeletonNode;
    double timeSeconds = 0.0;
    double speed = 1.0;
    bool looping = true;
    bool playing = false;
    bool finished = false;
    bool paused = false;
    std::vector<SkeletonMatrix> transitionSource;
    double transitionElapsed = 0;
    double transitionDuration = 0;
};
