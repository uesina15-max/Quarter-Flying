#pragma once

#include "System.h"
#include "Components.h"
#include <glm/mat4x4.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace Engine
{
    // Forward declarations
    class InstancedBatchManager;
    class Mesh;
    class Camera;
    struct InstancedBatchKey;

    // TransformComponent(TRS) -> world 행렬. GL/ECSRegistry에 의존하지 않는 순수 함수라서
    // 유닛테스트로 직접 검증 가능하다.
    glm::mat4 ComposeWorldMatrix(const TransformComponent& transform);

    // RenderableComponent::meshHandle -> InstancedBatchManager가 쓰는 InstancedBatchKey.
    // materialId/shaderId/passType/materialLayout/features는 아직 머티리얼 시스템이 없어서
    // 전부 기본값으로 둔다 - 지금은 "메시가 같으면 같은 배치" 수준의 최소 분류만 한다.
    InstancedBatchKey MakeMeshBatchKey(uint32_t meshHandle);

    /// <summary>
    /// ROADMAP.md P0-2("Scene Editor 3D 뷰포트가 실제로 그리는지 확인")의 핵심 - ECS의
    /// TransformComponent+RenderableComponent를 실제 draw call로 옮기는 첫 System.
    ///
    /// 지금까지는 이 다리가 아예 없었다: RenderableComponent는 필드만 존재했고 아무도
    /// 읽지 않았으며, InstancedBatchManager(GPU 인스턴스 배치 관리자)는 다 만들어져 있었지만
    /// 실제로 호출하는 코드가 없었다(CLAUDE.md의 "InstanceData GPU 레이아웃 미검증" 항목이
    /// 바로 이것 - 실행된 적이 없으니 검증도 안 된 상태였다). 이 System이 그 둘을 잇는다.
    ///
    /// 메시 핸들 해석 정책(1차, 의도적으로 단순화): meshHandle에 명시적으로 RegisterMesh()된
    /// 메시가 없으면 절차적으로 생성한 유닛 큐브로 대체한다. 즉 RenderableComponent를 붙인
    /// 엔티티는 (아직 실제 메시를 못 구해도) 항상 뭔가 보인다 - "파이프라인 자체가 살아있는가"
    /// 를 눈으로 확인하는 데 필요한 최소한이다. scene.json의 objects[].model 경로를 실제
    /// meshHandle로 매핑하는 브리지는 별도 후속 작업(ROADMAP.md 참고).
    ///
    /// 카메라 동기화: CameraComponent.isMainCamera == true인 엔티티의 TransformComponent를
    /// 읽어 Camera*(position/lookAt)에 반영한다 - 지금까지 Engine::defaultCamera는 ECS와
    /// 완전히 분리된 고정값이었다(Engine.h 주석 참고). projection(fov/aspect/near/far)은
    /// 건드리지 않는다 - 그건 이미 Engine::HandleWindowResize가 종횡비 기준으로 관리하고
    /// 있어서, 여기서 같이 손대면 두 갱신 경로가 서로 덮어쓸 위험이 있다.
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
        // batchManager/camera는 nullptr일 수 있다(예: GL 컨텍스트 없는 유닛테스트 환경) -
        // 그 경우 Update()는 아무 것도 하지 않고 조용히 리턴한다.
        RenderSystem(InstancedBatchManager* batchManager, Camera* camera);

        void Update(ECSRegistry& registry, float deltaTime) override;

        const char* GetName() const override { return "RenderSystem"; }

        // GL 자원을 직접 건드리는 System을 병렬 스케줄러에 맡기지 않는다(위 클래스 주석 참고).
        bool CanRunInParallel() const override { return false; }

        // 명시적으로 특정 meshHandle에 실제 메시를 등록한다(파일 로드 등). 없으면 기본 큐브로
        // 대체된다.
        void RegisterMesh(uint32_t meshHandle, std::shared_ptr<Mesh> mesh);

    private:
        std::shared_ptr<Mesh> GetOrCreateMesh(uint32_t meshHandle);
        std::shared_ptr<Mesh> CreateUnitCubeMesh();

        InstancedBatchManager* batchManager_;
        Camera* camera_;

        std::unordered_map<uint32_t, std::shared_ptr<Mesh>> meshRegistry_;
        std::shared_ptr<Mesh> defaultCubeMesh_;  // 지연 생성 - 실제로 필요해지기 전엔 GL 호출 없음

        // 이번 프레임 이전까지 배치가 존재했던 meshHandle 집합. 다음 프레임에 더 이상 아무
        // 엔티티도 그 handle을 쓰지 않으면 배치를 정리한다(엔티티 삭제/컴포넌트 제거 대응).
        std::unordered_set<uint32_t> activeMeshHandles_;
    };
}
