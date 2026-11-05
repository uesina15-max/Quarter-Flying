#pragma once

#include "System.h"
#include "Components.h"
#include "Hierarchy.h"   // ComposeWorldMatrix / ComputeWorldMatrix (예전엔 여기 선언돼 있었다)
#include "Picking.h"     // LocalBounds
#include "../renderer/SceneLighting.h"
#include "../asset/MaterialAsset.h"   // MaterialData(순수 데이터). 파일 읽기는 주입된 로더가 한다
#include <optional>
#include <glm/mat4x4.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

    // 씬 조명 선택 규칙의 유일한 구현(docs/USABILITY_IMPROVEMENT_PLAN.md R-1). RenderSystem이 매 프레임 부르고,
    // 테스트도 이것을 직접 부른다(GL 없음).
    //  1. 후보: enabled == true인 DirectionalLightComponent
    //  2. priority가 가장 높은 것, 같으면 UUID가 가장 작은 것("첫 번째"는 ECS 순회 순서라 저장·불러오기·복제 뒤에
    //     바뀔 수 있다. UUID는 씬 파일에 저장되므로 에디터와 Player가 같은 라이트를 고른다)
    //  3. 후보가 없거나 direction이 0이면 SceneLighting::Default()(라이트 도입 전과 같은 화면)
    struct SceneLightingSelection
    {
        SceneLighting lighting = SceneLighting::Default();
        Entity entity;                 // 고른 라이트(없으면 IsValid() == false)
        size_t candidateCount = 0;     // 켜진 라이트 수(2 이상이면 나머지는 무시됨)
        bool invalidDirection = false; // 고른 라이트의 direction이 0이라 기본 방향을 썼다
    };
    SceneLightingSelection SelectSceneLighting(ECSRegistry& registry);

    // 머티리얼 합성 규칙의 유일한 구현(R-4). 배치 키에 넣지 않는다(색·roughness·metallic이 달라도 같은 배치).
    //  색 = 머티리얼 baseColor x 컴포넌트 baseColor, roughness·metallic = 머티리얼(없으면 0.6 / 0.0),
    //  텍스처 경로 = 컴포넌트 texturePath가 있으면 그것, 없으면 머티리얼 texture(EffectiveTexturePath).
    struct InstanceMaterial
    {
        glm::vec3 color;
        float roughness;
        float metallic;
    };
    InstanceMaterial MakeInstanceMaterial(const RenderableComponent& renderable, const MaterialData* material = nullptr);
    const std::string& EffectiveTexturePath(const RenderableComponent& renderable, const MaterialData* material);

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
    ///  1. meshPath가 RegisterMesh()로 등록한 이름이면 그 메시를 쓴다(파일을 찾지 않는다).
    ///  2. 아니면 그 파일(에셋 루트 기준 상대경로)을 이 System이 처음 볼 때 한 번 로드하고, 내부
    ///     핸들(kPathHandleBase 이상)에 캐시한다. 경로·이름은 직렬화에 안전해서 프리팹/PIE 스냅샷/
    ///     Inspector 편집이 그대로 동작한다. 내부 핸들 정수는 파일에 저장하지 않는다(R-4 이전의
    ///     RenderableComponent.meshHandle은 다음 세션에 의미가 없어서 스키마 2에서 뺐다).
    ///  3. meshPath가 비어 있으면 절차적 유닛 큐브("파이프라인이 살아있는가"를 보기 위한 기존 동작).
    ///  4. 경로 로드에 실패하면 경고를 한 번 남기고(경로 포함) 큐브로 대체한다.
    ///
    /// 머티리얼(ResolveMaterial, R-4): materialPath 파일을 경로당 한 번 읽는다. 실패하면 경고 한 번 후
    /// 기본 머티리얼(roughness 0.6, metallic 0 = 머티리얼 도입 전 화면). 합성 규칙은 MakeInstanceMaterial
    /// 하나에 있다.
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

        // lightingTarget: 매 Update에 씬 조명(SelectSceneLighting 결과)을 써 넣을 곳. 보통 Renderer가 소유한다.
        // nullptr이면 조명을 고르지 않는다(유닛테스트).
        explicit RenderSystem(InstancedBatchManager* batchManager,
                              std::string assetRoot = {}, MeshLoader meshLoader = {},
                              TextureLoader textureLoader = {}, SceneLighting* lightingTarget = nullptr);

        void Update(ECSRegistry& registry, float deltaTime) override;

        const char* GetName() const override { return "RenderSystem"; }

        // GL 자원을 직접 건드리는 System을 병렬 스케줄러에 맡기지 않는다(위 클래스 주석 참고).
        bool CanRunInParallel() const override { return false; }

        // 코드가 만든 메시를 이름으로 등록한다. RenderableComponent.meshPath에 그 이름을 쓰면 파일 대신 이 메시를 쓴다.
        // 이름은 경로처럼 저장·스냅샷에 안전하다(예전에는 정수 핸들로 등록해서 Undo·Play/Stop을 지나면 의미가 없어졌다).
        // 에셋 기준 폴더가 바뀌어도 유지된다. 같은 이름으로 다시 등록하면 바꾼다.
        void RegisterMesh(const std::string& name, std::shared_ptr<Mesh> mesh);

        // 머티리얼 파일 로더(보통 Engine이 asset/MaterialAsset의 LoadMaterialFile을 넣는다). 전체 경로를 받는다.
        // 비워 두면 materialPath는 경고 후 무시된다(텍스처 로더와 같은 방식).
        using MaterialLoader = std::function<Result<MaterialData>(const std::string& fullPath)>;
        void SetMaterialLoader(MaterialLoader loader) { materialLoader_ = std::move(loader); }
        // materialPath -> 머티리얼(경로당 한 번 로드, 실패는 경고 한 번 후 nullptr = 기본 머티리얼).
        const MaterialData* ResolveMaterial(const std::string& materialPath);

        // 에셋 기준 폴더를 바꾼다(에디터에서 다른 프로젝트를 열 때). 메시·텍스처 캐시는 상대 경로가
        // 키라서 같이 비워야 하는데, 텍스처 해제는 GL 호출이라 이 엔진의 GL 컨텍스트가 current일 때만
        // 해야 한다. 에디터에는 엔진(GL 컨텍스트)이 여럿 있어서 Python에서 부른 시점의 current
        // 컨텍스트는 남의 것일 수 있다. 그래서 여기서는 예약만 하고 다음 Update(TickFrame 안)에서 바꾼다.
        void SetAssetRoot(std::string assetRoot);
        const std::string& GetAssetRoot() const { return assetRoot_; }

        // 이 컴포넌트를 그릴 때 쓸 meshHandle (클래스 주석의 해석 정책). 경로 메시는 첫 호출 때
        // 로드되므로 GL 컨텍스트가 current인 상태(Update 안)에서 불러야 한다.
        uint32_t ResolveMeshHandle(const RenderableComponent& renderable);

        // 이 컴포넌트를 그릴 때 바인딩할 GL 텍스처(0 = 없음/실패). 경로당 한 번 로드하고 캐시한다.
        // 텍스처도 GL 자원이라 ResolveMeshHandle과 같이 Update 안에서 불러야 한다.
        uint32_t ResolveTexture(const RenderableComponent& renderable);
        uint32_t ResolveTexture(const std::string& texturePath);

        // 이 컴포넌트가 그리는 메시의 로컬 경계(뷰포트 피킹, ecs/Picking.h). ResolveMeshHandle과 달리 파일을
        // 불러오지 않는다(GL 불필요) - 아직 불러오지 않았거나 실패한 메시는 그 자리에 그려지는 유닛 큐브의 경계다.
        LocalBounds GetMeshBounds(const RenderableComponent& renderable) const;

        // 에디터 선택 강조(셰이더의 isSelected). 고른 엔티티와 그 자손의 메시가 강조된다(부모만 고르면 메시가 없는
        // 경우가 많다 - 예: Lamp). 무엇을 넘길지(Edit 모드에서만 등)는 Engine이 정한다.
        void SetSelectedEntities(std::vector<Entity> entities) { selected_ = std::move(entities); }

        // 경로 기반 메시에 배정하는 내부 핸들의 시작값. RegisterMesh(이름)가 배정하는 핸들은 이보다 작다
        // (에셋 기준 폴더가 바뀌면 이 값 이상만 비운다 - 코드가 등록한 메시는 남는다).
        static constexpr uint32_t kPathHandleBase = 0x40000000u;

    private:
        std::shared_ptr<Mesh> GetOrCreateMesh(uint32_t meshHandle);
        std::shared_ptr<Mesh> CreateUnitCubeMesh();

        InstancedBatchManager* batchManager_;
        SceneLighting* lightingTarget_ = nullptr;
        std::vector<Entity> selected_;
        bool IsSelected(ECSRegistry& registry, Entity entity) const;
        // 직전 프레임에 고른 라이트(로그는 선택이 바뀔 때만 남긴다 - 매 프레임 같은 경고가 쌓이지 않게).
        uint64_t lastLightUuid_ = ~0ull;
        size_t lastLightCandidates_ = ~size_t(0);
        void UpdateLighting(ECSRegistry& registry);

        std::unordered_map<uint32_t, std::shared_ptr<Mesh>> meshRegistry_;
        std::shared_ptr<Mesh> defaultCubeMesh_;

        std::string assetRoot_;
        std::string pendingAssetRoot_;
        bool assetRootChangePending_ = false;
        void ApplyPendingAssetRoot();
        MeshLoader meshLoader_;
        std::unordered_map<std::string, uint32_t> pathHandles_;  // meshPath -> 핸들 (실패는 0)

        TextureLoader textureLoader_;
        MaterialLoader materialLoader_;
        std::unordered_map<std::string, std::optional<MaterialData>> materials_;   // materialPath -> (실패는 nullopt)
        std::unordered_map<std::string, uint32_t> registeredMeshes_;              // RegisterMesh 이름 -> 핸들
        uint32_t nextRegisteredHandle_ = 1;   // 0 = 절차적 큐브, 등록 메시는 1부터, 경로 메시는 kPathHandleBase부터
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
