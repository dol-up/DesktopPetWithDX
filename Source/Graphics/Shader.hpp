#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <string>

class Shader {
public:
    Shader(ID3D11Device* device, HWND hWnd, const std::wstring& shaderFile);

    void Bind(ID3D11DeviceContext* context);

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
};