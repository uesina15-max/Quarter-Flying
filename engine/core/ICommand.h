#pragma once

#include "EngineError.h"
#include <expected>
#include <string>

namespace Engine
{

class ICommand
{
public:
    virtual ~ICommand() = default;

    virtual std::expected<void, EngineError> Apply() = 0;
    virtual std::expected<void, EngineError> Undo() = 0;

    virtual bool CanMergeWith(const ICommand& other) const { return false; }
    virtual std::expected<void, EngineError> MergeWith(const ICommand& other)
    {
        return MakeError(EngineErrorCode::OperationFailed, "Command is not mergeable", "Command");
    }

    virtual std::string GetName() const = 0;
};

} // namespace Engine
