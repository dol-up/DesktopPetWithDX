#include "Animator.hpp"
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    void Require(bool valid, const char* message) {
        if (!valid) throw std::invalid_argument(message);
    }

    aiMatrix4x4 ToAssimp(const SkeletonMatrix& matrix) {
        const auto& v = matrix.elements;
        return aiMatrix4x4(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
            v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]);
    }

    SkeletonMatrix Multiply(const SkeletonMatrix& a, const SkeletonMatrix& b) {
        SkeletonMatrix result{};
        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c)
                for (std::size_t k = 0; k < 4; ++k)
                    result.elements[r * 4 + c] += a.elements[r * 4 + k] * b.elements[k * 4 + c];
        return result;
    }

    AnimationVector3 Blend(AnimationVector3 a, AnimationVector3 b, double t) {
        return { static_cast<float>(a.x + (b.x - a.x) * t),
            static_cast<float>(a.y + (b.y - a.y) * t), static_cast<float>(a.z + (b.z - a.z) * t) };
    }

    aiQuaternion Normalized(AnimationQuaternion value) {
        const double length = std::sqrt(static_cast<double>(value.x) * value.x +
            static_cast<double>(value.y) * value.y + static_cast<double>(value.z) * value.z +
            static_cast<double>(value.w) * value.w);
        Require(std::isfinite(length) && length > 0.0, "Invalid animation rotation");
        return aiQuaternion(static_cast<float>(value.w / length), static_cast<float>(value.x / length),
            static_cast<float>(value.y / length), static_cast<float>(value.z / length));
    }

    AnimationQuaternion Blend(AnimationQuaternion a, AnimationQuaternion b, double t) {
        aiQuaternion result;
        aiQuaternion::Interpolate(result, Normalized(a), Normalized(b), static_cast<float>(t));
        result.Normalize();
        return { result.x, result.y, result.z, result.w };
    }

    SkeletonMatrix BlendTransform(const SkeletonMatrix& a, const SkeletonMatrix& b, double factor) {
        // Preserve untouched nodes exactly, including any bind-pose shear.
        if (a.elements == b.elements) return b;
        aiVector3D sa, sb, pa, pb;
        aiQuaternion qa, qb, rotation;
        ToAssimp(a).Decompose(sa, qa, pa);
        ToAssimp(b).Decompose(sb, qb, pb);
        const float t = static_cast<float>(factor);
        const auto scale = sa + (sb - sa) * t;
        const auto position = pa + (pb - pa) * t;
        aiQuaternion::Interpolate(rotation, qa, qb, t);
        rotation.Normalize();
        const auto r = rotation.GetMatrix();
        return { { r.a1 * scale.x, r.a2 * scale.y, r.a3 * scale.z, position.x,
            r.b1 * scale.x, r.b2 * scale.y, r.b3 * scale.z, position.y,
            r.c1 * scale.x, r.c2 * scale.y, r.c3 * scale.z, position.z, 0, 0, 0, 1 } };
    }

    template<typename T>
    T Sample(const std::vector<AnimationKey<T>>& keys, double time, T fallback,
        AnimationBoundaryBehavior pre, AnimationBoundaryBehavior post) {
        if (keys.empty()) return fallback;
        const double first = keys.front().timeSeconds;
        const double last = keys.back().timeSeconds;
        const bool outside = time < first || time > last;
        if (outside) {
            const auto behavior = time < first ? pre : post;
            if (behavior == AnimationBoundaryBehavior::DefaultPose) return fallback;
            if (behavior == AnimationBoundaryBehavior::Constant)
                return time < first ? keys.front().value : keys.back().value;
            if (behavior == AnimationBoundaryBehavior::Repeat && last > first) {
                double offset = std::fmod(time - first, last - first);
                if (offset < 0) offset += last - first;
                time = first + offset;
            }
        }
        if (keys.size() == 1 || first == last) return keys.back().value;
        std::size_t left, right;
        if (time < first) {
            left = 0;
            right = 1;
            while (right < keys.size() && keys[right].timeSeconds == first) ++right;
        } else if (time > last) {
            right = keys.size() - 1;
            left = right - 1;
            while (left > 0 && keys[left].timeSeconds == last) --left;
        } else {
            auto upper = std::upper_bound(keys.begin(), keys.end(), time,
                [](double t, const AnimationKey<T>& key) { return t < key.timeSeconds; });
            if (upper == keys.end()) return keys.back().value;
            right = static_cast<std::size_t>(upper - keys.begin());
            left = right - 1; // Last key wins for equal timestamps.
        }
        const double factor = (time - keys[left].timeSeconds) /
            (keys[right].timeSeconds - keys[left].timeSeconds);
        return Blend(keys[left].value, keys[right].value, factor);
    }

    SkeletonMatrix SampleChannel(const SkeletonMatrix& bind, const AnimationChannel& channel, double time) {
        if (channel.positions.empty() && channel.rotations.empty() && channel.scales.empty()) return bind;
        aiVector3D scale, position;
        aiQuaternion rotation;
        ToAssimp(bind).Decompose(scale, rotation, position);
        const auto p = Sample(channel.positions, time, AnimationVector3{ position.x, position.y, position.z },
            channel.preBehavior, channel.postBehavior);
        const auto s = Sample(channel.scales, time, AnimationVector3{ scale.x, scale.y, scale.z },
            channel.preBehavior, channel.postBehavior);
        const auto q = Sample(channel.rotations, time,
            AnimationQuaternion{ rotation.x, rotation.y, rotation.z, rotation.w },
            channel.preBehavior, channel.postBehavior);
        const auto r = Normalized(q).GetMatrix();
        // Column-vector convention: T * R * S. Scale columns, not rows.
        return SkeletonMatrix{ { r.a1 * s.x, r.a2 * s.y, r.a3 * s.z, p.x,
            r.b1 * s.x, r.b2 * s.y, r.b3 * s.z, p.y,
            r.c1 * s.x, r.c2 * s.y, r.c3 * s.z, p.z, 0, 0, 0, 1 } };
    }
}

