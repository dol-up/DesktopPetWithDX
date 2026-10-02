#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <memory>
#include <string>
#include "VertexBuffer.hpp"
#include "IndexBuffer.hpp"
#include "Skinning.hpp"
#include "PetBehavior.hpp"

class Model {
public:
    Model(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath);
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;
    void Draw(ID3D11DeviceContext* context);
    DirectX::XMMATRIX GetNormalizationMatrix() const;
    const std::vector<DirectX::XMFLOAT3>& GetPickVertices() const { EnsurePicking(); return pickVertices; }
    const std::vector<std::uint32_t>& GetPickIndices() const { return geometry.indices; }
    const DirectX::XMFLOAT3& GetBoundsMin() const { EnsurePicking(); return boundsMin; }
    const DirectX::XMFLOAT3& GetBoundsMax() const { EnsurePicking(); return boundsMax; }
    // Fixed bind geometry for physics: animation changes never move the ground reference.
    const std::vector<DirectX::XMFLOAT3>& GetGroundingVertices() const { return groundingVertices; }
    float GetGroundingBottom(const DirectX::XMMATRIX& worldViewProjection, float clientHeight) const;
    const std::vector<AnimationClip>& GetAnimationClips() const { return animationClips; }
    const Skeleton& GetSkeleton() const { return skeleton; }
    const AnimationNodeBindings& GetAnimationNodeBindings() const { return animationNodeBindings; }
    Animator& GetAnimator() { return *animator; }
    const Animator& GetAnimator() const { return *animator; }
    PetBehavior& GetBehavior() { return *behavior; }
    const PetBehavior& GetBehavior() const { return *behavior; }

private:
    void EnsurePalette() const;
    void EnsurePicking() const;
    std::vector<AnimationClip> animationClips;
    Skeleton skeleton;
    AnimationNodeBindings animationNodeBindings;
    std::unique_ptr<Animator> animator;
    std::unique_ptr<PetBehavior> behavior;
    SkinningGeometry geometry;
    std::unique_ptr<VertexBuffer> vertexBuffer;
    std::unique_ptr<IndexBuffer> indexBuffer;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;
    struct MaterialCB { int hasTexture; int padding[3]; };
    Microsoft::WRL::ComPtr<ID3D11Buffer> materialBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> skinningBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> skinningView;
    mutable std::vector<SkeletonMatrix> palette;
    mutable std::uint64_t paletteRevision = 0;
    mutable std::uint64_t pickRevision = 0;
    std::uint64_t gpuRevision = 0;
    mutable std::vector<DirectX::XMFLOAT3> pickVertices;
    std::vector<DirectX::XMFLOAT3> groundingVertices;
    mutable DirectX::XMFLOAT3 boundsMin{};
    mutable DirectX::XMFLOAT3 boundsMax{};
    float scaleFactor = 1;
    DirectX::XMFLOAT3 centerOffset{};
};
