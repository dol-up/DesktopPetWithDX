#include "Model.hpp"
#include <stdexcept>
#include <Windows.h>
#include <WICTextureLoader.h> // assimp였나 이 라이브러리 쓰면 걍 알아서 해준다고 해서 ㅇㅇ
#include <cassert>

Model::Model(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath) {
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded | //assimp는 right_handed 좌표계를 쓴다 그래서 convert
        aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string err = importer.GetErrorString();
        OutputDebugStringA(err.c_str());
        assert(false);
    }

    std::vector<Vertex> allVertices;
    std::vector<unsigned short> allIndices;
    unsigned int vertexOffset = 0;
    unsigned int indexOffset = 0;


    float minX = 1e9f, minY = 1e9f, minZ = 1e9f;
    float maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;

    // 1. 메쉬 파싱
    for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
        aiMesh* mesh = scene->mMeshes[m];

        SubMesh sm;
        sm.startIndexLocation = indexOffset;
        sm.materialIndex = mesh->mMaterialIndex;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex v;
            v.x = mesh->mVertices[i].x;
            v.y = mesh->mVertices[i].y;
            v.z = mesh->mVertices[i].z;
            v.r = 1.0f; v.g = 1.0f; v.b = 1.0f; v.a = 1.0f;

            minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
            minZ = std::min(minZ, v.z); maxZ = std::max(maxZ, v.z);

            if (mesh->HasTextureCoords(0)) {
                v.u = mesh->mTextureCoords[0][i].x;
                v.v = mesh->mTextureCoords[0][i].y;
            }
            else {
                v.u = 0.0f; v.v = 0.0f;
            }
            allVertices.push_back(v);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                allIndices.push_back(face.mIndices[j] + vertexOffset);
                indexOffset++;
            }
        }

        sm.indexCount = indexOffset - sm.startIndexLocation;
        subMesh.push_back(sm);

        vertexOffset += mesh->mNumVertices;

    }
    
    float width = maxX - minX + 1.0f;
    float height = maxY - minY;
    float depth = maxZ - minZ;

    float maxDim = std::max({ width, height, depth });
    if (maxDim == 0.0f) maxDim = 1.0f; // divide-by-zero 방지

    // 모델 크기 화면에 맞게 조절 (크기 이상하면 조절)
    scaleFactor = 1.5f / maxDim;

    // 모델 아래를 바닥에
    centerOffset.x = -(minX + maxX) / 2.0f;
    centerOffset.y = -(minY + maxY) / 2.0f;
    centerOffset.z = -(minZ + maxZ) / 2.0f;

    textures.resize(scene->mNumMaterials);

    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* material = scene->mMaterials[i];

        aiString texPathStr;
        bool isEmbeddedLoaded = false;

        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPathStr) == AI_SUCCESS ||
            material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPathStr) == AI_SUCCESS) {

            const aiTexture* embeddedTex = scene->GetEmbeddedTexture(texPathStr.C_Str());

            if (embeddedTex != nullptr) {
                // Assimp에서 mHeight가 0이면 압축된 데이터이고, mWidth가 파일의 바이트 크기
                if (embeddedTex->mHeight == 0) {
                    HRESULT hr = DirectX::CreateWICTextureFromMemory(
                        device, context,
                        reinterpret_cast<const uint8_t*>(embeddedTex->pcData),
                        embeddedTex->mWidth,
                        nullptr,
                        &textures[i]
                    );

                    if (SUCCEEDED(hr)) {
                        OutputDebugStringA(("FBX 내장 텍스처[" + std::string(texPathStr.C_Str()) + "] 로드 성공\n").c_str());
                        isEmbeddedLoaded = true;
                    }
                }
                else {
                    OutputDebugStringA("경고: 압축되지 않은 원시 텍스처 포맷입니다!\n");
                }
            }
        }

        if (!isEmbeddedLoaded) {
            aiString aiMatName;
            material->Get(AI_MATKEY_NAME, aiMatName);
            std::string matName = aiMatName.C_Str();

            std::string fullPath = "Asset/Textures/" + matName + ".png";

            int size_needed = MultiByteToWideChar(CP_UTF8, 0, &fullPath[0], (int)fullPath.size(), NULL, 0);
            std::wstring wPath(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, &fullPath[0], (int)fullPath.size(), &wPath[0], size_needed);

            HRESULT hr = DirectX::CreateWICTextureFromFile(device, context, wPath.c_str(), nullptr, &textures[i]);

            if (FAILED(hr)) {
                OutputDebugStringA(("로드 실패: 내장도 없고, 폴더에 [" + matName + ".png]도 없음\n").c_str());
            }
            else {
                OutputDebugStringA(("로드 성공: 외부 폴더에서 [" + matName + ".png] 장착\n").c_str());
            }
        }
    }

    D3D11_BUFFER_DESC mbDesc = {};
    mbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mbDesc.Usage = D3D11_USAGE_DEFAULT;
    mbDesc.ByteWidth = sizeof(MaterialCB);

    device->CreateBuffer(&mbDesc, nullptr, &materialBuffer); //텍스쳐 있는지 없는지 정보 전달해줄 버퍼

    vertexBuffer = std::make_unique<VertexBuffer>(device, allVertices.data(), allVertices.size());
    indexBuffer = std::make_unique<IndexBuffer>(device, allIndices.data(), allIndices.size());
}

void Model::Draw(ID3D11DeviceContext* context) {
    vertexBuffer->Bind(context);
    indexBuffer->Bind(context);

    for (const auto& sm : subMesh) {
        // 텍스쳐 존재 확인
        bool hasTexture = (sm.materialIndex < textures.size() && textures[sm.materialIndex] != nullptr);

        if (hasTexture) {
            context->PSSetShaderResources(0, 1, textures[sm.materialIndex].GetAddressOf());
        }
        else {
            //  빈 텍스처를 꽂아서 초기화
            ID3D11ShaderResourceView* nullSRV = nullptr;
            context->PSSetShaderResources(0, 1, &nullSRV);
        }

        // 셰이더에게 "텍스처 유무" 신호(Constant Buffer) 업데이트 및 전송
        MaterialCB cbData;
        cbData.hasTexture = hasTexture ? 1 : 0;
        context->UpdateSubresource(materialBuffer.Get(), 0, nullptr, &cbData, 0, 0);
        context->PSSetConstantBuffers(0, 1, materialBuffer.GetAddressOf()); // 픽셀 셰이더의 b0 슬롯에 연결
        
        context->DrawIndexed(sm.indexCount, sm.startIndexLocation, 0);
    }
}

DirectX::XMMATRIX Model::GetNormalizationMatrix() const {
    // 모델 (0,0,0)으로 끌고 옴
    DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(centerOffset.x, centerOffset.y, centerOffset.z);

    // 화면에 맞춰 스케일링
    DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor);

    // 이동 -> 축소 순서
    return translation * scaling;
}