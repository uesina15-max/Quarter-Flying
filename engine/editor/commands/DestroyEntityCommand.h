#pragma once

#include "../../core/ICommand.h"
#include "../../ecs/Entity.h"
#include "EntitySnapshot.h"
#include <string>
#include <vector>

namespace Engine
{

class ECSRegistry;

// 엔티티와 그 자손 전체를 파괴한다(프리팹 Phase 5). Undo는 전부 원래 UUID로 되살린다.
// 자손까지 지우는 이유: 부모만 지우면 자식의 HierarchyComponent.parent가 사라진 엔티티를 가리켜
// 자식이 조용히 루트로 튀어나오고(월드 위치가 부모 기준에서 원점 기준으로 바뀜), Undo로 부모를
// 되살려도 부모의 런타임 id가 새로 바뀌어 다시 연결되지 않는다.
class DestroyEntityCommand : public ICommand
{
public:
    explicit DestroyEntityCommand(ECSRegistry* registry, Entity entity);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override { return "Destroy Entity"; }

private:
    ECSRegistry*                registry_;
    Entity                      entity_;
    std::vector<EntitySnapshot> snapshots_;          // [0] = 대상, 이후 자손(부모 먼저)
    bool                        snapshotCaptured_ = false;
};

} // namespace Engine
