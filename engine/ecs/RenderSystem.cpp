#include "RenderSystem.h"
#include "ECSRegistry.h"
#include "ComponentArray.h"
#include "../renderer/InstancedBatchManager.h"
#include "../renderer/Mesh.h"
#include "../core/logging/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <filesystem>

namespace Engine
{
InstancedBatchKey MakeMeshBatchKey(uint32_t meshHandle, uint32_t glTexture)
    {
        InstancedBatchKey key;
        key.meshGuid = static_cast<uint64_t>(meshHandle);
        key.materialId = static_cast<uint64_t>(glTexture);
        // shaderId/passType/materialLayout/features는 전부 기본값 - 아직 머티리얼 시스템이 없다.
        return key;
    }

    RenderSystem::RenderSystem(InstancedBatchManager* batchManager,
                               std::string assetRoot, MeshLoader meshLoader,
                               TextureLoader textureLoader)
        : batchManager_(batchManager)
        , assetRoot_(std::move(assetRoot))
        , meshLoader_(meshLoader ? std::move(meshLoader) : MeshLoader(&Mesh::loadFromFile))
        , textureLoader_(std::move(textureLoader))
    {
    }

    uint32_t RenderSystem::ResolveTexture(const RenderableComponent& renderable)
    {
        if (renderable.texturePath.empty())
        {
            return 0;
        }

        auto it = textures_.find(renderable.texturePath);
        if (it != textures_.end())
        {
            return it->second.glId;
        }

        const std::string fullPath = assetRoot_.empty()
            ? renderable.texturePath
            : (std::filesystem::path(assetRoot_) / renderable.texturePath).generic_string();

        LoadedTexture loaded;
        if (textureLoader_)
        {
            loaded = textureLoader_(fullPath);
        }
        if (loaded.glId == 0)
        {
            // 경로당 한 번만 경고한다. 원래 색으로 그려지므로 "텍스처가 왜 안 보이지"의 답은 이 로그뿐이다.
            Logger::Log(LogLevel::Warning,
                "RenderSystem - failed to load texture '{}' (resolved to '{}', assetRoot='{}'{}); drawing without texture",
                renderable.texturePath, fullPath, assetRoot_,
                textureLoader_ ? "" : ", no texture loader configured");
            loaded = LoadedTexture{};
        }
        return textures_.emplace(renderable.texturePath, std::move(loaded)).first->second.glId;
    }

    uint32_t RenderSystem::ResolveMeshHandle(const RenderableComponent& renderable)
    {
        if (renderable.meshPath.empty())
        {
            return renderable.meshHandle;
        }

        auto it = pathHandles_.find(renderable.meshPath);
        if (it != pathHandles_.end())
        {
            return it->second;
        }

        const std::string fullPath = assetRoot_.empty()
            ? renderable.meshPath
            : (std::filesystem::path(assetRoot_) / renderable.meshPath).generic_string();

        std::shared_ptr<Mesh> mesh = meshLoader_(fullPath);
        if (!mesh)
        {
            // 경로당 한 번만 경고한다(매 프레임 찍으면 로그가 묻힌다). 큐브로 대체되므로 화면에는
            // "뭔가" 보이지만, 그게 의도한 모델이 아니라는 건 이 로그로만 알 수 있다.
            Logger::Log(LogLevel::Warning,
                "RenderSystem - failed to load mesh '{}' (resolved to '{}', assetRoot='{}'); drawing placeholder cube",
                renderable.meshPath, fullPath, assetRoot_);
            pathHandles_[renderable.meshPath] = 0;
            return 0;
        }

        uint32_t handle = nextPathHandle_++;
        meshRegistry_[handle] = std::move(mesh);
        pathHandles_[renderable.meshPath] = handle;
        return handle;
    }

