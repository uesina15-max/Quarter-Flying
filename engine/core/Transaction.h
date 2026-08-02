#pragma once

#include "ICommand.h"
#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace Engine
{

class Transaction : public ICommand
{
public:
    explicit Transaction(std::string name);

    void AddCommand(std::unique_ptr<ICommand> cmd);

    // Adds a command that has already been Apply()ed externally (e.g., inside
    // EditorAPI::Dispatch for live-preview during a transaction).
    // The command is counted as applied so that Apply() treats it as a no-op
    // for the already-applied portion and Undo() can roll it back correctly.
    void AddAppliedCommand(std::unique_ptr<ICommand> cmd);

    bool Empty() const { return commands_.empty(); }
    size_t Size() const { return commands_.size(); }

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;
    std::string GetName() const override;

private:
    std::string name_;
    std::vector<std::unique_ptr<ICommand>> commands_;
    size_t appliedCount_ = 0;
};

} // namespace Engine
