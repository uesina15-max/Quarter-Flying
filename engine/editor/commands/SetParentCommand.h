#pragma once

#include "../../core/ICommand.h"
#include "../../core/UUID.h"
#include "../../ecs/Entity.h"
#include "../../ecs/Components.h"
#include <optional>
#include <string>

namespace Engine
{

class ECSRegistry;

// 부모 변경(Scene Hierarchy 드래그, 프리팹 Phase 5). 순환 검사는 Hierarchy::SetParent가 한다.
// 월드 위치를 유지한다(SetParent의 keepWorldTransform = true) - 로컬 값을 유지하면 드래그한 물체가
// 새 부모의 회전/스케일로 다시 해석돼 화면 밖으로 사라졌다. Undo/Redo는 재계산하지 않고 로컬 Transform을
// 그대로 되돌린다(부동소수점 오차가 Undo/Redo를 반복할수록 쌓이지 않게).
// 엔티티는 UUID로 기억한다 - 그 사이 다른 명령의 Undo/Redo로 엔티티가 새 런타임 id로 되살아나도
// (DestroyEntityCommand 등) 이 명령의 Undo/Redo가 올바른 엔티티를 찾는다.
class SetParentCommand : public ICommand
{
public:
    // newParent가 Entity()면 루트로 만든다.
    SetParentCommand(ECSRegistry* registry, Entity child, Entity newParent);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override { return "Set Parent"; }

private:
    std::expected<void, EngineError> SetParentByUUID(UUID parentUuid, bool keepWorld,
                                                     const std::optional<TransformComponent>& local);

    ECSRegistry* registry_;
    UUID         childUuid_{0};
    UUID         newParentUuid_{0};   // 0 = 루트
    UUID         oldParentUuid_{0};   // 0 = 루트
    bool         captured_ = false;
    std::optional<TransformComponent> oldLocal_;   // Transform이 없는 엔티티면 비어 있음
    std::optional<TransformComponent> newLocal_;
};

} // namespace Engine