Animator::Animator(const Skeleton& skeleton, const std::vector<AnimationClip>& clips,
    const AnimationNodeBindings& bindings) : skeleton(skeleton), clips(clips), bindings(bindings) {
    Require(bindings.size() == clips.size(), "Invalid clip bindings");
    for (std::size_t n = 0; n < skeleton.nodes.size(); ++n)
        Require(skeleton.nodes[n].parentIndex == InvalidSkeletonNode || skeleton.nodes[n].parentIndex < n,
            "Skeleton must be ordered parent before child");
    for (std::size_t c = 0; c < clips.size(); ++c) {
        Require(bindings[c].size() == clips[c].channels.size(), "Invalid channel bindings");
        Require(std::isfinite(clips[c].durationSeconds) && clips[c].durationSeconds >= 0, "Invalid duration");
        for (auto node : bindings[c]) Require(node < skeleton.nodes.size(), "Invalid channel node");
    }
    pose.localTransforms.resize(skeleton.nodes.size());
    pose.globalTransforms.resize(skeleton.nodes.size());
    pose.boneTransforms.resize(skeleton.meshes.size());
    for (std::size_t m = 0; m < skeleton.meshes.size(); ++m) {
        pose.boneTransforms[m].resize(skeleton.meshes[m].bones.size());
        for (const auto& bone : skeleton.meshes[m].bones)
            Require(bone.nodeIndex < skeleton.nodes.size(), "Invalid bone node");
    }
    Evaluate();
}

void Animator::Play(std::size_t index, bool loop, bool restart) {
    if (index >= clips.size()) throw std::out_of_range("Animation clip index");
    Require(clips[index].unsupportedMeshChannelCount == 0 && clips[index].unsupportedMorphChannelCount == 0,
        "Mesh/morph animation playback is not supported");
    transitionSource.clear();
    paused = false;
    if (clipIndex != index || restart) timeSeconds = 0;
    clipIndex = index;
    looping = loop;
    finished = !looping && timeSeconds >= clips[index].durationSeconds;
    playing = !finished && clips[index].durationSeconds > 0;
    Evaluate();
}

void Animator::TransitionTo(std::size_t index, bool loop, double seconds, bool restart) {
    Require(std::isfinite(seconds) && seconds >= 0, "Invalid transition duration");
    const auto source = pose.localTransforms;
    if (index == InvalidSkeletonNode) Stop();
    else Play(index, loop, restart);
    if (seconds > 0 && !source.empty()) {
        transitionSource = source;
        transitionElapsed = 0;
        transitionDuration = seconds;
        Evaluate();
    }
}

void Animator::Resume() {
    paused = false;
    if (clipIndex != InvalidSkeletonNode && !finished && clips[clipIndex].durationSeconds > 0) playing = true;
}

void Animator::Stop() {
    transitionSource.clear();
    paused = false;
    clipIndex = InvalidSkeletonNode;
    timeSeconds = 0;
    playing = finished = false;
    Evaluate();
}

void Animator::Seek(double seconds) {
    Require(std::isfinite(seconds), "Invalid seek time");
    transitionSource.clear();
    if (clipIndex == InvalidSkeletonNode) return;
    timeSeconds = std::max(0.0, std::min(seconds, clips[clipIndex].durationSeconds));
    finished = !looping && timeSeconds >= clips[clipIndex].durationSeconds;
    if (finished) playing = false;
    Evaluate();
}

void Animator::SetSpeed(double multiplier) {
    Require(std::isfinite(multiplier) && multiplier >= 0, "Invalid playback speed");
    speed = multiplier;
}

