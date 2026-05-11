#pragma once
#include <d3d11.h>
#include <string>
#include <vector>
#include <memory>
#include "VertexBuffer.hpp"
#include "IndexBuffer.hpp"
#include "Vertex.hpp"
#include <wrl.h>
#include <WICTextureLoader.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct SubMesh {
    unsigned int indexCount;        // 부위가 그릴 인덱스 개수
    unsigned int startIndexLocation; // 부위의 인덱스가 시작되는 위치
    unsigned int materialIndex;      // 부위가 사용할 텍스처 번호
};

class Model {
public:
    // 생성할 때 디바이스랑 파일 경로(fbx, obj 등)를 던져줍니다.
    Model(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath);
    void Draw(ID3D11DeviceContext* context);

private:
    std::unique_ptr<VertexBuffer> vertexBuffer;
    std::unique_ptr<IndexBuffer> indexBuffer;

    std::vector<SubMesh> subMesh;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;
};