#include "PetBehavior.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace {
    bool Supported(const AnimationClip& clip) {
        return !clip.channels.empty() && !clip.unsupportedMeshChannelCount && !clip.unsupportedMorphChannelCount;
    }
    std::string ShortName(const AnimationClip& clip) {
        const auto separator = clip.name.find_last_of('|');
        auto name = clip.name.substr(separator == std::string::npos ? 0 : separator + 1);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return name;
    }
    std::size_t FindClip(const std::vector<AnimationClip>& clips, std::initializer_list<const char*> names, bool requireDuration = false) {
        for (const auto* name : names)
            for (std::size_t i = 0; i < clips.size(); ++i)
                if (Supported(clips[i]) && (!requireDuration || clips[i].durationSeconds > 0) && ShortName(clips[i]) == name) return i;
        return InvalidSkeletonNode;
    }
}

PetBehavior::PetBehavior(Animator& animator, const std::vector<AnimationClip>& clips) : animator(animator), clips(clips) {
    automaticClips.fill(InvalidSkeletonNode);
    automaticClips[0] = FindClip(clips, { "idle" });
    if (automaticClips[0] == InvalidSkeletonNode)
        for (std::size_t i = 0; i < clips.size(); ++i)
            if (Supported(clips[i])) { automaticClips[0] = i; break; }
    automaticClips[1] = FindClip(clips, { "dragged", "held", "grabbed", "pickup" });
    automaticClips[2] = FindClip(clips, { "falling", "fall" });
    automaticClips[3] = FindClip(clips, { "landing", "land" }, true);
    walkClips.resize(clips.size(), false);
    for (std::size_t c = 0; c < clips.size(); ++c)
        walkClips[c] = Supported(clips[c]) && clips[c].durationSeconds > 0 && !animator.HasPlanarRootMotion(c);
    for (const char* name : { "walk", "walking" }) {
        for (std::size_t c = 0; c < clips.size(); ++c)
            if (walkClips[c] && ShortName(clips[c]) == name) { automaticClips[WalkStateIndex] = c; break; }
        if (automaticClips[WalkStateIndex] != InvalidSkeletonNode) break;
    }
    ApplyState(PetState::Idle, true, true);
}

bool PetBehavior::IsClipSupported(std::size_t index) const {
    return index < clips.size() && Supported(clips[index]);
}

bool PetBehavior::IsWalkClipSupported(std::size_t index) const {
    return index < walkClips.size() && walkClips[index];
}

bool PetBehavior::HasWalkAnimation() const { return IsWalkClipSupported(GetAssignedClip(PetState::Walk)); }

std::size_t PetBehavior::GetAssignedClip(PetState target) const {
    const auto index = static_cast<std::size_t>(target);
    const auto& config = settings.states.at(index);
    if (config.selection == ClipSelection::None) return InvalidSkeletonNode;
    return config.selection == ClipSelection::Clip ? config.clipIndex : automaticClips.at(index);
}

void PetBehavior::ValidateSettings(const PetAnimationSettings& value) const {
    WanderController::ValidateSettings(value.wander);
    if (!std::isfinite(value.transitionSeconds) || value.transitionSeconds < 0 || value.transitionSeconds > 2)
        throw std::invalid_argument("Transition must be between 0 and 2 seconds");
    for (std::size_t i = 0; i < value.states.size(); ++i) {
        const auto& config = value.states[i];
        if (config.selection != ClipSelection::Automatic && config.selection != ClipSelection::None && config.selection != ClipSelection::Clip)
            throw std::invalid_argument("Invalid clip selection");
        if (!std::isfinite(config.speed) || config.speed < 0.05 || config.speed > 4)
            throw std::invalid_argument("Speed must be between 0.05 and 4");
        if (i == 3 && config.loop) throw std::invalid_argument("Landing must play once");
        if (i == WalkStateIndex && !config.loop) throw std::invalid_argument("Walking must loop");
        if (config.selection == ClipSelection::Clip && (!IsClipSupported(config.clipIndex) ||
            (i == 3 && clips[config.clipIndex].durationSeconds <= 0) ||
            (i == WalkStateIndex && !IsWalkClipSupported(config.clipIndex))))
            throw std::invalid_argument("Unsupported state clip");
    }
}

