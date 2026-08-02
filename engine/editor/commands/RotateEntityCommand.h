#pragma once

#include "../../core/ICommand.h"
#include "../../core/Types.h"
#include "../../ecs/Entity.h"
#include <string>

namespace Engine
{

class ECSRegistry;

class RotateEntityCommand : public ICommand
{
public:
    RotateEntityCommand(ECSRegistry* registry, Entity entity, Quaternion newRotation);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    bool CanMergeWith(const ICommand& other) const override;
    std::expected<void, EngineError> MergeWith(const ICommand& other) override;

    std::string GetName() const override { return "Rotate Entity"; }

private:
    ECSRegistry* registry_;
    Entity entity_;
    Quaternion oldRotation_;
    Quaternion newRotation_;
};

} // namespace Engine
