#include "Shader.hpp"
#include <d3dcompiler.h>
#include "Vertex.hpp"
#include <cstddef>
#include <stdexcept>

Shader::Shader(ID3D11Device* device, HWND hWnd, const std::wstring& shaderFile) {
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

    // 버텍스 셰이더 컴파일
    HRESULT hrVS = D3DCompileFromFile(shaderFile.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hrVS)) {
        if (errorBlob) {
            MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "셰이더 컴파일 에러", MB_OK);
        }
        else {
            MessageBoxA(nullptr, "파일을 찾을 수 없습니다", "Error", MB_OK);
        }
        throw std::runtime_error("Vertex shader compilation failed");
    }

    // 픽셀 셰이더 컴파일
    HRESULT hrPS = D3DCompileFromFile(shaderFile.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hrPS)) {
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        MessageBox(hWnd, "픽셀 셰이더 컴파일 에러", "Error", MB_OK);
        throw std::runtime_error("Pixel shader compilation failed");
    }

    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader)) ||
        FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader)))
        throw std::runtime_error("Failed to create shaders");

    // 인풋 레이아웃 생성
    D3D11_INPUT_ELEMENT_DESC ied[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, static_cast<UINT>(offsetof(Vertex, boneIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex, boneWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    if (FAILED(device->CreateInputLayout(ied, ARRAYSIZE(ied), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout)))
        throw std::runtime_error("Failed to create skinned vertex input layout");
}

void Shader::Bind(ID3D11DeviceContext* context) {
    context->IASetInputLayout(inputLayout.Get());
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);
}
