#include "PetBehavior.hpp"
#include "AnimationSettingsStore.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <cmath>
#include <algorithm>

void Check(bool, const char*);

namespace {
    Skeleton SkeletonForWalk() {
        Skeleton skeleton;
        SkeletonNode node;
        node.name = "Root";
        node.localBindTransform = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
        skeleton.nodes.push_back(node);
        return skeleton;
    }
    AnimationClip Clip(const char* name, bool forward = false) {
        AnimationClip clip;
        clip.name = name;
        clip.durationSeconds = 1;
        AnimationChannel channel;
        channel.nodeName = "Root";
        channel.positions = { { 0, { 0, 0, 0 } }, { 1, { forward ? 1.0f : 0, 0, 0 } } };
        clip.channels.push_back(channel);
        return clip;
    }
    PetBehaviorInput Moving() {
        PetBehaviorInput input;
        input.physicsValid = input.grounded = input.walking = true;
        return input;
    }
    std::string ModelKey(const char* path) {
        auto result = std::filesystem::absolute(path).lexically_normal().generic_u8string();
        for (auto& c : result) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return result;
    }
}

void TestWalking() {
    const auto skeleton = SkeletonForWalk();
    std::vector<AnimationClip> clips = { Clip("Rig|Idle"), Clip("Rig|Walk"), Clip("Rig|Pickup"), Clip("Rig|Land"),
        Clip("Forward motion", true), Clip("Other Walk") };
    const auto bindings = BindAnimationChannels(skeleton, clips);
    Animator animator(skeleton, clips, bindings);
    PetBehavior behavior(animator, clips);
    Check(behavior.GetAssignedClip(PetState::Walk) == 1 && behavior.HasWalkAnimation(), "walk short-name automatic selection");
    const auto revision = animator.GetPoseRevision();
    Check(animator.HasPlanarRootMotion(4) && !animator.HasPlanarRootMotion(1) && animator.GetPoseRevision() == revision,
        "root displacement check does not mutate visible playback");
    auto settings = behavior.GetSettings();
    settings.autonomousWalking = true;
    settings.states[WalkStateIndex].speed = 2;
    behavior.SetSettings(settings);
    auto input = Moving();
    behavior.Update(0.1, input);
    Check(behavior.GetState() == PetState::Walk && animator.GetClipIndex() == 1 && animator.IsLooping() && animator.GetSpeed() == 2,
        "grounded travel enters looping walk at configured speed");
    behavior.Update(0.1, input);
    Check(std::abs(animator.GetTimeSeconds() - 0.4) < 0.0001, "walk continues without per-frame restart");
    settings.states[WalkStateIndex].speed = 0.5;
    behavior.SetSettings(settings);
    Check(std::abs(animator.GetTimeSeconds() - 0.4) < 0.0001 && animator.GetSpeed() == 0.5, "walk speed edit preserves phase");
    input.walking = false;
    behavior.Update(0.01, input);
    Check(behavior.GetState() == PetState::Idle && animator.GetClipIndex() == 0, "waiting and turning select idle");
    input = Moving(); input.dragging = true;
    behavior.Update(0.01, input);
    Check(behavior.GetState() == PetState::Dragged, "drag overrides walk intent");
    input = Moving(); input.grounded = false;
    behavior.Update(0.01, input);
    Check(behavior.GetState() == PetState::Falling, "fall overrides walk intent");
    input = Moving(); input.justLanded = true;
    behavior.Update(0.1, input);
    Check(behavior.GetState() == PetState::Landing, "landing overrides walk intent");
    input.justLanded = false;
    behavior.Update(0.1, input);
    Check(behavior.GetState() == PetState::Landing, "unfinished landing holds before walking");
    behavior.Update(0.9, input);
    behavior.Update(0.01, input);
    Check(behavior.GetState() == PetState::Walk, "walk can resume after landing completion");
    behavior.Preview(0);
    behavior.Update(0.1, input);
    Check(behavior.IsPreviewing() && animator.GetClipIndex() == 0, "preview overrides walk clip");
    behavior.StopPreview();
    behavior.Update(0.01, input);
    settings.states[WalkStateIndex].selection = ClipSelection::None;
    behavior.SetSettings(settings);
    Check(behavior.GetState() == PetState::Idle && !behavior.HasWalkAnimation(), "removing active walk immediately returns to idle");
    behavior.Update(0.01, input);
    Check(behavior.GetState() == PetState::Idle, "missing walk never slides with idle fallback");
    auto invalid = settings;
    invalid.states[WalkStateIndex].loop = false;
    bool rejected = false;
    try { behavior.SetSettings(invalid); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && behavior.GetSettings().states[WalkStateIndex].loop, "walk loop validation is atomic");
    invalid = settings;
    invalid.states[WalkStateIndex].selection = ClipSelection::Clip;
    invalid.states[WalkStateIndex].clipIndex = 4;
    rejected = false;
    try { behavior.SetSettings(invalid); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && behavior.GetSettings().states[WalkStateIndex].selection == ClipSelection::None,
        "forward root displacement cannot be assigned to autonomous walking");
    invalid = settings;
    invalid.wander.maximumWaitSeconds = -1;
    rejected = false;
    try { behavior.SetSettings(invalid); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && behavior.GetSettings().wander.maximumWaitSeconds == 8, "invalid wander profile is rejected atomically");

    const std::string file = "x64/Debug/walking-settings-test.txt";
    struct Cleanup { std::string path; ~Cleanup() { std::error_code ec; std::filesystem::remove(path, ec); } } cleanup{ file };
    {
        std::ofstream legacy(file);
        legacy << "desktop_pet_animation 1\n";
        for (const char* path : { "Asset/Models/Old One.fbx", "Asset/Models/Old Two.fbx" }) {
            legacy << "model " << std::quoted(ModelKey(path)) << "\ntransition 0.25\n";
            legacy << "state 0 2 1.5 1 \"Rig|Idle\" 0\nstate 1 1 2 0 \"\" 0\n";
            legacy << "state 2 0 0.8 1 \"\" 0\nstate 3 2 1 0 \"Rig|Land\" 0\nend\n";
        }
    }
    AnimationSettingsStore store(file);
    std::string error;
    Check(store.Load(error), "version one profiles remain readable");
    auto restored = store.Restore("Asset/Models/Old One.fbx", clips);
    Check(restored.found && !restored.settings.autonomousWalking && restored.settings.states[0].clipIndex == 0 &&
        restored.settings.states[0].speed == 1.5 && restored.settings.states[1].selection == ClipSelection::None &&
        restored.settings.states[WalkStateIndex].selection == ClipSelection::Automatic,
        "legacy load preserves four states and adds disabled default walking");
    settings = restored.settings;
    settings.autonomousWalking = true;
    settings.states[WalkStateIndex].selection = ClipSelection::Clip;
    settings.states[WalkStateIndex].clipIndex = 5;
    settings.states[WalkStateIndex].speed = 1.75;
    settings.wander.pixelsPerSecond = 93;
    settings.wander.minimumWaitSeconds = 1;
    settings.wander.maximumWaitSeconds = 4;
    settings.wander.minimumMoveSeconds = 2;
    settings.wander.maximumMoveSeconds = 6;
    settings.wander.turnSeconds = 0.5;
    settings.wander.forwardYawRadians = 1;
    Check(store.Save("Asset/Models/Old One.fbx", settings, clips, error), "save upgrades legacy profiles to version two");
    { std::ifstream saved(file); std::string tag; int version = 0; saved >> tag >> version; Check(version == 2, "new file uses version two"); }
    AnimationSettingsStore reopened(file);
    Check(reopened.Load(error), "version two profile reload");
    auto reordered = clips;
    std::swap(reordered[1], reordered[5]);
    restored = reopened.Restore("Asset/Models/Old One.fbx", reordered);
    Check(restored.settings.autonomousWalking && restored.settings.wander == settings.wander &&
        restored.settings.states[WalkStateIndex].clipIndex == 1 && restored.settings.states[WalkStateIndex].speed == 1.75,
        "walk identity, enable, speed and all timing/facing fields persist");
    Check(reopened.Restore("Asset/Models/Old Two.fbx", clips).settings.states[0].speed == 1.5,
        "upgrading one model preserves other legacy profiles");
    restored = reopened.Restore("Asset/Models/Old One.fbx", {});
    Check(restored.missingClips == 3 && restored.settings.states[WalkStateIndex].selection == ClipSelection::Automatic,
        "missing custom walk resets selection without dropping other profile values");
    const auto size = std::filesystem::file_size(file);
    invalid = settings;
    invalid.wander.pixelsPerSecond = std::numeric_limits<double>::infinity();
    Check(!reopened.Save("Asset/Models/Old One.fbx", invalid, clips, error) && std::filesystem::file_size(file) == size,
        "invalid walking profile cannot replace existing file");
    { std::ofstream malformed(file); malformed << "desktop_pet_animation 2\nmodel \"bad\"\ntransition 0.2\nwander 1 -3 1 4 2 6 0.5 0\n"; }
    Check(!reopened.Load(error) && reopened.Restore("Asset/Models/Old Two.fbx", clips).found,
        "invalid version two load preserves previous in-memory profiles");
}
