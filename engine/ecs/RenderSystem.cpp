#include "RenderSystem.h"
#include "ECSRegistry.h"
#include "ComponentArray.h"
#include "../renderer/InstancedBatchManager.h"
#include "../renderer/Mesh.h"
#include "../core/logging/Logger.h"
#include "../core/Utf8Path.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <iterator>

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

    SceneLightingSelection SelectSceneLighting(ECSRegistry& registry)
    {
        SceneLightingSelection result;
        auto* lights = registry.GetComponentArray<DirectionalLightComponent>();
        if (!lights)
        {
            return result;
        }
        const DirectionalLightComponent* best = nullptr;
        uint64_t bestUuid = 0;
        for (size_t i = 0; i < lights->Size(); ++i)
        {
            const DirectionalLightComponent& light = lights->GetDenseArray()[i];
            if (!light.enabled)
            {
                continue;
            }
            ++result.candidateCount;
            const Entity entity(lights->GetEntityIDs()[i]);
            const uint64_t uuid = registry.GetUUID(entity).GetValue();
            if (!best || light.priority > best->priority || (light.priority == best->priority && uuid < bestUuid))
            {
                best = &light;
                bestUuid = uuid;
                result.entity = entity;
            }
        }
        if (!best)
        {
            return result;
        }
        const glm::vec3 dir(best->direction.x, best->direction.y, best->direction.z);
        if (glm::dot(dir, dir) < 1e-12f)
        {
            result.invalidDirection = true;   // 방향은 기본값으로, 색·세기·주변광은 라이트 값으로
        }
        else
        {
            result.lighting.toLight = glm::normalize(-dir);
        }
        result.lighting.color = glm::vec3(best->color.x, best->color.y, best->color.z) * best->intensity;
        result.lighting.ambient = best->ambient;
        return result;
    }

    InstanceMaterial MakeInstanceMaterial(const RenderableComponent& renderable, const MaterialData* material)
    {
        const glm::vec3 tint(renderable.baseColor.x, renderable.baseColor.y, renderable.baseColor.z);
        if (!material)
        {
            const MaterialData defaults;
            return { defaults.baseColor * tint, defaults.roughness, defaults.metallic };
        }
        return { material->baseColor * tint, material->roughness, material->metallic };
    }

    const std::string& EffectiveTexturePath(const RenderableComponent& renderable, const MaterialData* material)
    {
        if (!renderable.texturePath.empty() || !material)
        {
            return renderable.texturePath;
        }
        return material->texture;
    }

    void RenderSystem::UpdateLighting(ECSRegistry& registry)
    {
        if (!lightingTarget_)
        {
            return;
        }
        const SceneLightingSelection selection = SelectSceneLighting(registry);
        *lightingTarget_ = selection.lighting;

        const uint64_t uuid = selection.entity.IsValid() ? registry.GetUUID(selection.entity).GetValue() : 0;
        if (uuid == lastLightUuid_ && selection.candidateCount == lastLightCandidates_)
        {
            return;
        }
        lastLightUuid_ = uuid;
        lastLightCandidates_ = selection.candidateCount;
        if (!selection.entity.IsValid())
        {
            Logger::Info("RenderSystem: no enabled DirectionalLightComponent; using the default light");
            return;
        }
        Logger::Info("RenderSystem: using DirectionalLight '{}' (uuid {})", registry.GetEntityName(selection.entity), uuid);
        if (selection.candidateCount > 1)
        {
            Logger::Warning("RenderSystem: {} directional lights are enabled; only one is supported. Using '{}' "
                            "(highest priority, then smallest UUID) and ignoring the others",
                            selection.candidateCount, registry.GetEntityName(selection.entity));
        }
        if (selection.invalidDirection)
        {
            Logger::Warning("RenderSystem: DirectionalLight '{}' has a zero direction; using the default direction",
                            registry.GetEntityName(selection.entity));
        }
    }

    RenderSystem::RenderSystem(InstancedBatchManager* batchManager,
                               std::string assetRoot, MeshLoader meshLoader,
                               TextureLoader textureLoader, SceneLighting* lightingTarget)
        : batchManager_(batchManager)
        , lightingTarget_(lightingTarget)
        , assetRoot_(std::move(assetRoot))
        , meshLoader_(meshLoader ? std::move(meshLoader) : MeshLoader(&Mesh::loadFromFile))
        , textureLoader_(std::move(textureLoader))
    {
    }

    uint32_t RenderSystem::ResolveTexture(const RenderableComponent& renderable)
    {
        return ResolveTexture(renderable.texturePath);
    }

    uint32_t RenderSystem::ResolveTexture(const std::string& texturePath)
    {
        if (texturePath.empty())
        {
            return 0;
        }

        auto it = textures_.find(texturePath);
        if (it != textures_.end())
        {
            return it->second.glId;
        }

        const std::string fullPath = assetRoot_.empty()
            ? texturePath
            : GenericUtf8FromPath(PathFromUtf8(assetRoot_) / PathFromUtf8(texturePath));  // core/Utf8Path.h

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
                texturePath, fullPath, assetRoot_,
                textureLoader_ ? "" : ", no texture loader configured");
            loaded = LoadedTexture{};
        }
        return textures_.emplace(texturePath, std::move(loaded)).first->second.glId;
    }

    uint32_t RenderSystem::ResolveMeshHandle(const RenderableComponent& renderable)
    {
        if (renderable.meshPath.empty())
        {
            return 0;   // 절차적 유닛 큐브
        }
        if (auto reg = registeredMeshes_.find(renderable.meshPath); reg != registeredMeshes_.end())
        {
            return reg->second;   // 코드가 등록한 메시(RegisterMesh)
        }

        auto it = pathHandles_.find(renderable.meshPath);
        if (it != pathHandles_.end())
        {
            return it->second;
        }

        const std::string fullPath = assetRoot_.empty()
            ? renderable.meshPath
            : GenericUtf8FromPath(PathFromUtf8(assetRoot_) / PathFromUtf8(renderable.meshPath));  // core/Utf8Path.h

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

    void RenderSystem::RegisterMesh(const std::string& name, std::shared_ptr<Mesh> mesh)
    {
        if (name.empty() || !mesh)
        {
            Logger::Log(LogLevel::Error, "RenderSystem::RegisterMesh - empty name or null mesh; ignored");
            return;
        }
        uint32_t handle;
        if (auto it = registeredMeshes_.find(name); it != registeredMeshes_.end())
        {
            handle = it->second;
        }
        else
        {
            if (nextRegisteredHandle_ >= kPathHandleBase)
            {
                Logger::Log(LogLevel::Error, "RenderSystem::RegisterMesh - too many registered meshes; '{}' ignored", name);
                return;
            }
            handle = nextRegisteredHandle_++;
            registeredMeshes_[name] = handle;
        }
        meshRegistry_[handle] = std::move(mesh);
    }

    const MaterialData* RenderSystem::ResolveMaterial(const std::string& materialPath)
    {
        if (materialPath.empty())
        {
            return nullptr;
        }
        if (auto it = materials_.find(materialPath); it != materials_.end())
        {
            return it->second ? &*it->second : nullptr;
        }
        const std::string fullPath = assetRoot_.empty()
            ? materialPath
            : GenericUtf8FromPath(PathFromUtf8(assetRoot_) / PathFromUtf8(materialPath));
        std::optional<MaterialData> loaded;
        if (!materialLoader_)
        {
            Logger::Log(LogLevel::Warning, "RenderSystem - material '{}' ignored: no material loader configured", materialPath);
        }
        else if (auto result = materialLoader_(fullPath); result)
        {
            loaded = *result;
        }
        else
        {
            // 경로당 한 번. 기본 머티리얼로 그려지므로 "머티리얼이 안 먹는다"의 답은 이 로그다.
            Logger::Log(LogLevel::Warning, "RenderSystem - material '{}' failed to load (resolved to '{}'): {}; using the default material",
                        materialPath, fullPath, result.error().message);
        }
        auto& slot = materials_.emplace(materialPath, std::move(loaded)).first->second;
        return slot ? &*slot : nullptr;
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

    void RenderSystem::SetAssetRoot(std::string assetRoot)
    {
        pendingAssetRoot_ = std::move(assetRoot);
        assetRootChangePending_ = true;
    }

    void RenderSystem::ApplyPendingAssetRoot()
    {
        if (!assetRootChangePending_) return;
        assetRootChangePending_ = false;
        assetRoot_ = std::move(pendingAssetRoot_);
        pendingAssetRoot_.clear();
        // 경로 메시 핸들과 그 메시, 텍스처를 버린다. 다음 Update에서 새 기준 폴더로 다시 불러온다.
        for (auto it = meshRegistry_.begin(); it != meshRegistry_.end();)
        {
            it = (it->first >= kPathHandleBase) ? meshRegistry_.erase(it) : std::next(it);
        }
        pathHandles_.clear();
        textures_.clear();
        materials_.clear();   // 등록 메시(registeredMeshes_)는 경로 기준이 아니므로 남긴다
        nextPathHandle_ = kPathHandleBase;
        Logger::Info("RenderSystem - asset root changed to '{}'", assetRoot_);
    }

    void RenderSystem::Update(ECSRegistry& registry, float /*deltaTime*/)
    {
        ApplyPendingAssetRoot();
        UpdateLighting(registry);
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
                    const MaterialData* material = ResolveMaterial(rc.materialPath);
                    const uint32_t texture = ResolveTexture(EffectiveTexturePath(rc, material));
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
                    // 인스턴스별 색(R-2). 예전에는 모든 엔티티가 고정값(텍스처 있으면 흰색, 없으면 회색 0.75/0.75/0.78,
                    // roughness 0.6, metallic 0)이었다. 옛 파일은 불러올 때 같은 값으로 채워진다(Reflection.cpp 훅).
                    const InstanceMaterial surface = MakeInstanceMaterial(rc, material);
                    data.color = surface.color;
                    data.roughness = surface.roughness;
                    data.metallic = surface.metallic;
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
