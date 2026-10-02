#include "AnimationClip.hpp"
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
    void Require(bool valid) {
        if (!valid) throw std::runtime_error("Invalid animation clip data");
    }

    AnimationVector3 CopyValue(const aiVector3D& value) {
        Require(std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z));
        return { value.x, value.y, value.z };
    }

    AnimationQuaternion CopyValue(const aiQuaternion& value) {
        Require(std::isfinite(value.x) && std::isfinite(value.y) &&
            std::isfinite(value.z) && std::isfinite(value.w));
        return { value.x, value.y, value.z, value.w };
    }

    AnimationBoundaryBehavior CopyBehavior(aiAnimBehaviour behavior) {
        switch (behavior) {
        case aiAnimBehaviour_DEFAULT: return AnimationBoundaryBehavior::DefaultPose;
        case aiAnimBehaviour_CONSTANT: return AnimationBoundaryBehavior::Constant;
        case aiAnimBehaviour_LINEAR: return AnimationBoundaryBehavior::Linear;
        case aiAnimBehaviour_REPEAT: return AnimationBoundaryBehavior::Repeat;
        default: throw std::runtime_error("Unknown animation boundary behavior");
        }
    }

    template<typename SourceKey, typename Value>
    void CopyKeys(const SourceKey* source, unsigned int count, double ticksPerSecond,
        std::vector<AnimationKey<Value>>& destination, double& durationSeconds) {
        Require(count == 0 || source != nullptr);
        destination.reserve(count);
        double previousTime = -std::numeric_limits<double>::infinity();
        for (unsigned int i = 0; i < count; ++i) {
            const double time = source[i].mTime / ticksPerSecond;
            Require(std::isfinite(time) && time >= previousTime);
            destination.push_back({ time, CopyValue(source[i].mValue) });
            previousTime = time;
            durationSeconds = std::max(durationSeconds, time);
        }
    }
}

std::vector<AnimationClip> LoadAnimationClips(const aiScene& scene) {
    Require(scene.mNumAnimations == 0 || scene.mAnimations != nullptr);
    std::vector<AnimationClip> clips;
    clips.reserve(scene.mNumAnimations);
    for (unsigned int i = 0; i < scene.mNumAnimations; ++i) {
        Require(scene.mAnimations[i] != nullptr);
        const aiAnimation& source = *scene.mAnimations[i];
        Require(std::isfinite(source.mDuration) && std::isfinite(source.mTicksPerSecond));
        Require(source.mTicksPerSecond >= 0.0);
        Require(source.mNumChannels == 0 || source.mChannels != nullptr);
        AnimationClip clip;
        clip.sourceIndex = i;
        clip.name = source.mName.C_Str();
        clip.displayName = clip.name.empty() ? "Animation " + std::to_string(i + 1) : clip.name;
        clip.usedDefaultTicksPerSecond = source.mTicksPerSecond == 0.0;
        clip.ticksPerSecond = clip.usedDefaultTicksPerSecond
            ? DefaultAnimationTicksPerSecond : source.mTicksPerSecond;
        clip.durationSeconds = std::max(0.0, source.mDuration) / clip.ticksPerSecond;
        Require(std::isfinite(clip.durationSeconds));
        clip.unsupportedMeshChannelCount = source.mNumMeshChannels;
        clip.unsupportedMorphChannelCount = source.mNumMorphMeshChannels;
        clip.channels.reserve(source.mNumChannels);
        for (unsigned int c = 0; c < source.mNumChannels; ++c) {
            Require(source.mChannels[c] != nullptr);
            const aiNodeAnim& sourceChannel = *source.mChannels[c];
            AnimationChannel channel;
            channel.nodeName = sourceChannel.mNodeName.C_Str();
            channel.preBehavior = CopyBehavior(sourceChannel.mPreState);
            channel.postBehavior = CopyBehavior(sourceChannel.mPostState);
            CopyKeys(sourceChannel.mPositionKeys, sourceChannel.mNumPositionKeys,
                clip.ticksPerSecond, channel.positions, clip.durationSeconds);
            CopyKeys(sourceChannel.mRotationKeys, sourceChannel.mNumRotationKeys,
                clip.ticksPerSecond, channel.rotations, clip.durationSeconds);
            CopyKeys(sourceChannel.mScalingKeys, sourceChannel.mNumScalingKeys,
                clip.ticksPerSecond, channel.scales, clip.durationSeconds);
            clip.channels.push_back(std::move(channel));
        }
        clips.push_back(std::move(clip));
    }
    return clips;
}
