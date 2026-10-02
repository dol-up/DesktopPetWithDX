#include "AnimationSettingsStore.hpp"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
    void Require(bool valid) { if (!valid) throw std::runtime_error("Invalid animation settings file"); }
    bool Supported(const AnimationClip& clip) {
        return !clip.channels.empty() && !clip.unsupportedMeshChannelCount && !clip.unsupportedMorphChannelCount;
    }
    bool ValidSpeed(double value) { return std::isfinite(value) && value >= 0.05 && value <= 4; }
}

std::string AnimationSettingsStore::ModelKey(const std::string& path) {
    auto key = std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().generic_u8string();
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return key;
}

bool AnimationSettingsStore::Load(std::string& error) {
    error.clear();
    try {
        if (!std::filesystem::exists(filePath)) { models.clear(); return true; }
        std::ifstream input(filePath);
        Require(input.is_open());
        std::string tag;
        int version = 0;
        Require(static_cast<bool>(input >> tag >> version) && tag == "desktop_pet_animation" && (version == 1 || version == 2));
        std::map<std::string, SavedModel> loaded;
        while (input >> tag) {
            Require(tag == "model");
            std::string key;
            Require(static_cast<bool>(input >> std::quoted(key)) && !key.empty());
            SavedModel model;
            Require(static_cast<bool>(input >> tag >> model.transitionSeconds) && tag == "transition" &&
                std::isfinite(model.transitionSeconds) && model.transitionSeconds >= 0 && model.transitionSeconds <= 2);
            if (version == 2) {
                int enabled = -1;
                auto& w = model.wander;
                Require(static_cast<bool>(input >> tag >> enabled >> w.pixelsPerSecond >> w.minimumWaitSeconds >>
                    w.maximumWaitSeconds >> w.minimumMoveSeconds >> w.maximumMoveSeconds >> w.turnSeconds >> w.forwardYawRadians) &&
                    tag == "wander" && (enabled == 0 || enabled == 1));
                WanderController::ValidateSettings(w);
                model.autonomousWalking = enabled != 0;
            }
            const int stateCount = version == 1 ? 4 : static_cast<int>(PetStateCount);
            std::array<bool, PetStateCount> seen{};
            for (int row = 0; row < stateCount; ++row) {
                int state = -1, mode = -1, loop = -1;
                SavedState value;
                Require(static_cast<bool>(input >> tag >> state >> mode >> value.speed >> loop >>
                    std::quoted(value.clipName) >> value.occurrence));
                Require(tag == "state" && state >= 0 && state < stateCount && !seen[state] && mode >= 0 && mode <= 2 &&
                    ValidSpeed(value.speed) && (loop == 0 || loop == 1) && (state != 3 || loop == 0) &&
                    (state != static_cast<int>(WalkStateIndex) || loop == 1));
                value.selection = static_cast<ClipSelection>(mode);
                value.loop = loop != 0;
                model.states[state] = value;
                seen[state] = true;
            }
            Require(static_cast<bool>(input >> tag) && tag == "end");
            loaded[key] = std::move(model);
        }
        Require(input.eof());
        models = std::move(loaded);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

RestoredAnimationSettings AnimationSettingsStore::Restore(const std::string& modelPath,
    const std::vector<AnimationClip>& clips) const {
    RestoredAnimationSettings result;
    const auto found = models.find(ModelKey(modelPath));
    if (found == models.end()) return result;
    result.found = true;
    result.settings.transitionSeconds = found->second.transitionSeconds;
    result.settings.autonomousWalking = found->second.autonomousWalking;
    result.settings.wander = found->second.wander;
    for (std::size_t state = 0; state < PetStateCount; ++state) {
        const auto& saved = found->second.states[state];
        auto& target = result.settings.states[state];
        target.selection = saved.selection;
        target.speed = saved.speed;
        target.loop = saved.loop;
        if (saved.selection != ClipSelection::Clip) continue;
        std::size_t occurrence = 0;
        for (std::size_t c = 0; c < clips.size(); ++c) {
            if (clips[c].name != saved.clipName) continue;
            if (occurrence++ == saved.occurrence) {
                if (Supported(clips[c]) && ((state != 3 && state != WalkStateIndex) || clips[c].durationSeconds > 0)) target.clipIndex = c;
                break;
            }
        }
        if (target.clipIndex == InvalidSkeletonNode) {
            target.selection = ClipSelection::Automatic;
            ++result.missingClips;
        }
    }
    return result;
}

bool AnimationSettingsStore::Save(const std::string& modelPath, const PetAnimationSettings& settings,
    const std::vector<AnimationClip>& clips, std::string& error) {
    error.clear();
    try {
        Require(std::isfinite(settings.transitionSeconds) && settings.transitionSeconds >= 0 && settings.transitionSeconds <= 2);
        SavedModel model;
        model.transitionSeconds = settings.transitionSeconds;
        WanderController::ValidateSettings(settings.wander);
        model.autonomousWalking = settings.autonomousWalking;
        model.wander = settings.wander;
        for (std::size_t state = 0; state < PetStateCount; ++state) {
            const auto& config = settings.states[state];
            auto& target = model.states[state];
            Require(ValidSpeed(config.speed) && (state != 3 || !config.loop) && (state != WalkStateIndex || config.loop));
            Require(config.selection == ClipSelection::Automatic || config.selection == ClipSelection::None || config.selection == ClipSelection::Clip);
            target.selection = config.selection;
            target.speed = config.speed;
            target.loop = config.loop;
            if (config.selection != ClipSelection::Clip) continue;
            Require(config.clipIndex < clips.size() && Supported(clips[config.clipIndex]) &&
                ((state != 3 && state != WalkStateIndex) || clips[config.clipIndex].durationSeconds > 0));
            target.clipName = clips[config.clipIndex].name;
            for (std::size_t c = 0; c < config.clipIndex; ++c)
                if (clips[c].name == target.clipName) ++target.occurrence;
        }
        auto updated = models;
        updated[ModelKey(modelPath)] = std::move(model);
        const std::filesystem::path target(filePath), temporary(filePath + ".tmp");
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot write animation settings");
        output << "desktop_pet_animation 2\n" << std::setprecision(17);
        for (const auto& entry : updated) {
            output << "model " << std::quoted(entry.first) << "\ntransition " << entry.second.transitionSeconds << '\n';
            const auto& w = entry.second.wander;
            output << "wander " << entry.second.autonomousWalking << ' ' << w.pixelsPerSecond << ' ' <<
                w.minimumWaitSeconds << ' ' << w.maximumWaitSeconds << ' ' << w.minimumMoveSeconds << ' ' <<
                w.maximumMoveSeconds << ' ' << w.turnSeconds << ' ' << w.forwardYawRadians << '\n';
            for (std::size_t state = 0; state < PetStateCount; ++state) {
                const auto& value = entry.second.states[state];
                output << "state " << state << ' ' << static_cast<int>(value.selection) << ' ' << value.speed << ' '
                    << value.loop << ' ' << std::quoted(value.clipName) << ' ' << value.occurrence << '\n';
            }
            output << "end\n";
        }
        output.flush();
        if (!output) throw std::runtime_error("Cannot flush animation settings");
        output.close();
        if (!output || !MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace animation settings file");
        models = std::move(updated);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}
