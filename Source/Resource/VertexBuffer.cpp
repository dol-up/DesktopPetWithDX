#include "VertexBuffer.hpp"
#include <limits>
#include <stdexcept>

VertexBuffer::VertexBuffer(ID3D11Device* device, Vertex* vertices, UINT count) {
    stride = sizeof(Vertex);
    offset = 0;
    if (count == 0 || count > (std::numeric_limits<UINT>::max)() / stride)
        throw std::runtime_error("Invalid vertex buffer size");

    D3D11_BUFFER_DESC bd = {};
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = stride * count;
    bd.StructureByteStride = stride;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = vertices;

    if (FAILED(device->CreateBuffer(&bd, &sd, &buffer)))
        throw std::runtime_error("Failed to create vertex buffer");
}

void VertexBuffer::Bind(ID3D11DeviceContext* context) {
    context->IASetVertexBuffers(0, 1, buffer.GetAddressOf(), &stride, &offset);
}