void Animator::Update(double deltaSeconds) {
    Require(std::isfinite(deltaSeconds) && deltaSeconds >= 0, "Invalid frame time");
    if (paused || deltaSeconds == 0) return;
    bool changed = false;
    if (playing && speed > 0) {
        const double next = timeSeconds + deltaSeconds * speed;
        Require(std::isfinite(next), "Playback time overflow");
        const double duration = clips[clipIndex].durationSeconds;
        if (looping) timeSeconds = std::fmod(next, duration);
        else {
            timeSeconds = std::min(next, duration);
            if (timeSeconds >= duration) { finished = true; playing = false; }
        }
        changed = true;
    }
    if (IsTransitioning()) {
        transitionElapsed += deltaSeconds;
        if (transitionElapsed >= transitionDuration) transitionSource.clear();
        changed = true;
    }
    if (changed) Evaluate();
}

bool Animator::HasPlanarRootMotion(std::size_t index) const {
    if (index >= clips.size()) throw std::out_of_range("Animation clip index");
    std::vector<bool> candidates(skeleton.nodes.size(), false);
    for (const auto& mesh : skeleton.meshes)
        for (const auto& bone : mesh.bones) candidates[bone.nodeIndex] = true;
    if (std::none_of(candidates.begin(), candidates.end(), [](bool value) { return value; }))
        for (auto node : bindings[index]) candidates[node] = true;
    std::vector<std::size_t> roots;
    for (std::size_t node = 0; node < candidates.size(); ++node) {
        if (!candidates[node]) continue;
        auto parent = skeleton.nodes[node].parentIndex;
        while (parent != InvalidSkeletonNode && !candidates[parent]) parent = skeleton.nodes[parent].parentIndex;
        if (parent == InvalidSkeletonNode) roots.push_back(node);
    }
    const auto sampleGlobals = [&](double seconds) {
        std::vector<SkeletonMatrix> transforms;
        transforms.reserve(skeleton.nodes.size());
        for (const auto& node : skeleton.nodes) transforms.push_back(node.localBindTransform);
        for (std::size_t c = 0; c < clips[index].channels.size(); ++c) {
            const auto node = bindings[index][c];
            transforms[node] = SampleChannel(skeleton.nodes[node].localBindTransform, clips[index].channels[c], seconds);
        }
        for (std::size_t n = 0; n < transforms.size(); ++n) {
            const auto parent = skeleton.nodes[n].parentIndex;
            if (parent != InvalidSkeletonNode) transforms[n] = Multiply(transforms[parent], transforms[n]);
        }
        return transforms;
    };
    const auto start = sampleGlobals(0), end = sampleGlobals(clips[index].durationSeconds);
    for (auto root : roots) {
        const auto& a = start[root].elements;
        const auto& b = end[root].elements;
        double scale = 0;
        for (int column = 0; column < 3; ++column)
            scale = std::max(scale, std::sqrt(static_cast<double>(a[column]) * a[column] +
                static_cast<double>(a[4 + column]) * a[4 + column] + static_cast<double>(a[8 + column]) * a[8 + column]));
        const double tolerance = std::max(1e-5, scale * 0.005);
        if (std::hypot(static_cast<double>(b[3]) - a[3], static_cast<double>(b[11]) - a[11]) > tolerance) return true;
    }
    return false;
}

void Animator::Evaluate() {
    ++poseRevision;
    for (std::size_t n = 0; n < skeleton.nodes.size(); ++n)
        pose.localTransforms[n] = skeleton.nodes[n].localBindTransform;
    if (clipIndex != InvalidSkeletonNode) {
        const auto& channels = clips[clipIndex].channels;
        for (std::size_t c = 0; c < channels.size(); ++c) {
            const auto node = bindings[clipIndex][c];
            pose.localTransforms[node] = SampleChannel(skeleton.nodes[node].localBindTransform,
                channels[c], timeSeconds);
        }
    }
    if (IsTransitioning()) {
        const double t = std::min(1.0, transitionElapsed / transitionDuration);
        for (std::size_t n = 0; n < skeleton.nodes.size(); ++n)
            pose.localTransforms[n] = BlendTransform(transitionSource[n], pose.localTransforms[n], t);
    }
    for (std::size_t n = 0; n < skeleton.nodes.size(); ++n) {
        const auto parent = skeleton.nodes[n].parentIndex;
        pose.globalTransforms[n] = parent == InvalidSkeletonNode ? pose.localTransforms[n]
            : Multiply(pose.globalTransforms[parent], pose.localTransforms[n]);
    }
    for (std::size_t m = 0; m < skeleton.meshes.size(); ++m)
        for (std::size_t b = 0; b < skeleton.meshes[m].bones.size(); ++b) {
            const auto& bone = skeleton.meshes[m].bones[b];
            pose.boneTransforms[m][b] = Multiply(pose.globalTransforms[bone.nodeIndex], bone.offsetMatrix);
        }
}
