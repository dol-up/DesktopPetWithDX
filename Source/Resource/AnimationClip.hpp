#pragma once
#include <string>
#include <vector>

struct aiScene;

struct AnimationVector3 { float x, y, z; };
// x,y,z,w order; Assimp's quaternion constructor takes w,x,y,z.
struct AnimationQuaternion { float x, y, z, w; };

template<typename T>
struct AnimationKey {
    double timeSeconds;
    T value;
};

enum class AnimationBoundaryBehavior { DefaultPose, Constant, Linear, Repeat };

struct AnimationChannel {
    std::string nodeName;
    std::vector<AnimationKey<AnimationVector3>> positions;
    std::vector<AnimationKey<AnimationQuaternion>> rotations;
    std::vector<AnimationKey<AnimationVector3>> scales;
    AnimationBoundaryBehavior preBehavior = AnimationBoundaryBehavior::DefaultPose;
    AnimationBoundaryBehavior postBehavior = AnimationBoundaryBehavior::DefaultPose;
};

struct AnimationClip {
    unsigned int sourceIndex = 0; // Unique within a model, not across file revisions.
    std::string name;             // Original name; may be empty or duplicated.
    std::string displayName;
    double durationSeconds = 0.0;
    double ticksPerSecond = 0.0;  // Effective time conversion rate, not rendering FPS.
    bool usedDefaultTicksPerSecond = false;
    std::vector<AnimationChannel> channels;
    // Vertex/morph payloads are not loaded yet; callers can detect partial support.
    unsigned int unsupportedMeshChannelCount = 0;
    unsigned int unsupportedMorphChannelCount = 0;
};

// Application policy when the file does not declare its time unit.
constexpr double DefaultAnimationTicksPerSecond = 25.0;

// Deep copy, valid after importer destruction. Invalid data throws runtime_error.
std::vector<AnimationClip> LoadAnimationClips(const aiScene& scene);
