#pragma once

#include "../../core/ICommand.h"
#include "../../core/UUID.h"
#include "../../ecs/Entity.h"
#include <string>

namespace Engine
{

class ECSRegistry;

class CreateEntityCommand : public ICommand
{
public:
    explicit CreateEntityCommand(ECSRegistry* registry, std::string entityName = "");

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override { return "Create Entity"; }

    // 생성된 Entity를 외부에서 조회 (EditorAPI::CreateEntity 반환용)
    Entity GetCreatedEntity() const { return createdEntity_; }

private:
    ECSRegistry* registry_;
    std::string  entityName_;
    Entity       createdEntity_;
    UUID         savedUUID_;
    bool         applied_ = false;  // 첫 Apply 여부 추적
};

} // namespace Engine
