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

// 자식부터(역순) 파괴한다. 무효한 엔티티는 건너뛴다.
void DestroyEntitiesReverse(ECSRegistry& registry, const std::vector<Entity>& entities);

} // namespace Engine
