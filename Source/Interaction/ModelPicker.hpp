#pragma once

#include <DirectXMath.h>
#include <vector>
#include <cstdint>

class ModelPicker {
public:
    static bool HitTest(
        float screenX,
        float screenY,
        float viewportWidth,
        float viewportHeight,
        const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const std::vector<DirectX::XMFLOAT3>& vertices,
        const std::vector<std::uint32_t>& indices,
        const DirectX::XMFLOAT3& boundsMin,
        const DirectX::XMFLOAT3& boundsMax);

private:
    static bool RayIntersectsAabb(
        const DirectX::XMFLOAT3& rayOrigin,
        const DirectX::XMFLOAT3& rayDirection,
        const DirectX::XMFLOAT3& boundsMin,
        const DirectX::XMFLOAT3& boundsMax);

    static bool RayIntersectsTriangle(
        const DirectX::XMFLOAT3& rayOrigin,
        const DirectX::XMFLOAT3& rayDirection,
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b,
        const DirectX::XMFLOAT3& c);
};
