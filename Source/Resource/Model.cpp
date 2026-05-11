#include "Model.hpp"
#include <stdexcept>
#include <Windows.h>
#include <WICTextureLoader.h> // assimp였나 이 라이브러리 쓰면 걍 알아서 해준다고 해서 ㅇㅇ

Model::Model(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath) {
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string err = importer.GetErrorString();
        OutputDebugStringA(err.c_str());
        throw std::runtime_error("모델 로드 실패: " + err);
    }

    std::vector<Vertex> allVertices;
    std::vector<unsigned short> allIndices;
    unsigned int vertexOffset = 0;
    unsigned int indexOffset = 0;

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


    textures.resize(scene->mNumMaterials);

    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* material = scene->mMaterials[i];

        // FBX에서 재질(Material) 이름만 깔끔하게 뜯어옵니다.
        aiString aiMatName;
        material->Get(AI_MATKEY_NAME, aiMatName);
        std::string matName = aiMatName.C_Str();

        OutputDebugStringA(("----------------------------------------\n"));
        OutputDebugStringA(("이 부위의 재질 이름은 [" + matName + "] 입니다.\n").c_str());

        std::string fullPath = "Asset/Textures/" + matName + ".png";

        // 유니코드로 변환
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &fullPath[0], (int)fullPath.size(), NULL, 0);
        std::wstring wPath(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &fullPath[0], (int)fullPath.size(), &wPath[0], size_needed);

        // 로드 시도 및 결과 뱉어내기
        HRESULT hr = DirectX::CreateWICTextureFromFile(device, context, wPath.c_str(), nullptr, &textures[i]);

        if (FAILED(hr)) {
            OutputDebugStringA(("로드 실패: 폴더에 [" + matName + ".png] 파일이 있는지 확인\n").c_str());
        }
        else {
            OutputDebugStringA(("로드 성공: [" + matName + ".png] 장착\n").c_str());
        }
        OutputDebugStringA(("----------------------------------------\n"));
        
    }

    vertexBuffer = std::make_unique<VertexBuffer>(device, allVertices.data(), allVertices.size());
    indexBuffer = std::make_unique<IndexBuffer>(device, allIndices.data(), allIndices.size());
}

void Model::Draw(ID3D11DeviceContext* context) {
    vertexBuffer->Bind(context);
    indexBuffer->Bind(context);

    for (const auto& sm : subMesh) {
        if (sm.materialIndex < textures.size() && textures[sm.materialIndex] != nullptr) {
            context->PSSetShaderResources(0, 1, textures[sm.materialIndex].GetAddressOf());
        }
        context->DrawIndexed(sm.indexCount, sm.startIndexLocation, 0);
    }
}