#pragma once

#define NOMINMAX

#include <windows.h>
#include <d3d11.h>
#include <wrl.h> 
#include <DirectXMath.h>
#include <memory>
#include <WICTextureLoader.h> 


#include "Shader.hpp"
#include "Camera.hpp"
#include "Model.hpp"

struct ModelScreenBounds {
    float left = 0;
    float right = 0;
    bool valid = false;
};

class Graphics {
public:
    Graphics(HWND hWnd, int width, int height, const std::string& initialModelPath,
        D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE);
    ~Graphics();

    // 복사 방지
    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;

    void Render();
    void UpdateAnimation(double deltaSeconds);
    void UpdateBehavior(double deltaSeconds, const PetBehaviorInput& input);
    const PetBehavior& GetBehavior() const { return model->GetBehavior(); }
    PetBehavior& GetBehavior() { return model->GetBehavior(); }
    const std::vector<AnimationClip>& GetAnimationClips() const { return model->GetAnimationClips(); }
    ID3D11Device* GetDevice() const { return device.Get(); }
    ID3D11DeviceContext* GetContext() const { return context.Get(); }
    Animator& GetAnimator() { return model->GetAnimator(); }

    void LoadNewModel(const std::string& filePath);
    bool HitTestModel(int clientX, int clientY) const;
    float GetModelBottomInClient() const;
    ModelScreenBounds GetWanderBoundsInClient() const;
    void SetAutonomousFacing(double yawRadians);
    double GetAutonomousFacing() const { return autonomousYaw; }
    void RotateModel(float deltaX, float deltaY);
    void ResetModelRotation();


private:
    friend void TestAutonomousMotion(const char* modelPath);
    void InvalidateGeometryCaches();
    mutable bool wanderBoundsCacheValid = false;
    mutable int wanderClientWidth = 0, wanderClientHeight = 0;
    mutable ModelScreenBounds wanderCachedBounds;
    mutable bool groundCacheValid = false;
    mutable int groundClientWidth = 0;
    mutable int groundClientHeight = 0;
    mutable float groundCachedBottom = 0;
    DirectX::XMMATRIX GetModelMatrix() const;

    HWND hWnd;
    int renderWidth;
    int renderHeight;

    // 다이렉트X 핵심 인터페이스 4인방
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;     // 깊이 값을 저장할 메모리 공간
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView; // 파이프라인에 꽂아줄 어댑터

    std::unique_ptr<Shader> shader;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<Model> model;

    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    DirectX::XMFLOAT4 modelRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    double autonomousYaw = 0;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;      // 이미지 돋보기(필터)

    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
};
