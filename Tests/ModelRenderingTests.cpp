#include "Model.hpp"
#include "Shader.hpp"
#include <ScreenGrab.h>
#include <wincodec.h>
#include <d3d11sdklayers.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

#pragma comment(lib, "ole32.lib")
void Check(bool condition, const char* message);
using Microsoft::WRL::ComPtr;

void TestModelRendering(const char* path, int testIndex) {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_DEBUG,
        nullptr, 0, D3D11_SDK_VERSION, &device, &feature, &context)), "create debug WARP device");
    ComPtr<ID3D11InfoQueue> info;
    device.As(&info);
    Model model(device.Get(), context.Get(), path);
    Shader shader(device.Get(), nullptr, L"Asset/Shaders/Shader.hlsl");
    constexpr UINT size = 512;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = size;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target, depth, staging;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11DepthStencilView> dsv;
    Check(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &target)), "render target");
    Check(SUCCEEDED(device->CreateRenderTargetView(target.Get(), nullptr, &rtv)), "render target view");
    desc.BindFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Check(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging)), "image readback");
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    Check(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &depth)), "depth target");
    Check(SUCCEEDED(device->CreateDepthStencilView(depth.Get(), nullptr, &dsv)), "depth view");
    D3D11_VIEWPORT viewport = { 0, 0, static_cast<float>(size), static_cast<float>(size), 0, 1 };
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    Check(SUCCEEDED(device->CreateSamplerState(&samplerDesc, &sampler)), "sampler");
    context->PSSetSamplers(0, 1, sampler.GetAddressOf());
    using namespace DirectX;
    const auto view = XMMatrixLookAtLH(XMVectorSet(0, 0, -3.5f, 1), XMVectorZero(), XMVectorSet(0, 1, 0, 0));
    const auto projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(45), 1, 0.1f, 100);
    const auto wvp = model.GetNormalizationMatrix() * view * projection;
    const auto gpuWvp = XMMatrixTranspose(wvp);
    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(gpuWvp);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA initial{};
    initial.pSysMem = &gpuWvp;
    ComPtr<ID3D11Buffer> cb;
    Check(SUCCEEDED(device->CreateBuffer(&cbDesc, &initial, &cb)), "MVP buffer");
    context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    shader.Bind(context.Get());

    const float fixedBottom = model.GetGroundingBottom(wvp, size);
    Check(std::abs(model.GetGroundingBottom(wvp, size * 2) - fixedBottom * 2) < 0.001f, "ground scales with window");
    const auto rotatedWvp = model.GetNormalizationMatrix() * XMMatrixRotationX(0.4f) * view * projection;
    const float rotatedBottom = model.GetGroundingBottom(rotatedWvp, size);
    const auto fixedGround = model.GetGroundingVertices();
    std::vector<XMFLOAT3> firstPose;
    std::vector<unsigned char> firstImage;
    const auto selected = model.GetAnimator().GetClipIndex();
    for (int frame = 0; frame < 2; ++frame) {
        if (selected != InvalidSkeletonNode)
            model.GetAnimator().Seek(model.GetAnimationClips()[selected].durationSeconds * (frame == 0 ? 0.0 : 0.45));
        const auto currentPose = model.GetPickVertices();
        Check(model.GetGroundingBottom(wvp, size) == fixedBottom, "animation must not shift ground");
        Check(model.GetGroundingBottom(rotatedWvp, size) == rotatedBottom, "rotated ground remains stable");
        Check(std::memcmp(fixedGround.data(), model.GetGroundingVertices().data(), fixedGround.size() * sizeof(XMFLOAT3)) == 0,
            "grounding geometry is immutable");
        if (frame == 0) firstPose = currentPose;
        const float clear[4] = { 0.08f, 0.1f, 0.13f, 1 };
        context->ClearRenderTargetView(rtv.Get(), clear);
        context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1, 0);
        model.Draw(context.Get());
        context->CopyResource(staging.Get(), target.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "map render result");
        std::vector<unsigned char> pixels(size * size * 4);
        for (UINT y = 0; y < size; ++y)
            std::memcpy(pixels.data() + y * size * 4, static_cast<unsigned char*>(mapped.pData) + y * mapped.RowPitch, size * 4);
        context->Unmap(staging.Get(), 0);
        std::size_t changed = 0;
        for (std::size_t i = 4; i < pixels.size(); i += 4)
            if (std::memcmp(pixels.data(), pixels.data() + i, 3) != 0) ++changed;
        Check(changed > 100, "model produced visible pixels");
        if (frame == 0) firstImage = pixels;
        if (frame == 1 && selected != InvalidSkeletonNode) {
            Check(firstImage != pixels, "animated rendered pixels must change");
            Check(std::memcmp(firstPose.data(), currentPose.data(), firstPose.size() * sizeof(XMFLOAT3)) != 0,
                "animated picking vertices must change");
        }
        const std::wstring output = L"x64/Debug/skinning-preview-" + std::to_wstring(testIndex) + L"-" + std::to_wstring(frame) + L".png";
        Check(SUCCEEDED(SaveWICTextureToFile(context.Get(), target.Get(), GUID_ContainerFormatPng, output.c_str())), "save render preview");
    }
    if (info) {
        for (UINT64 i = 0; i < info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i) {
            SIZE_T length = 0;
            info->GetMessage(i, nullptr, &length);
            std::vector<unsigned char> storage(length);
            auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
            info->GetMessage(i, message, &length);
            if (message->Severity <= D3D11_MESSAGE_SEVERITY_ERROR) {
                std::cerr << message->pDescription << '\n';
                Check(false, "D3D11 debug validation error");
            }
        }
    }
    std::cout << "Offscreen render and stable grounding passed: " << path << '\n';
}
