#pragma once

#include "Components.h"
#include "Entity.h"
#include "../core/EngineError.h"
#include <glm/mat4x4.hpp>
#include <expected>
#include <vector>

namespace Engine
{
    class ECSRegistry;

    /// 엔티티 계층(부모-자식) 조회/변경과 월드 변환 계산 (프리팹 Phase 5).
    ///
    /// 저장은 HierarchyComponent.parent 하나뿐이다(Components.h 주석). 이 파일의 함수는 그 위의
    /// 읽기 전용 계산과, 순환을 막는 유일한 쓰기 경로(SetParent)다.
    ///
    /// 순환: SetParent는 순환을 거절한다. 하지만 Inspector나 SetComponentJson처럼 필드를 직접 쓰는
    /// 경로는 이 검사를 거치지 않으므로, 계층을 따라 올라가는 모든 함수는 kMaxHierarchyDepth에서
    /// 멈추고 경고를 남긴다(무한 루프 대신 "잘못된 위치에 그려짐 + 로그").
    ///
    /// 부모가 파괴돼 parent가 무효(IsValid false)가 된 자식은 루트로 취급한다. EntityManager는 id를
    /// 재사용하지 않으므로(단조 증가) 무효 parent가 엉뚱한 새 엔티티를 가리키게 되는 일은 없다.

    constexpr int kMaxHierarchyDepth = 64;

    // TransformComponent(TRS) -> 행렬. 부모를 고려하지 않는 로컬 행렬이다(루트면 곧 월드 행렬).
    // 이름은 계층 도입 전부터 쓰던 것을 유지한다(RenderSystem 테스트 등).
    glm::mat4 ComposeWorldMatrix(const TransformComponent& transform);

    // 부모(없거나 무효면 Entity()).
    Entity GetParent(ECSRegistry& registry, Entity entity);

    // 직계 자식. id 오름차순(= 생성 순서)이라 호출마다 순서가 같다.
    std::vector<Entity> GetChildren(ECSRegistry& registry, Entity parent);

    // root를 제외한 모든 자손. 부모가 항상 자식보다 먼저 나온다(전위 순회) - 이 순서대로 다시 만들면
    // parent EntityRef(UUID)가 항상 이미 존재하는 엔티티를 가리킨다.
    std::vector<Entity> GetDescendants(ECSRegistry& registry, Entity root);

    // ancestor가 entity 자신이거나 그 조상인가.
    bool IsSelfOrAncestor(ECSRegistry& registry, Entity ancestor, Entity entity);

    // child의 부모를 parent로 바꾼다. parent가 Entity()면 루트로 만든다(HierarchyComponent 제거).
    // keepWorldTransform:
    //   false - 로컬 Transform 값을 그대로 둔다(새 부모 기준 같은 로컬 위치). 프리팹 스폰처럼 로컬 값이
    //           곧 데이터인 경우.
    //   true  - 화면에서 제자리에 있도록 새 부모 기준 로컬 값을 다시 계산한다(에디터 드래그/Inspector).
    //           false로 드래그했더니 물체가 새 부모의 회전/스케일로 다시 해석돼 화면 밖으로 사라져서,
    //           에디터 경로는 이 쪽을 쓴다. 부모가 비균등 스케일이면서 회전돼 있으면 근사(전단 손실).
    // 거절: child 무효, parent가 무효(Entity()가 아닌데), 자기 자신, 자기 자손(순환).
    std::expected<void, EngineError> SetParent(ECSRegistry& registry, Entity child, Entity parent,
                                               bool keepWorldTransform = false);

    // 행렬 -> TRS(위치, 회전, 스케일). 거울 변환(행렬식 < 0)은 x 스케일 부호로 표현한다.
    TransformComponent DecomposeToTransform(const glm::mat4& matrix);

    // 월드 행렬 = 루트부터 부모 행렬들 * 로컬 행렬. Transform이 없는 조상은 항등으로 본다.
    glm::mat4 ComputeWorldMatrix(ECSRegistry& registry, Entity entity);

    // 월드 TRS. 위치와 회전은 정확하다. 스케일은 성분별 곱이라 부모가 비균등 스케일이면서 자식이
    // 회전돼 있으면(전단 변형) 근사다 - 그 경우에도 그리기는 ComputeWorldMatrix(정확)를 쓴다.
    // 카메라/파티클처럼 "위치와 방향"만 필요한 곳이 쓴다.
    TransformComponent ComputeWorldTransform(ECSRegistry& registry, Entity entity);
}