void PetBehavior::SetSettings(const PetAnimationSettings& value) {
    ValidateSettings(value);
    settings = value;
    if (!previewing) ApplyState((state == PetState::Landing && GetAssignedClip(PetState::Landing) == InvalidSkeletonNode) ||
        (state == PetState::Walk && (!settings.autonomousWalking || !HasWalkAnimation()))
        ? PetState::Idle : state);
}

void PetBehavior::Preview(std::size_t clip, bool loop, double speed) {
    if (!IsClipSupported(clip) || !std::isfinite(speed) || speed < 0.05 || speed > 4)
        throw std::invalid_argument("Invalid preview");
    animator.TransitionTo(clip, loop, settings.transitionSeconds);
    animator.SetSpeed(speed);
    previewing = true;
}

void PetBehavior::PausePreview() { if (previewing) animator.Pause(); }
void PetBehavior::SetPreviewOptions(bool loop, double speed) {
    if (!std::isfinite(speed) || speed < 0.05 || speed > 4) throw std::invalid_argument("Invalid preview speed");
    if (!previewing) return;
    if (animator.IsLooping() != loop) {
        const bool playing = animator.IsPlaying();
        animator.TransitionTo(animator.GetClipIndex(), loop, settings.transitionSeconds, false);
        if (!playing) animator.Pause();
    }
    animator.SetSpeed(speed);
}
void PetBehavior::ResumePreview() { if (previewing) animator.Resume(); }
void PetBehavior::SeekPreview(double seconds) { if (previewing) animator.Seek(seconds); }
void PetBehavior::StopPreview() {
    if (!previewing) return;
    previewing = false;
    ApplyState(PetState::Idle, true);
}

void PetBehavior::ApplyState(PetState next, bool force, bool immediate) {
    const bool enteringLanding = next == PetState::Landing && state != next;
    const bool changedState = state != next;
    state = next;
    const auto& config = settings.states[static_cast<std::size_t>(state)];
    auto clip = GetAssignedClip(state);
    if (clip == InvalidSkeletonNode && config.selection == ClipSelection::Automatic && state != PetState::Landing && state != PetState::Walk)
        clip = GetAssignedClip(PetState::Idle);
    const double transition = immediate ? 0 : settings.transitionSeconds;
    if (clip == InvalidSkeletonNode) {
        if (force || animator.GetClipIndex() != InvalidSkeletonNode)
            animator.TransitionTo(InvalidSkeletonNode, true, transition);
        return;
    }
    const bool loop = config.loop;
    if (force || enteringLanding || (changedState && !loop) || animator.GetClipIndex() != clip || animator.IsLooping() != loop)
        animator.TransitionTo(clip, loop, transition);
    animator.SetSpeed(config.speed);
}

void PetBehavior::Update(double deltaSeconds, const PetBehaviorInput& input) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0) throw std::invalid_argument("Invalid behavior frame time");
    if (previewing) { animator.Update(deltaSeconds); return; }
    PetState next = state;
    if (input.dragging) next = PetState::Dragged;
    else if (input.physicsSuspended) next = PetState::Idle;
    else if (input.physicsValid) {
        if (!input.grounded) next = PetState::Falling;
        else if (state == PetState::Landing && !animator.IsFinished()) next = PetState::Landing;
        else if (input.justLanded && state != PetState::Landing && GetAssignedClip(PetState::Landing) != InvalidSkeletonNode)
            next = PetState::Landing;
        else if (input.walking && settings.autonomousWalking && HasWalkAnimation()) next = PetState::Walk;
        else next = PetState::Idle;
    }
    ApplyState(next);
    animator.Update(deltaSeconds);
    if (state == PetState::Landing && animator.IsFinished()) ApplyState(PetState::Idle);
}
