#pragma once

#include "../../core/ICommand.h"
#include "../../ecs/Entity.h"
#include "../../core/UUID.h"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace Engine
{

class ECSRegistry;

// 리플렉션에 등록된 컴포넌트의 필드 하나를 바꾸는 명령(Inspector의 일반 필드 편집).
//
// 왜 있나(2026-10-22, docs/EDITOR_BUGFIX_PLAN_2026-10-22.md F1): Inspector는 position·parent만 명령으로 보내고
// 나머지 필드(rotation, scale, 색, 메시 경로 …)는 ECSRegistry.SetComponentFieldJson으로 레지스트리에 직접 썼다.
// 증상: 그 편집은 Undo가 안 되고, "저장 안 된 변경"(CommandManager 변경 카운터)에도 잡히지 않아 저장하지 않고
// 닫으면 경고 없이 사라졌다. 크래시도 로그도 없어서 드러나지 않았다.
//
// 이전 값은 컴포넌트를 serialize한 JSON에서 그 필드만 떼어 둔다. 적용·되돌리기는 모두 patchField를 쓰므로
// 필드 형식(EulerRotation은 도 단위 오일러, EntityRef는 UUID)이 직렬화와 같은 규약으로 왕복한다.
class SetComponentFieldCommand : public ICommand
{
public:
    SetComponentFieldCommand(ECSRegistry* registry, Entity entity, std::string componentName,
                             std::string fieldName, nlohmann::json newValue, uint32_t expectedVersion);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    // 병합 세션(입력칸에 포커스가 있는 동안) 안에서 같은 엔티티·컴포넌트·필드의 연속 편집을 Undo 한 번으로 묶는다.
    bool CanMergeWith(const ICommand& other) const override;
    std::expected<void, EngineError> MergeWith(const ICommand& other) override;

    std::string GetName() const override { return "Set " + componentName_ + "." + fieldName_; }

private:
    std::expected<void, EngineError> Validate();
    std::expected<void, EngineError> Write(const nlohmann::json& value);

    ECSRegistry* registry_;
    Entity entity_;
    UUID entityUUID_;
    std::string componentName_;
    std::string fieldName_;
    nlohmann::json newValue_;
    nlohmann::json oldValue_;
    bool hasOldValue_ = false;
    uint32_t expectedVersion_;
};

} // namespace Engine
