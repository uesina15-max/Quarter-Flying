#pragma once

#include "System.h"
#include "Components.h"
#include "Hierarchy.h"   // ComposeWorldMatrix / ComputeWorldMatrix (예전엔 여기 선언돼 있었다)
#include <glm/mat4x4.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Engine
{
    // Forward declarations
    class InstancedBatchManager;
    class Mesh;
    class Camera;
    struct InstancedBatchKey;

// (meshHandle, GL 텍스처) -> InstancedBatchManager가 쓰는 InstancedBatchKey.
    // 텍스처는 배치 단위로 바인딩되므로(InstancedBatchManager::RenderBatch) 메시가 같아도 텍스처가
    // 다르면 다른 배치여야 한다. 텍스처 id를 materialId 자리에 넣는다(머티리얼 시스템이 아직 없음).
    // shaderId/passType/materialLayout/features는 기본값.
    InstancedBatchKey MakeMeshBatchKey(uint32_t meshHandle, uint32_t glTexture = 0);

    /// <summary>
    /// ROADMAP.md P0-2("Scene Editor 3D 뷰포트가 실제로 그리는지 확인")의 핵심 - ECS의
    /// TransformComponent+RenderableComponent를 실제 draw call로 옮기는 첫 System.
    ///
    /// 지금까지는 이 다리가 아예 없었다: RenderableComponent는 필드만 존재했고 아무도
    /// 읽지 않았으며, InstancedBatchManager(GPU 인스턴스 배치 관리자)는 다 만들어져 있었지만
    /// 실제로 호출하는 코드가 없었다(CLAUDE.md의 "InstanceData GPU 레이아웃 미검증" 항목이
    /// 바로 이것 - 실행된 적이 없으니 검증도 안 된 상태였다). 이 System이 그 둘을 잇는다.
    ///
    /// 메시 해석 정책(ResolveMeshHandle):
    ///  1. RenderableComponent.meshPath가 있으면 그 파일(에셋 루트 기준 상대경로)을 이 System이
    ///     처음 볼 때 한 번 로드하고, 내부 핸들(kPathHandleBase 이상)에 캐시한다. 경로는 직렬화에
    ///     안전해서 프리팹/PIE 스냅샷/Inspector 편집이 그대로 동작한다. 런타임에 배정되는
    ///     meshHandle 정수는 다음 세션에 의미가 없다.
    ///  2. meshPath가 비어 있으면 meshHandle을 쓴다. RegisterMesh()된 메시가 없으면 절차적 유닛
    ///     큐브로 대체한다("파이프라인이 살아있는가"를 보기 위한 기존 동작).
    ///  3. 경로 로드에 실패하면 경고를 한 번 남기고(경로 포함) 큐브로 대체한다.
    ///
    /// 텍스처(ResolveTexture): RenderableComponent.texturePath가 있으면 TextureLoader로 경로당 한 번
    /// 로드한다. 실패하면 경고를 한 번 남기고 텍스처 없이 그린다(흰색 대신 원래 색 - "텍스처가 안 붙었다"는
    /// 것을 로그와 화면 양쪽에서 알 수 있다). 파일 해석(asset/ 모듈)과 GL 업로드(Texture2D)는 로더를
    /// 만드는 Engine이 잇는다 - 이 System은 asset/에 의존하지 않는다.
    /// 캐시가 System(=World=Engine=GL 컨텍스트) 단위인 이유는 Mesh::loadFromFile 주석 참고.
    ///
    /// 카메라 동기화는 여기 없다. CameraSystem으로 분리했다(docs/INGAME_CAMERA_PLAN.md C1).
    ///
    /// System.h의 설계 원칙("System은 상태를 가지면 안 됨 - Thread-safe 하지 않음")과
    /// 정면으로는 안 맞지만(meshRegistry_/activeBatchKeys_ 등 내부 상태를 가짐), 그 원칙은
    /// "여러 System이 동시에 병렬 실행돼도 안전해야 한다"는 뜻이고 이 System은
    /// CanRunInParallel()을 false로 명시해 그 경우 자체를 차단한다 - GL 자원(InstancedBatchManager)
    /// 을 건드리는 System을 병렬 스케줄러에 맡기는 건 애초에 안전하지 않다.
    /// </summary>
    class RenderSystem : public System
    {
    public:
        // batchManager는 nullptr일 수 있다(예: GL 컨텍스트 없는 유닛테스트 환경) -
        // 그 경우 Update()는 아무 것도 하지 않고 조용히 리턴한다.
        // meshLoader: 전체 경로를 받아 메시를 만든다(실패 시 nullptr). 비워 두면 Mesh::loadFromFile.
        // 유닛테스트는 GL 없이 가짜 로더를 넣는다.
        using MeshLoader = std::function<std::shared_ptr<Mesh>(const std::string& fullPath)>;

        // 텍스처 로더의 결과: GL 텍스처 id(0 = 실패)와 그 수명을 쥐는 소유자(보통 Texture2D).
        // 소유자를 void로 지운 이유: 유닛테스트가 GL 없이 가짜 id만 돌려줄 수 있게 하려고.
        struct LoadedTexture
        {
            uint32_t glId = 0;
            std::shared_ptr<void> owner;
        };
        // 전체 경로를 받아 텍스처를 만든다. 비워 두면 텍스처를 쓰지 않는다(texturePath는 경고 후 무시).
        using TextureLoader = std::function<LoadedTexture(const std::string& fullPath)>;

        explicit RenderSystem(InstancedBatchManager* batchManager,
                              std::string assetRoot = {}, MeshLoader meshLoader = {},
                              TextureLoader textureLoader = {});

        void Update(ECSRegistry& registry, float deltaTime) override;

        const char* GetName() const override { return "RenderSystem"; }

        // GL 자원을 직접 건드리는 System을 병렬 스케줄러에 맡기지 않는다(위 클래스 주석 참고).
        bool CanRunInParallel() const override { return false; }

        // 명시적으로 특정 meshHandle에 실제 메시를 등록한다(파일 로드 등). 없으면 기본 큐브로
        // 대체된다.
        void RegisterMesh(uint32_t meshHandle, std::shared_ptr<Mesh> mesh);

        // 이 컴포넌트를 그릴 때 쓸 meshHandle (클래스 주석의 해석 정책). 경로 메시는 첫 호출 때
        // 로드되므로 GL 컨텍스트가 current인 상태(Update 안)에서 불러야 한다.
        uint32_t ResolveMeshHandle(const RenderableComponent& renderable);

        // 이 컴포넌트를 그릴 때 바인딩할 GL 텍스처(0 = 없음/실패). 경로당 한 번 로드하고 캐시한다.
        // 텍스처도 GL 자원이라 ResolveMeshHandle과 같이 Update 안에서 불러야 한다.
        uint32_t ResolveTexture(const RenderableComponent& renderable);

        // 경로 기반 메시에 배정하는 내부 핸들의 시작값. 명시적 RegisterMesh 핸들은 이보다 작아야 한다.
        static constexpr uint32_t kPathHandleBase = 0x40000000u;

    private:
        std::shared_ptr<Mesh> GetOrCreateMesh(uint32_t meshHandle);
        std::shared_ptr<Mesh> CreateUnitCubeMesh();

        InstancedBatchManager* batchManager_;

        std::unordered_map<uint32_t, std::shared_ptr<Mesh>> meshRegistry_;
        std::shared_ptr<Mesh> defaultCubeMesh_;

        std::string assetRoot_;
        MeshLoader meshLoader_;
        std::unordered_map<std::string, uint32_t> pathHandles_;  // meshPath -> 핸들 (실패는 0)

        TextureLoader textureLoader_;
        std::unordered_map<std::string, LoadedTexture> textures_;  // texturePath -> 텍스처 (실패는 glId 0)
        uint32_t nextPathHandle_ = kPathHandleBase;  // 지연 생성 - 실제로 필요해지기 전엔 GL 호출 없음

        // 이번 프레임 이전까지 배치가 존재했던 (meshHandle, 텍스처) 집합(BatchId로 묶음). 다음
        // 프레임에 더 이상 아무 엔티티도 그 조합을 쓰지 않으면 배치를 정리한다(엔티티 삭제/컴포넌트
        // 제거/Inspector에서 texturePath 변경 대응).
        static uint64_t BatchId(uint32_t meshHandle, uint32_t glTexture)
        {
            return (static_cast<uint64_t>(glTexture) << 32) | meshHandle;
        }
        std::unordered_set<uint64_t> activeBatches_;
    };
}
