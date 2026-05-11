#pragma once
#include <DirectXMath.h>

class Camera {
public:
    Camera(float windowWidth, float windowHeight);

    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix() const;

private:
    DirectX::XMVECTOR position;
    DirectX::XMVECTOR focus;
    DirectX::XMVECTOR up;
    float aspectRatio;
};