    void RenderSystem::RegisterMesh(uint32_t meshHandle, std::shared_ptr<Mesh> mesh)
    {
        if (meshHandle >= kPathHandleBase)
        {
            Logger::Log(LogLevel::Error,
                "RenderSystem::RegisterMesh - handle {} is in the reserved path-mesh range (>= {}); ignored",
                meshHandle, kPathHandleBase);
            return;
        }
        if (mesh)
        {
            meshRegistry_[meshHandle] = std::move(mesh);
        }
    }

    std::shared_ptr<Mesh> RenderSystem::CreateUnitCubeMesh()
    {
        // 24 vertex(면마다 4개, 면별 법선 유지) + 36 index(면마다 삼각형 2개) 유닛 큐브.
        // 실제 메시 에셋 파이프라인(경로->핸들 매핑)이 아직 없어서(RenderSystem.h 주석
        // 참고), meshHandle이 등록 안 된 경우를 위한 절차적 플레이스홀더로 쓴다.
        std::vector<Vertex> vertices = {
            // +X
            {  0.5f, -0.5f, -0.5f,  1,0,0,  0,0 }, {  0.5f,  0.5f, -0.5f,  1,0,0,  1,0 },
            {  0.5f,  0.5f,  0.5f,  1,0,0,  1,1 }, {  0.5f, -0.5f,  0.5f,  1,0,0,  0,1 },
            // -X
            { -0.5f, -0.5f,  0.5f, -1,0,0,  0,0 }, { -0.5f,  0.5f,  0.5f, -1,0,0,  1,0 },
            { -0.5f,  0.5f, -0.5f, -1,0,0,  1,1 }, { -0.5f, -0.5f, -0.5f, -1,0,0,  0,1 },
            // +Y
            { -0.5f,  0.5f, -0.5f,  0,1,0,  0,0 }, { -0.5f,  0.5f,  0.5f,  0,1,0,  1,0 },
            {  0.5f,  0.5f,  0.5f,  0,1,0,  1,1 }, {  0.5f,  0.5f, -0.5f,  0,1,0,  0,1 },
            // -Y
            { -0.5f, -0.5f,  0.5f,  0,-1,0, 0,0 }, { -0.5f, -0.5f, -0.5f, 0,-1,0, 1,0 },
            {  0.5f, -0.5f, -0.5f,  0,-1,0, 1,1 }, {  0.5f, -0.5f,  0.5f, 0,-1,0, 0,1 },
            // +Z
            { -0.5f, -0.5f,  0.5f,  0,0,1,  0,0 }, {  0.5f, -0.5f,  0.5f,  0,0,1,  1,0 },
            {  0.5f,  0.5f,  0.5f,  0,0,1,  1,1 }, { -0.5f,  0.5f,  0.5f,  0,0,1,  0,1 },
            // -Z
            {  0.5f, -0.5f, -0.5f,  0,0,-1, 0,0 }, { -0.5f, -0.5f, -0.5f, 0,0,-1, 1,0 },
            { -0.5f,  0.5f, -0.5f,  0,0,-1, 1,1 }, {  0.5f,  0.5f, -0.5f,  0,0,-1, 0,1 },
        };
        std::vector<unsigned int> indices;
        indices.reserve(36);
        for (unsigned int face = 0; face < 6; ++face)
        {
            unsigned int base = face * 4;
            indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->create(vertices, indices);
        return mesh;
    }

    std::shared_ptr<Mesh> RenderSystem::GetOrCreateMesh(uint32_t meshHandle)
    {
        auto it = meshRegistry_.find(meshHandle);
        if (it != meshRegistry_.end())
        {
            return it->second;
        }
        if (!defaultCubeMesh_)
        {
            defaultCubeMesh_ = CreateUnitCubeMesh();
        }
        return defaultCubeMesh_;
    }

    void RenderSystem::Update(ECSRegistry& registry, float /*deltaTime*/)
    {
        if (batchManager_)
        {
            auto* renderables = registry.GetComponentArray<RenderableComponent>();
            auto* transforms = registry.GetComponentArray<TransformComponent>();

            std::unordered_set<uint64_t> seenThisFrame;  // BatchId(meshHandle, texture)

            if (renderables && transforms)
            {
                for (size_t i = 0; i < renderables->Size(); ++i)
                {
                    EntityID id = renderables->GetEntityIDs()[i];
                    const RenderableComponent& rc = renderables->GetDenseArray()[i];
                    const TransformComponent* tc = transforms->Get(id);
                    if (!tc)
                    {
                        continue;  // Renderable인데 Transform이 없으면 그릴 위치를 알 수 없다
                    }

                    // meshPath가 있으면 경로 메시(첫 사용 시 로드), 없으면 meshHandle - ResolveMeshHandle 참고
                    const uint32_t meshHandle = ResolveMeshHandle(rc);
                    const uint32_t texture = ResolveTexture(rc);
                    const uint64_t batchId = BatchId(meshHandle, texture);
                    InstancedBatchKey key = MakeMeshBatchKey(meshHandle, texture);

                    if (seenThisFrame.insert(batchId).second)
                    {
                        // 이 프레임에 처음 보는 (메시, 텍스처) - 배치가 없으면 만들고, 있으면
                        // 매 프레임 처음부터 다시 채울 것이므로 인스턴스 목록을 비운다
                        // (AddInstance는 중복 없이 그냥 append라서 안 비우면 계속 누적됨).
                        if (activeBatches_.find(batchId) == activeBatches_.end())
                        {
                            auto mesh = GetOrCreateMesh(meshHandle);
                            auto result = batchManager_->CreateBatch(key, BatchType::Dynamic, mesh);
                            if (!result)
                            {
                                Logger::Log(LogLevel::Warning,
                                    "RenderSystem::Update - CreateBatch failed for meshHandle {} (texture {}): {}",
                                    meshHandle, texture, result.error().message);
                                continue;
                            }
                            batchManager_->SetBatchTexture(key, texture);
                            activeBatches_.insert(batchId);
                        }
                        batchManager_->ClearInstances(key);
                    }

                    InstanceData data{};
                    // 부모가 있으면 조상 Transform까지 곱한 월드 행렬(Hierarchy.h). 루트면 로컬과 같다.
                    data.model = ComputeWorldMatrix(registry, Entity(id));
                    // 머티리얼 시스템 이전 임시 고정값. 텍스처가 있으면 셰이더가 텍스처 색에 이 값을 곱하므로
                    // 흰색으로 둬야 텍스처 원래 색이 나온다.
                    data.color = texture != 0 ? glm::vec3(1.0f) : glm::vec3(0.75f, 0.75f, 0.78f);
                    data.roughness = 0.6f;
                    data.metallic = 0.0f;
                    data.entityId = id;
                    data.isSelected = 0;
                    auto addResult = batchManager_->AddInstance(key, data, id);
                    if (!addResult)
                    {
                        Logger::Log(LogLevel::Warning,
                            "RenderSystem::Update - AddInstance failed for entity {}: {}",
                            id, addResult.error().message);
                    }
                }
            }

            // 이번 프레임에 아무 엔티티도 안 쓴 (메시, 텍스처) 배치는 정리한다
            // (엔티티 삭제/RenderableComponent 제거/texturePath 변경에 대응).
            for (auto it = activeBatches_.begin(); it != activeBatches_.end(); )
            {
                if (seenThisFrame.find(*it) == seenThisFrame.end())
                {
                    const uint32_t meshHandle = static_cast<uint32_t>(*it & 0xFFFFFFFFu);
                    const uint32_t texture = static_cast<uint32_t>(*it >> 32);
                    auto removeResult = batchManager_->RemoveBatch(MakeMeshBatchKey(meshHandle, texture));
                    if (!removeResult)
                    {
                        Logger::Log(LogLevel::Warning,
                            "RenderSystem::Update - RemoveBatch failed for meshHandle {} (texture {}): {}",
                            meshHandle, texture, removeResult.error().message);
                    }
                    it = activeBatches_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }
}
