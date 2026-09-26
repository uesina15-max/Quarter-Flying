#include "RenderSystem.h"
#include "ECSRegistry.h"
#include "ComponentArray.h"
#include "../renderer/InstancedBatchManager.h"
#include "../renderer/Mesh.h"
#include "../renderer/Camera.h"
#include "../core/logging/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine
{
    glm::mat4 ComposeWorldMatrix(const TransformComponent& transform)
    {
        glm::vec3 position(transform.position.x, transform.position.y, transform.position.z);
        glm::quat rotation(transform.rotation.w, transform.rotation.x, transform.rotation.y, transform.rotation.z);
        glm::vec3 scale(transform.scale.x, transform.scale.y, transform.scale.z);

        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 r = glm::mat4_cast(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }

    InstancedBatchKey MakeMeshBatchKey(uint32_t meshHandle)
    {
        InstancedBatchKey key;
        key.meshGuid = static_cast<uint64_t>(meshHandle);
        // materialId/shaderId/passType/materialLayout/features는 전부 기본값 - 아직
        // 머티리얼 시스템이 없어서(§주석 참고) 메시 단위로만 배치를 나눈다.
        return key;
    }

    RenderSystem::RenderSystem(InstancedBatchManager* batchManager, Camera* camera)
        : batchManager_(batchManager)
        , camera_(camera)
    {
    }

    void RenderSystem::RegisterMesh(uint32_t meshHandle, std::shared_ptr<Mesh> mesh)
    {
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

            std::unordered_set<uint32_t> seenThisFrame;

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

                    InstancedBatchKey key = MakeMeshBatchKey(rc.meshHandle);

                    if (seenThisFrame.insert(rc.meshHandle).second)
                    {
                        // 이 프레임에 처음 보는 meshHandle - 배치가 없으면 만들고, 있으면
                        // 매 프레임 처음부터 다시 채울 것이므로 인스턴스 목록을 비운다
                        // (AddInstance는 중복 없이 그냥 append라서 안 비우면 계속 누적됨).
                        if (activeMeshHandles_.find(rc.meshHandle) == activeMeshHandles_.end())
                        {
                            auto mesh = GetOrCreateMesh(rc.meshHandle);
                            auto result = batchManager_->CreateBatch(key, BatchType::Dynamic, mesh);
                            if (!result)
                            {
                                Logger::Log(LogLevel::Warning,
                                    "RenderSystem::Update - CreateBatch failed for meshHandle {}: {}",
                                    rc.meshHandle, result.error().message);
                                continue;
                            }
                            activeMeshHandles_.insert(rc.meshHandle);
                        }
                        batchManager_->ClearInstances(key);
                    }

                    InstanceData data{};
                    data.model = ComposeWorldMatrix(*tc);
                    data.color = glm::vec3(0.75f, 0.75f, 0.78f);  // 머티리얼 시스템 이전 임시 고정값
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

            // 이번 프레임에 아무 엔티티도 안 쓴 meshHandle의 배치는 정리한다
            // (엔티티 삭제/RenderableComponent 제거에 대응).
            for (auto it = activeMeshHandles_.begin(); it != activeMeshHandles_.end(); )
            {
                if (seenThisFrame.find(*it) == seenThisFrame.end())
                {
                    auto removeResult = batchManager_->RemoveBatch(MakeMeshBatchKey(*it));
                    if (!removeResult)
                    {
                        Logger::Log(LogLevel::Warning,
                            "RenderSystem::Update - RemoveBatch failed for meshHandle {}: {}",
                            *it, removeResult.error().message);
                    }
                    it = activeMeshHandles_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // 카메라 동기화: isMainCamera 엔티티의 Transform -> Camera(position/lookAt).
        // projection(fov/aspect/near/far)은 건드리지 않는다(RenderSystem.h 주석 참고).
        if (camera_)
        {
            auto* cameras = registry.GetComponentArray<CameraComponent>();
            auto* transforms = registry.GetComponentArray<TransformComponent>();
            if (cameras && transforms)
            {
                for (size_t i = 0; i < cameras->Size(); ++i)
                {
                    const CameraComponent& cc = cameras->GetDenseArray()[i];
                    if (!cc.isMainCamera)
                    {
                        continue;
                    }
                    EntityID id = cameras->GetEntityIDs()[i];
                    const TransformComponent* tc = transforms->Get(id);
                    if (!tc)
                    {
                        continue;
                    }

                    glm::vec3 position(tc->position.x, tc->position.y, tc->position.z);
                    glm::quat rotation(tc->rotation.w, tc->rotation.x, tc->rotation.y, tc->rotation.z);
                    glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);

                    camera_->setPosition(position);
                    camera_->lookAt(position + forward);
                    break;  // isMainCamera 유일성은 상위 계층 책임 - 첫 번째 것만 사용
                }
            }
        }
    }
}
