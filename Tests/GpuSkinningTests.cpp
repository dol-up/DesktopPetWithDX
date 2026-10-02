#include "Skinning.hpp"
#include "Shader.hpp"
#include "VertexBuffer.hpp"
#include <d3dcompiler.h>
#include <wrl.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
using Microsoft::WRL::ComPtr;
void Check(bool condition, const char* message);

void TestGpuSkinning(const SkinningGeometry& source, const std::vector<SkeletonMatrix>& palette) {
    SkinningGeometry geometry;
    // Spread samples across all meshes, including vertices beyond the old 16-bit range.
    const std::size_t sampleCount = std::min<std::size_t>(source.vertices.size(), 257);
    for (std::size_t i = 0; i < sampleCount; ++i)
        geometry.vertices.push_back(source.vertices[i * source.vertices.size() / sampleCount]);
    if (geometry.vertices.empty()) return;
    auto expected = SkinVertices(geometry, palette);
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, &feature, &context)), "create WARP device");
    Shader shader(device.Get(), nullptr, L"Asset/Shaders/Shader.hlsl");
    shader.Bind(context.Get());
    ComPtr<ID3DBlob> code, errors;
    Check(SUCCEEDED(D3DCompileFromFile(L"Asset/Shaders/Shader.hlsl", nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", 0, 0, &code, &errors)), "compile actual vertex shader");
    D3D11_SO_DECLARATION_ENTRY output = { 0, "SV_POSITION", 0, 0, 4, 0 };
    const UINT outputStride = sizeof(float) * 4;
    ComPtr<ID3D11GeometryShader> streamShader;
    Check(SUCCEEDED(device->CreateGeometryShaderWithStreamOutput(code->GetBufferPointer(), code->GetBufferSize(),
        &output, 1, &outputStride, 1, D3D11_SO_NO_RASTERIZED_STREAM, nullptr, &streamShader)), "create stream-output shader");
    context->GSSetShader(streamShader.Get(), nullptr, 0);
    VertexBuffer vertices(device.Get(), geometry.vertices.data(), static_cast<UINT>(geometry.vertices.size()));
    vertices.Bind(context.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = sizeof(identity);
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA initial{};
    initial.pSysMem = identity;
    ComPtr<ID3D11Buffer> transform;
    Check(SUCCEEDED(device->CreateBuffer(&desc, &initial, &transform)), "create identity MVP");
    context->VSSetConstantBuffers(0, 1, transform.GetAddressOf());
    desc.ByteWidth = static_cast<UINT>(palette.size() * sizeof(SkeletonMatrix));
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(SkeletonMatrix);
    initial.pSysMem = palette.data();
    ComPtr<ID3D11Buffer> matrices;
    Check(SUCCEEDED(device->CreateBuffer(&desc, &initial, &matrices)), "create palette");
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = static_cast<UINT>(palette.size());
    ComPtr<ID3D11ShaderResourceView> srv;
    Check(SUCCEEDED(device->CreateShaderResourceView(matrices.Get(), &srvDesc, &srv)), "create palette view");
    context->VSSetShaderResources(0, 1, srv.GetAddressOf());
    desc = {};
    desc.ByteWidth = static_cast<UINT>(geometry.vertices.size()) * outputStride;
    desc.BindFlags = D3D11_BIND_STREAM_OUTPUT;
    ComPtr<ID3D11Buffer> outputBuffer, staging;
    Check(SUCCEEDED(device->CreateBuffer(&desc, nullptr, &outputBuffer)), "create stream buffer");
    desc.BindFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Check(SUCCEEDED(device->CreateBuffer(&desc, nullptr, &staging)), "create readback");
    UINT offset = 0;
    context->SOSetTargets(1, outputBuffer.GetAddressOf(), &offset);
    context->Draw(static_cast<UINT>(geometry.vertices.size()), 0);
    context->SOSetTargets(0, nullptr, nullptr);
    context->CopyResource(staging.Get(), outputBuffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "read GPU result");
    const auto* actual = static_cast<const float*>(mapped.pData);
    bool matches = true;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const float values[4] = { expected[i].x, expected[i].y, expected[i].z, 1 };
        for (unsigned int c = 0; c < 4; ++c)
            matches &= std::isfinite(actual[i * 4 + c]) &&
                std::abs(actual[i * 4 + c] - values[c]) <= 0.0001f * std::max(1.0f, std::abs(values[c]));
    }
    context->Unmap(staging.Get(), 0);
    Check(matches, "GPU vertex shader and CPU picking disagree");
}
