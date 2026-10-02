#pragma once
#include "PetBehavior.hpp"
#include <map>
#include <string>

struct RestoredAnimationSettings {
    PetAnimationSettings settings;
    bool found = false;
    std::size_t missingClips = 0;
};

class AnimationSettingsStore {
public:
    explicit AnimationSettingsStore(std::string filePath = "animation_settings.txt") : filePath(std::move(filePath)) {}
    bool Load(std::string& error);
    RestoredAnimationSettings Restore(const std::string& modelPath, const std::vector<AnimationClip>& clips) const;
    bool Save(const std::string& modelPath, const PetAnimationSettings& settings,
        const std::vector<AnimationClip>& clips, std::string& error);
private:
    struct SavedState {
        ClipSelection selection = ClipSelection::Automatic;
        std::string clipName;
        std::size_t occurrence = 0;
        double speed = 1;
        bool loop = true;
    };
    struct SavedModel {
        std::array<SavedState, PetStateCount> states;
        double transitionSeconds = 0.15;
        bool autonomousWalking = false;
        WanderSettings wander;
        SavedModel() { states[3].loop = false; }
    };
    static std::string ModelKey(const std::string& path);
    std::string filePath;
    std::map<std::string, SavedModel> models;
};
