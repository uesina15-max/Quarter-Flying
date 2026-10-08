#pragma once

#include "../../core/ICommand.h"
#include "../../ecs/Entity.h"
#include "EntitySnapshot.h"
#include <string>
#include <vector>

namespace Engine
{

class ECSRegistry;

// 엔티티와 그 자손 전체를 복제한다(개선안 P1-5). 원본은 그대로 두고, 전부 새 UUID를 가진
// 복제본을 만든다. 복제된 트리 안을 가리키는 EntityRef(자식의 parent, 카메라 리그 target 등)는
// 복제본으로 재연결하고, 트리 밖을 가리키는 참조(루트의 부모 등)는 그대로 둬서 복제본이 원본과
// 같은 부모 아래로 들어간다 - 재매핑 규칙은 MakeDuplicateSnapshots 참고.
//
// 구조는 DestroyEntityCommand를 뒤집은 것이다: 최초 Apply가 복제 스냅샷(새 UUID 고정)을 한 번
// 만들어두고, Apply는 그 스냅샷으로 생성, Undo는 같은 UUID로 다시 찾아 파괴한다. 그래서 Redo
// (두 번째 Apply)도 항상 같은 UUID의 복제본을 되살린다(Undo/Redo 사이에 원본이 바뀌어도 복제본은
// 최초 시점 그대로다 - InstantiatePrefabCommand가 캐싱한 PrefabAsset을 재사용하는 것과 같은 이유).
class DuplicateEntityCommand : public ICommand
{
public:
    explicit DuplicateEntityCommand(ECSRegistry* registry, Entity source);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override { return "Duplicate Entity"; }

    // 복제된 하위 트리의 루트 런타임 Entity (EditorAPI::DuplicateEntity 반환용 - 선택 이동에 쓴다).
    // Apply마다 갱신된다(Redo 시 런타임 id가 새로 바뀌므로).
    Entity GetDuplicatedRoot() const { return duplicatedRoot_; }

private:
    ECSRegistry*                registry_;
    Entity                      source_;
    std::vector<EntitySnapshot> snapshots_;        // [0] = 복제 루트, 이후 자손(부모 먼저). 새 UUID.
    bool                        prepared_ = false;  // 복제 스냅샷을 만들었는지
    Entity                      duplicatedRoot_;
};

} // namespace Engine
