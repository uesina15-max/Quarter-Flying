#pragma once

#include "../../core/ICommand.h"
#include "../../core/Types.h"
#include "../../ecs/Entity.h"
#include <string>

namespace Engine
{

class ECSRegistry;

class MoveEntityCommand : public ICommand
{
public:
    MoveEntityCommand(ECSRegistry* registry, Entity entity, Vec3 newPosition);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    bool CanMergeWith(const ICommand& other) const override;
    std::expected<void, EngineError> MergeWith(const ICommand& other) override;

    std::string GetName() const override { return "Move Entity"; }

private:
    ECSRegistry* registry_;
    Entity entity_;
    Vec3 oldPosition_;
    Vec3 newPosition_;
};

} // namespace Engine
