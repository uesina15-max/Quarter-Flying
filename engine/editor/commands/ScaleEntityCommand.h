#pragma once

#include "../../core/ICommand.h"
#include "../../core/Types.h"
#include "../../ecs/Entity.h"
#include <string>

namespace Engine
{

class ECSRegistry;

class ScaleEntityCommand : public ICommand
{
public:
    ScaleEntityCommand(ECSRegistry* registry, Entity entity, Vec3 newScale);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    bool CanMergeWith(const ICommand& other) const override;
    std::expected<void, EngineError> MergeWith(const ICommand& other) override;

    std::string GetName() const override { return "Scale Entity"; }

private:
    ECSRegistry* registry_;
    Entity entity_;
    Vec3 oldScale_;
    Vec3 newScale_;
};

} // namespace Engine
