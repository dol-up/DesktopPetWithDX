#pragma once
#include "Animator.hpp"
#include "WanderController.hpp"
#include <array>

enum class PetState { Idle, Dragged, Falling, Landing, Walk, Count };
constexpr std::size_t PetStateCount = static_cast<std::size_t>(PetState::Count);
constexpr std::size_t WalkStateIndex = static_cast<std::size_t>(PetState::Walk);

enum class ClipSelection { Automatic, None, Clip };
struct StateAnimationSettings {
    ClipSelection selection = ClipSelection::Automatic;
    std::size_t clipIndex = InvalidSkeletonNode;
    double speed = 1;
    bool loop = true;
};
struct PetAnimationSettings {
    std::array<StateAnimationSettings, PetStateCount> states;
    double transitionSeconds = 0.15;
    bool autonomousWalking = false;
    WanderSettings wander;
    PetAnimationSettings() { states[3].loop = false; }
};

struct PetBehaviorInput {
    bool dragging = false;
    bool physicsSuspended = false;
    bool physicsValid = false;
    bool grounded = false;
    bool justLanded = false;
    bool walking = false;
};

class PetBehavior {
public:
    // Per-model owner; resources must outlive the controller.
    PetBehavior(Animator& animator, const std::vector<AnimationClip>& clips);
    PetBehavior(const PetBehavior&) = delete;
    PetBehavior& operator=(const PetBehavior&) = delete;
    void Update(double deltaSeconds, const PetBehaviorInput& input);
    PetState GetState() const { return state; }
    std::size_t GetAssignedClip(PetState target) const;
    const PetAnimationSettings& GetSettings() const { return settings; }
    void SetSettings(const PetAnimationSettings& value); // Validate completely before applying.
    void ValidateSettings(const PetAnimationSettings& value) const;
    bool IsClipSupported(std::size_t index) const;
    bool IsWalkClipSupported(std::size_t index) const;
    bool HasWalkAnimation() const;
    void Preview(std::size_t clip, bool loop = true, double speed = 1);
    void PausePreview();
    void SetPreviewOptions(bool loop, double speed);
    void ResumePreview();
    void SeekPreview(double seconds);
    void StopPreview();
    bool IsPreviewing() const { return previewing; }
private:
    void ApplyState(PetState next, bool force = false, bool immediate = false);
    Animator& animator;
    const std::vector<AnimationClip>& clips;
    std::array<std::size_t, PetStateCount> automaticClips;
    std::vector<bool> walkClips;
    PetAnimationSettings settings;
    bool previewing = false;
    PetState state = PetState::Idle;
};
