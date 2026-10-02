#include "AnimationSettingsStore.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

void Check(bool, const char*);

void TestAnimationSettings() {
    Skeleton skeleton;
    SkeletonNode root;
    root.name = "Root";
    root.localBindTransform = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
    skeleton.nodes.push_back(root);
    std::vector<AnimationClip> clips(4);
    const char* names[] = { "Idle", "Custom Held", "Custom Land", "Custom Held" };
    for (std::size_t c = 0; c < clips.size(); ++c) {
        clips[c].name = names[c];
        clips[c].durationSeconds = 1;
        AnimationChannel channel;
        channel.nodeName = "Root";
        channel.positions = { { 0, { static_cast<float>(c), 0, 0 } } };
        clips[c].channels.push_back(channel);
    }
    auto bindings = BindAnimationChannels(skeleton, clips);
    Animator animator(skeleton, clips, bindings);
    PetBehavior behavior(animator, clips);
    auto settings = behavior.GetSettings();
    settings.transitionSeconds = 0;
    settings.states[1].selection = ClipSelection::Clip;
    settings.states[1].clipIndex = 3; // Second clip with the same name.
    settings.states[1].speed = 2;
    settings.states[1].loop = false;
    settings.states[2].selection = ClipSelection::None;
    settings.states[3].selection = ClipSelection::Clip;
    settings.states[3].clipIndex = 2;
    behavior.SetSettings(settings);
    PetBehaviorInput dragged;
    dragged.dragging = true;
    behavior.Update(0.1, dragged);
    Check(animator.GetClipIndex() == 3 && !animator.IsLooping() && animator.GetSpeed() == 2, "custom held clip, speed and one-shot");
    settings.states[0].speed = 1.25;
    behavior.SetSettings(settings);
    Check(std::abs(animator.GetTimeSeconds() - 0.2) < 0.0001, "editing another state does not restart current animation");
    settings.states[0].speed = 1;
    behavior.SetSettings(settings);
    PetBehaviorInput airborne;
    airborne.physicsValid = true;
    behavior.Update(0.1, airborne);
    Check(animator.GetClipIndex() == InvalidSkeletonNode, "explicit none uses bind instead of idle fallback");
    PetBehaviorInput landed;
    landed.physicsValid = landed.grounded = landed.justLanded = true;
    behavior.Update(0.1, landed);
    Check(behavior.GetState() == PetState::Landing && animator.GetClipIndex() == 2, "custom landing assigned by index");
    auto invalid = settings;
    invalid.states[3].loop = true;
    bool rejected = false;
    try { behavior.SetSettings(invalid); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && !behavior.GetSettings().states[3].loop, "landing loop rejected atomically");
    invalid = settings;
    invalid.states[1].clipIndex = 99;
    rejected = false;
    try { behavior.SetSettings(invalid); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && behavior.GetSettings().states[1].clipIndex == 3, "invalid clip rejected atomically");
    settings.states[3].selection = ClipSelection::None;
    behavior.SetSettings(settings);
    Check(behavior.GetState() == PetState::Idle, "removing current landing cannot trap behavior");

    behavior.Preview(1, true, 0.5);
    behavior.Update(0.2, dragged);
    Check(behavior.IsPreviewing() && animator.GetClipIndex() == 1, "preview overrides physics state selection");
    Check(std::abs(animator.GetTimeSeconds() - 0.1) < 0.0001, "preview speed applied");
    behavior.PausePreview();
    behavior.SetPreviewOptions(false, 0.5);
    Check(!animator.IsLooping() && !animator.IsPlaying(), "preview options preserve pause and phase");
    behavior.Update(0.2, airborne);
    Check(std::abs(animator.GetTimeSeconds() - 0.1) < 0.0001, "preview pause");
    behavior.SeekPreview(0.6);
    behavior.ResumePreview();
    behavior.Update(0.2, {});
    Check(std::abs(animator.GetTimeSeconds() - 0.7) < 0.0001, "preview seek and resume");
    behavior.StopPreview();
    Check(!behavior.IsPreviewing() && animator.GetClipIndex() == 0 && animator.GetSpeed() == 1, "return restores automatic playback speed");

    const std::string file = "x64/Debug/animation-settings-test.txt";
    std::error_code cleanupError;
    std::filesystem::remove(file, cleanupError);
    AnimationSettingsStore store(file);
    std::string error;
    Check(store.Load(error), "missing settings file is valid default");
    Check(store.Save("Asset/Models/First Pet.fbx", settings, clips, error), "save first model");
    auto second = settings;
    second.states[0].selection = ClipSelection::None;
    Check(store.Save("Asset/Models/Second Pet.fbx", second, clips, error), "save second model without losing first");
    AnimationSettingsStore reopened(file);
    Check(reopened.Load(error), "reopen persisted settings");
    auto reordered = clips;
    std::swap(reordered[0], reordered[2]);
    auto restored = reopened.Restore("Asset/Models/./First Pet.fbx", reordered);
    Check(restored.found && restored.missingClips == 0 && restored.settings.states[1].clipIndex == 3,
        "path normalization and duplicate clip name ordinal survive reload");
    Check(restored.settings.states[1].speed == 2 && !restored.settings.states[1].loop &&
        restored.settings.states[2].selection == ClipSelection::None, "speed loop and none persist");
    settings.states[3].selection = ClipSelection::Clip;
    Check(reopened.Save("Asset/Models/First Pet.fbx", settings, clips, error), "save landing identity");
    restored = reopened.Restore("Asset/Models/First Pet.fbx", reordered);
    Check(restored.settings.states[3].clipIndex == 0, "clip identity survives index reorder");
    restored = reopened.Restore("Asset/Models/First Pet.fbx", {});
    Check(restored.missingClips == 2 && restored.settings.states[1].selection == ClipSelection::Automatic,
        "missing clips reset only affected selections to automatic");
    Check(reopened.Restore("Asset/Models/Second Pet.fbx", clips).settings.states[0].selection == ClipSelection::None,
        "separate model setting retained");
    Check(!reopened.Restore("Asset/Models/Third Pet.fbx", clips).found, "new model uses defaults");
    const auto validSize = std::filesystem::file_size(file);
    invalid = settings;
    invalid.transitionSeconds = std::numeric_limits<double>::infinity();
    Check(!reopened.Save("Asset/Models/First Pet.fbx", invalid, clips, error) &&
        std::filesystem::file_size(file) == validSize, "invalid save preserves prior file");
    { std::ofstream broken(file); broken << "desktop_pet_animation 1\nmodel \"broken\"\ntransition 0.2\n"; }
    Check(!reopened.Load(error) && !error.empty(), "truncated settings reports failure");
    Check(reopened.Restore("Asset/Models/Second Pet.fbx", clips).found, "failed load preserves in-memory settings");
    AnimationSettingsStore unwritable("x64/Debug/missing-settings-dir/state.txt");
    Check(!unwritable.Save("first.fbx", settings, clips, error) && !unwritable.Restore("first.fbx", clips).found,
        "failed write does not update in-memory settings");
    std::filesystem::remove(file, cleanupError);
}
