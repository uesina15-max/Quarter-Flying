#pragma once

#include "../../core/ICommand.h"
#include "../../core/UUID.h"
#include "../../ecs/Entity.h"
#include <nlohmann/json.hpp>
#include <string>

namespace Engine
{

class ECSRegistry;

// Entity의 모든 컴포넌트 데이터를 직렬화한 스냅샷
struct EntitySnapshot
{
    UUID           uuid;
    std::string    name;
    nlohmann::json componentData;
};

class DestroyEntityCommand : public ICommand
{
public:
    explicit DestroyEntityCommand(ECSRegistry* registry, Entity entity);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override { return "Destroy Entity"; }

private:
    ECSRegistry*   registry_;
    Entity         entity_;
    EntitySnapshot snapshot_;
    bool           snapshotCaptured_ = false;
};

} // namespace Engine
