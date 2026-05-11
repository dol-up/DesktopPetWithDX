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

class Graphics {
public:
    Graphics(HWND hWnd, int width, int height);
    ~Graphics();

    // 복사 방지
    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;

    void Render();

private:
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

    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer; // 상수 버퍼 (GPU로 보낼 택배 상자)
    float rotationAngle = 0.0f; // 현재 회전 각도
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;      // 이미지 돋보기(필터)

    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
};