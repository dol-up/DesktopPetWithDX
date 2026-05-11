#include "IndexBuffer.hpp"

IndexBuffer::IndexBuffer(ID3D11Device* device, unsigned short* indices, UINT count)
    : indexCount(count)
{
    D3D11_BUFFER_DESC ibd = {};
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(unsigned short) * count;
    ibd.StructureByteStride = sizeof(unsigned short);

    D3D11_SUBRESOURCE_DATA isd = {};
    isd.pSysMem = indices;

    device->CreateBuffer(&ibd, &isd, &buffer);
}

void IndexBuffer::Bind(ID3D11DeviceContext* context) {
    context->IASetIndexBuffer(buffer.Get(), DXGI_FORMAT_R16_UINT, 0);
}

UINT IndexBuffer::GetCount() const {
    return indexCount;
}