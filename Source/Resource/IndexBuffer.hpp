#pragma once
#include <d3d11.h>
#include <wrl.h>

class IndexBuffer {
public:
    IndexBuffer(ID3D11Device* device, unsigned short* indices, UINT count);
    void Bind(ID3D11DeviceContext* context);
    UINT GetCount() const;

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    UINT indexCount;
};