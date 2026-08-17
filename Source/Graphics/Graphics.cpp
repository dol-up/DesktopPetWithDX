#include "Graphics.hpp"
#include "Interaction/ModelPicker.hpp"
#include <d3dcompiler.h> // 셰이더 컴파일을 위해 추가
#include <algorithm>
#include <memory>


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

Graphics::Graphics(HWND hWnd, int width, int height, const std::string& initialModelPath)
    : hWnd(hWnd), renderWidth(width), renderHeight(height) {
    // 디바이스 및 스왑 체인 생성
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.SampleDesc.Count = 4;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 1;
    sd.OutputWindow = hWnd;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        nullptr, 0, D3D11_SDK_VERSION, &sd,
        &swapChain, &device, nullptr, &context
    );

    Microsoft::WRL::ComPtr<ID3D11Resource> backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Resource), &backBuffer);
    device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 24비트는 깊이, 8비트는 스텐실에 사용
    depthDesc.SampleDesc.Count = 4;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL; // 깊이 버퍼로 쓰겠다고 선언

    device->CreateTexture2D(&depthDesc, nullptr, &depthStencilBuffer);
    device->CreateDepthStencilView(depthStencilBuffer.Get(), nullptr, &depthStencilView);
    
    //depthbuffer 나중에 수정해야함
    D3D11_BUFFER_DESC cbd = {};
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(DirectX::XMMATRIX);
    cbd.CPUAccessFlags = 0;

    device->CreateBuffer(&cbd, nullptr, &constantBuffer);

    model = std::make_unique<Model>(device.Get(), context.Get(), initialModelPath);
    shader = std::make_unique<Shader>(device.Get(), hWnd, L"Asset/Shaders/Shader.hlsl");
    camera = std::make_unique<Camera>((float)width, (float)height);

    // 뷰포트(그려질 영역) 설정
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.MultisampleEnable = TRUE;     // 
    rasterDesc.AntialiasedLineEnable = TRUE; // 안티 앨리어싱 스위치

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterState;
    device->CreateRasterizerState(&rasterDesc, &rasterState);
    context->RSSetState(rasterState.Get());


    //샘플러 세팅 (이미지를 확대/축소할 때 어떻게 부드럽게 만들지 결정)
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    device->CreateSamplerState(&sampDesc, samplerState.GetAddressOf());

    // 알파 블렌딩을 위한 사전 작업
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    device->CreateBlendState(&blendDesc, blendState.GetAddressOf());
}

Graphics::~Graphics() {}

void Graphics::Render() {
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
    context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // 렌더링과 피킹이 동일한 모델 행렬을 사용한다.
    DirectX::XMMATRIX mModel = GetModelMatrix();

    // view, projection 곱하기
    DirectX::XMMATRIX mMVP = mModel * camera->GetViewMatrix() * camera->GetProjectionMatrix();

    DirectX::XMMATRIX cbData = DirectX::XMMatrixTranspose(mMVP);
    context->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &cbData, 0, 0);
    context->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    //
    context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());
    
    //
    shader->Bind(context.Get());
    context->PSSetSamplers(0, 1, samplerState.GetAddressOf());

    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(blendState.Get(), blendFactor, 0xffffffff);

    // 그리기 명령
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    model->Draw(context.Get());

    swapChain->Present(1, 0);
}

void Graphics::LoadNewModel(const std::string& filePath) {
    model.reset();
    model = std::make_unique<Model>(device.Get(), context.Get(), filePath);
}

bool Graphics::HitTestModel(int clientX, int clientY) const {
    if (!model || !camera) {
        return false;
    }

    RECT clientRect = {};
    if (!GetClientRect(hWnd, &clientRect)) {
        return false;
    }

    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return false;
    }

    // 고정된 렌더 크기가 현재 창 크기로 늘어나는 비율을 피킹 좌표에도 적용한다.
    const float renderX = static_cast<float>(clientX) * renderWidth / clientWidth;
    const float renderY = static_cast<float>(clientY) * renderHeight / clientHeight;

    return ModelPicker::HitTest(
        renderX,
        renderY,
        static_cast<float>(renderWidth),
        static_cast<float>(renderHeight),
        GetModelMatrix(),
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix(),
        model->GetPickVertices(),
        model->GetPickIndices(),
        model->GetBoundsMin(),
        model->GetBoundsMax());
}

float Graphics::GetModelBottomInClient() const {
    if (!model || !camera) {
        return static_cast<float>(renderHeight);
    }

    RECT clientRect = {};
    if (!GetClientRect(hWnd, &clientRect)) {
        return static_cast<float>(renderHeight);
    }

    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return static_cast<float>(renderHeight);
    }

    const DirectX::XMMATRIX worldViewProjection =
        GetModelMatrix() * camera->GetViewMatrix() * camera->GetProjectionMatrix();

    float maximumRenderY = 0.0f;
    for (const DirectX::XMFLOAT3& vertex : model->GetPickVertices()) {
        const DirectX::XMVECTOR projected = DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(&vertex),
            worldViewProjection);

        const float normalizedY = DirectX::XMVectorGetY(projected);
        const float renderY = (1.0f - normalizedY) * 0.5f * renderHeight;
        maximumRenderY = std::max(maximumRenderY, renderY);
    }

    const float clientBottom = maximumRenderY * clientHeight / renderHeight;
    return std::max(0.0f, std::min(clientBottom, static_cast<float>(clientHeight)));
}

void Graphics::RotateModel(float deltaX, float deltaY) {
    constexpr float yawSensitivity = 0.01f;
    constexpr float pitchSensitivity = 0.0075f;

    const DirectX::XMVECTOR pitchRotation = DirectX::XMQuaternionRotationAxis(
        DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
        -deltaY * pitchSensitivity);
    const DirectX::XMVECTOR yawRotation = DirectX::XMQuaternionRotationAxis(
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
        -deltaX * yawSensitivity);

    // DirectXMath는 Q1 다음 Q2 순서로 합성하므로 pitch 다음 yaw를 하나의 delta로 만든다.
    const DirectX::XMVECTOR deltaRotation = DirectX::XMQuaternionMultiply(pitchRotation, yawRotation);
    const DirectX::XMVECTOR currentRotation = DirectX::XMLoadFloat4(&modelRotation);
    const DirectX::XMVECTOR updatedRotation = DirectX::XMQuaternionNormalize(
        DirectX::XMQuaternionMultiply(currentRotation, deltaRotation));

    DirectX::XMStoreFloat4(&modelRotation, updatedRotation);
}

void Graphics::ResetModelRotation() {
    modelRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
}

DirectX::XMMATRIX Graphics::GetModelMatrix() const {
    if (!model) {
        return DirectX::XMMatrixIdentity();
    }

    const DirectX::XMMATRIX normalization = model->GetNormalizationMatrix();
    const DirectX::XMMATRIX baseRotation = DirectX::XMMatrixRotationX(1.5708f);
    const DirectX::XMMATRIX userRotation = DirectX::XMMatrixRotationQuaternion(
        DirectX::XMLoadFloat4(&modelRotation));

    return normalization * baseRotation * userRotation;
}
