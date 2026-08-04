#include "ModelPicker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    DirectX::XMFLOAT3 Subtract(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    DirectX::XMFLOAT3 Cross(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    float Dot(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
}

bool ModelPicker::HitTest(
    float screenX,
    float screenY,
    float viewportWidth,
    float viewportHeight,
    const DirectX::XMMATRIX& world,
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& projection,
    const std::vector<DirectX::XMFLOAT3>& vertices,
    const std::vector<unsigned short>& indices,
    const DirectX::XMFLOAT3& boundsMin,
    const DirectX::XMFLOAT3& boundsMax) {

    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f || vertices.empty() || indices.size() < 3) {
        return false;
    }

    if (screenX < 0.0f || screenY < 0.0f || screenX >= viewportWidth || screenY >= viewportHeight) {
        return false;
    }

    using namespace DirectX;

    const XMVECTOR nearPoint = XMVector3Unproject(
        XMVectorSet(screenX, screenY, 0.0f, 1.0f),
        0.0f, 0.0f, viewportWidth, viewportHeight, 0.0f, 1.0f,
        projection, view, world);

    const XMVECTOR farPoint = XMVector3Unproject(
        XMVectorSet(screenX, screenY, 1.0f, 1.0f),
        0.0f, 0.0f, viewportWidth, viewportHeight, 0.0f, 1.0f,
        projection, view, world);

    const XMVECTOR rayVector = XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint));

    XMFLOAT3 rayOrigin;
    XMFLOAT3 rayDirection;
    XMStoreFloat3(&rayOrigin, nearPoint);
    XMStoreFloat3(&rayDirection, rayVector);

    if (!RayIntersectsAabb(rayOrigin, rayDirection, boundsMin, boundsMax)) {
        return false;
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const unsigned short indexA = indices[i];
        const unsigned short indexB = indices[i + 1];
        const unsigned short indexC = indices[i + 2];

        if (indexA >= vertices.size() || indexB >= vertices.size() || indexC >= vertices.size()) {
            continue;
        }

        if (RayIntersectsTriangle(
            rayOrigin,
            rayDirection,
            vertices[indexA],
            vertices[indexB],
            vertices[indexC])) {
            return true;
        }
    }

    return false;
}

bool ModelPicker::RayIntersectsAabb(
    const DirectX::XMFLOAT3& rayOrigin,
    const DirectX::XMFLOAT3& rayDirection,
    const DirectX::XMFLOAT3& boundsMin,
    const DirectX::XMFLOAT3& boundsMax) {

    constexpr float epsilon = 1.0e-7f;
    float nearest = 0.0f;
    float farthest = std::numeric_limits<float>::max();

    const float origins[3] = { rayOrigin.x, rayOrigin.y, rayOrigin.z };
    const float directions[3] = { rayDirection.x, rayDirection.y, rayDirection.z };
    const float minimums[3] = { boundsMin.x, boundsMin.y, boundsMin.z };
    const float maximums[3] = { boundsMax.x, boundsMax.y, boundsMax.z };

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(directions[axis]) < epsilon) {
            if (origins[axis] < minimums[axis] || origins[axis] > maximums[axis]) {
                return false;
            }
            continue;
        }

        const float inverseDirection = 1.0f / directions[axis];
        float first = (minimums[axis] - origins[axis]) * inverseDirection;
        float second = (maximums[axis] - origins[axis]) * inverseDirection;

        if (first > second) {
            std::swap(first, second);
        }

        nearest = std::max(nearest, first);
        farthest = std::min(farthest, second);

        if (nearest > farthest) {
            return false;
        }
    }

    return true;
}

bool ModelPicker::RayIntersectsTriangle(
    const DirectX::XMFLOAT3& rayOrigin,
    const DirectX::XMFLOAT3& rayDirection,
    const DirectX::XMFLOAT3& a,
    const DirectX::XMFLOAT3& b,
    const DirectX::XMFLOAT3& c) {

    constexpr float epsilon = 1.0e-7f;

    const DirectX::XMFLOAT3 edgeAB = Subtract(b, a);
    const DirectX::XMFLOAT3 edgeAC = Subtract(c, a);
    const DirectX::XMFLOAT3 perpendicular = Cross(rayDirection, edgeAC);
    const float determinant = Dot(edgeAB, perpendicular);

    if (std::abs(determinant) < epsilon) {
        return false;
    }

    const float inverseDeterminant = 1.0f / determinant;
    const DirectX::XMFLOAT3 originToA = Subtract(rayOrigin, a);
    const float u = Dot(originToA, perpendicular) * inverseDeterminant;

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    const DirectX::XMFLOAT3 crossValue = Cross(originToA, edgeAB);
    const float v = Dot(rayDirection, crossValue) * inverseDeterminant;

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    const float distance = Dot(edgeAC, crossValue) * inverseDeterminant;
    return distance >= 0.0f;
}
