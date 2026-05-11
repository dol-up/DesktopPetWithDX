#include "VertexBuffer.hpp"

VertexBuffer::VertexBuffer(ID3D11Device* device, Vertex* vertices, UINT count) {
    stride = sizeof(Vertex);
    offset = 0;

    D3D11_BUFFER_DESC bd = {};
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = stride * count;
    bd.StructureByteStride = stride;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = vertices;

    device->CreateBuffer(&bd, &sd, &buffer);
}

void VertexBuffer::Bind(ID3D11DeviceContext* context) {
    context->IASetVertexBuffers(0, 1, buffer.GetAddressOf(), &stride, &offset);
}