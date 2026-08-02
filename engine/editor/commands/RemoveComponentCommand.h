#pragma once

#include "../../core/ICommand.h"
#include "../../ecs/Entity.h"
#include <nlohmann/json.hpp>
#include <string>

namespace Engine
{

class ECSRegistry;

class RemoveComponentCommand : public ICommand
{
public:
    RemoveComponentCommand(ECSRegistry* registry, Entity entity, std::string componentType);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override { return "Remove Component"; }

private:
    ECSRegistry*   registry_;
    Entity         entity_;
    std::string    componentType_;
    nlohmann::json snapshot_;
    bool           snapshotCaptured_ = false;
};

} // namespace Engine
