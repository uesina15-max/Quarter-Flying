#pragma once

#include "../../core/EngineError.h"
#include "../../core/UUID.h"
#include "../../ecs/Entity.h"
#include <nlohmann/json.hpp>
#include <expected>
#include <string>
#include <vector>

namespace Engine
{

class ECSRegistry;

// Entity의 모든 컴포넌트 데이터를 직렬화한 스냅샷 (Undo용).
struct EntitySnapshot
{
    UUID           uuid;
    std::string    name;
    nlohmann::json componentData;
};

// 등록된 모든 컴포넌트(HierarchyComponent 포함)를 담는다.
EntitySnapshot CaptureEntitySnapshot(ECSRegistry& registry, Entity entity);

// 스냅샷들을 원래 UUID로 되살린다. 반환 순서 = 입력 순서.
// 두 단계로 한다: 먼저 전부 만들고(이름 포함) 그다음 컴포넌트를 채운다. 그래야 서로를 가리키는
// EntityRef(자식의 HierarchyComponent.parent 등)가 UUID로 올바른 엔티티를 찾는다 - 한 단계로 하면
// 아직 안 만들어진 부모를 가리키는 자식은 parent가 조용히 Entity()(루트)가 된다.
std::expected<std::vector<Entity>, EngineError>
RestoreEntitySnapshots(ECSRegistry& registry, const std::vector<EntitySnapshot>& snapshots);

// entity와 모든 자손을 전위 순서(부모 먼저)로 스냅샷한다. RestoreEntitySnapshots와 짝.
std::vector<EntitySnapshot> CaptureSubtreeSnapshots(ECSRegistry& registry, Entity root);

// root 하위 트리를 "복제"하기 위한 스냅샷을 만든다. CaptureSubtreeSnapshots와 달리 각 엔티티에
// 새 UUID를 주므로, RestoreEntitySnapshots에 넘기면 원본과 공존하는 별도 엔티티가 생긴다.
// 핵심은 EntityRef 재매핑이다(자식의 HierarchyComponent.parent, 카메라 리그 target 등 UUID로
// 직렬화되는 필드):
//   - 참조가 복제되는 트리 "안"을 가리키면  → 그에 대응하는 새 UUID로 바꾼다
//     (그래야 복제본의 부모-자식/타깃 관계가 원본이 아니라 복제본끼리 연결된다).
//   - 참조가 트리 "밖"을 가리키면(루트의 parent = 원본의 부모 등) → 그대로 둔다
//     (복제본이 원본과 같은 부모 아래 들어가고, 바깥 엔티티를 그대로 가리킨다).
// 원본 레지스트리 상태는 바꾸지 않는다(읽기만 한다).
std::vector<EntitySnapshot> MakeDuplicateSnapshots(ECSRegistry& registry, Entity root);

// 자식부터(역순) 파괴한다. 무효한 엔티티는 건너뛴다.
void DestroyEntitiesReverse(ECSRegistry& registry, const std::vector<Entity>& entities);

} // namespace Engine
