#include "Model.hpp"
#include <stdexcept>
#include <Windows.h>
#include <WICTextureLoader.h> // assimp였나 이 라이브러리 쓰면 걍 알아서 해준다고 해서 ㅇㅇ
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <limits>

Model::Model(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath) {
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded | //assimp는 right_handed 좌표계를 쓴다 그래서 convert
        aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string err = importer.GetErrorString();
        OutputDebugStringA(err.c_str());
        throw std::runtime_error("Failed to load model: " + err);
    }

    animationClips = LoadAnimationClips(*scene);
    skeleton = LoadSkeleton(*scene);
    animationNodeBindings = BindAnimationChannels(skeleton, animationClips);
    animator = std::make_unique<Animator>(skeleton, animationClips, animationNodeBindings);
    OutputDebugStringA(("Skeleton nodes: " + std::to_string(skeleton.nodes.size()) + "\n").c_str());
    OutputDebugStringA(("Animation clips: " + std::to_string(animationClips.size()) + "\n").c_str());
    for (const AnimationClip& clip : animationClips) {
        OutputDebugStringA(("  [" + std::to_string(clip.sourceIndex) + "] " + clip.displayName +
            " | seconds=" + std::to_string(clip.durationSeconds) +
            " | channels=" + std::to_string(clip.channels.size()) +
            (clip.usedDefaultTicksPerSecond ? " | assumed 25 ticks/s" : "") +
            " | unsupported mesh=" + std::to_string(clip.unsupportedMeshChannelCount) +
            " morph=" + std::to_string(clip.unsupportedMorphChannelCount) + "\n").c_str());
    }

    geometry = BuildSkinningGeometry(*scene, skeleton);
    if (geometry.vertices.empty() || geometry.indices.empty()) {
        throw std::runtime_error("Model contains no renderable triangles");
    }
    EnsurePicking();
    groundingVertices = pickVertices;
    const float maxDim = std::max({ boundsMax.x - boundsMin.x,
        boundsMax.y - boundsMin.y, boundsMax.z - boundsMin.z });
    scaleFactor = maxDim > 0 ? 1.5f / maxDim : 1;
    centerOffset = { -(boundsMin.x + boundsMax.x) * 0.5f,
        -(boundsMin.y + boundsMax.y) * 0.5f, -(boundsMin.z + boundsMax.z) * 0.5f };
    textures.resize(scene->mNumMaterials);

    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* material = scene->mMaterials[i];

        aiString texPathStr;

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
                    }
                }
                else {
                    OutputDebugStringA("경고: 압축되지 않은 원시 텍스처 포맷입니다!\n");
                }
            }
        }

        // 내장 텍스처가 없으면 빈 SRV를 유지하여 텍스처 없이 렌더링한다.
    }

    D3D11_BUFFER_DESC mbDesc = {};
    mbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mbDesc.Usage = D3D11_USAGE_DEFAULT;
    mbDesc.ByteWidth = sizeof(MaterialCB);

    if (FAILED(device->CreateBuffer(&mbDesc, nullptr, &materialBuffer)))
        throw std::runtime_error("Failed to create material buffer");
    if (palette.size() > (std::numeric_limits<UINT>::max)() / sizeof(SkeletonMatrix))
        throw std::runtime_error("Skinning palette is too large");
    D3D11_BUFFER_DESC skinDesc{};
    skinDesc.ByteWidth = static_cast<UINT>(palette.size() * sizeof(SkeletonMatrix));
    skinDesc.Usage = D3D11_USAGE_DEFAULT;
    skinDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    skinDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    skinDesc.StructureByteStride = sizeof(SkeletonMatrix);
    if (FAILED(device->CreateBuffer(&skinDesc, nullptr, &skinningBuffer)))
        throw std::runtime_error("Failed to create skinning buffer");
    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
    viewDesc.Format = DXGI_FORMAT_UNKNOWN;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    viewDesc.Buffer.NumElements = static_cast<UINT>(palette.size());
    if (FAILED(device->CreateShaderResourceView(skinningBuffer.Get(), &viewDesc, &skinningView)))
        throw std::runtime_error("Failed to create skinning buffer view");

    vertexBuffer = std::make_unique<VertexBuffer>(device, geometry.vertices.data(), static_cast<UINT>(geometry.vertices.size()));
    indexBuffer = std::make_unique<IndexBuffer>(device, geometry.indices.data(), static_cast<UINT>(geometry.indices.size()));

    behavior = std::make_unique<PetBehavior>(*animator, animationClips);
}

void Model::Draw(ID3D11DeviceContext* context) {
    EnsurePalette();
    if (gpuRevision != paletteRevision) {
        context->UpdateSubresource(skinningBuffer.Get(), 0, nullptr, palette.data(), 0, 0);
        gpuRevision = paletteRevision;
    }
    context->VSSetShaderResources(0, 1, skinningView.GetAddressOf());
    vertexBuffer->Bind(context);
    indexBuffer->Bind(context);

    for (const auto& sm : geometry.draws) {
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
        MaterialCB cbData{};
        cbData.hasTexture = hasTexture ? 1 : 0;
        context->UpdateSubresource(materialBuffer.Get(), 0, nullptr, &cbData, 0, 0);
        context->PSSetConstantBuffers(0, 1, materialBuffer.GetAddressOf()); // 픽셀 셰이더의 b0 슬롯에 연결
        
        context->DrawIndexed(sm.indexCount, sm.startIndex, 0);
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

float Model::GetGroundingBottom(const DirectX::XMMATRIX& worldViewProjection, float clientHeight) const {
    float maximumY = 0;
    for (const auto& vertex : groundingVertices) {
        const auto projected = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertex), worldViewProjection);
        maximumY = std::max(maximumY, (1 - DirectX::XMVectorGetY(projected)) * 0.5f * clientHeight);
    }
    return std::max(0.0f, std::min(maximumY, clientHeight));
}

void Model::EnsurePalette() const {
    if (paletteRevision == animator->GetPoseRevision()) return;
    palette = BuildSkinningPalette(skeleton, animator->GetPose(), geometry);
    paletteRevision = animator->GetPoseRevision();
}

void Model::EnsurePicking() const {
    EnsurePalette();
    if (pickRevision == paletteRevision) return;
    pickVertices = SkinVertices(geometry, palette);
    const float limit = (std::numeric_limits<float>::max)();
    boundsMin = { limit, limit, limit };
    boundsMax = { -limit, -limit, -limit };
    for (const auto& p : pickVertices) {
        boundsMin.x = std::min(boundsMin.x, p.x); boundsMax.x = std::max(boundsMax.x, p.x);
        boundsMin.y = std::min(boundsMin.y, p.y); boundsMax.y = std::max(boundsMax.y, p.y);
        boundsMin.z = std::min(boundsMin.z, p.z); boundsMax.z = std::max(boundsMax.z, p.z);
    }
    pickRevision = paletteRevision;
}
