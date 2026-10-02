#include "IndexBuffer.hpp"
#include <limits>
#include <stdexcept>

IndexBuffer::IndexBuffer(ID3D11Device* device, std::uint32_t* indices, UINT count)
    : indexCount(count)
{
    D3D11_BUFFER_DESC ibd = {};
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.Usage = D3D11_USAGE_DEFAULT;
    if (count == 0 || count > (std::numeric_limits<UINT>::max)() / sizeof(std::uint32_t))
        throw std::runtime_error("Invalid index buffer size");
    ibd.ByteWidth = sizeof(std::uint32_t) * count;
    ibd.StructureByteStride = sizeof(std::uint32_t);

    D3D11_SUBRESOURCE_DATA isd = {};
    isd.pSysMem = indices;

    if (FAILED(device->CreateBuffer(&ibd, &isd, &buffer)))
        throw std::runtime_error("Failed to create index buffer");
}

void IndexBuffer::Bind(ID3D11DeviceContext* context) {
    context->IASetIndexBuffer(buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
}

UINT IndexBuffer::GetCount() const {
    return indexCount;
}
