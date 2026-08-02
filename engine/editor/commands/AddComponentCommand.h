#pragma once

#include "../../core/ICommand.h"
#include "../../ecs/Entity.h"
#include <string>

namespace Engine
{

class ECSRegistry;

class AddComponentCommand : public ICommand
{
public:
    AddComponentCommand(ECSRegistry* registry, Entity entity, std::string componentType);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override { return "Add Component"; }

private:
    ECSRegistry* registry_;
    Entity       entity_;
    std::string  componentType_;
};

} // namespace Engine
