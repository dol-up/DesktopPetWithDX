#include "Camera.hpp"

Camera::Camera(float windowWidth, float windowHeight) {
    // 카메라 초기 위치
    position = DirectX::XMVectorSet(0.0f, 0.5f, -3.5f, 1.0f);
    focus = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    aspectRatio = windowWidth / windowHeight;
}

DirectX::XMMATRIX Camera::GetViewMatrix() const {
    return DirectX::XMMatrixLookAtLH(position, focus, up);
}

DirectX::XMMATRIX Camera::GetProjectionMatrix() const {
    // 45도 시야각, 가까운 한계선 0.1f, 먼 한계선 100.0f
    return DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(45.0f), aspectRatio, 0.1f, 100.0f
    );
}