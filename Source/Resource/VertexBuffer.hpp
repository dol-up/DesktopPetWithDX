#pragma once
#include <d3d11.h>
#include <wrl.h>
#include "Vertex.hpp"

class VertexBuffer {
public:
    VertexBuffer(ID3D11Device* device, Vertex* vertices, UINT count);
    void Bind(ID3D11DeviceContext* context);

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    UINT stride;
    UINT offset;
};