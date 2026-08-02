#include "Transaction.h"

namespace Engine
{

Transaction::Transaction(std::string name)
    : name_(std::move(name))
{
}

void Transaction::AddCommand(std::unique_ptr<ICommand> cmd)
{
    commands_.push_back(std::move(cmd));
}

void Transaction::AddAppliedCommand(std::unique_ptr<ICommand> cmd)
{
    commands_.push_back(std::move(cmd));
    ++appliedCount_;
}

std::expected<void, EngineError> Transaction::Apply()
{
    // Apply only commands that haven't been applied yet.
    // This supports two usage patterns:
    //   1. Batch apply: AddCommand() multiple times, then Apply() once.
    //   2. Incremental apply: AddCommand() + ApplyLast() for each command (live preview
    //      inside a transaction). Apply() on CommitTransaction is then a no-op when
    //      all commands are already applied (appliedCount_ == commands_.size()).
    // On Redo, appliedCount_ has been reset to 0 by Undo(), so all commands re-apply.

    for (size_t i = appliedCount_; i < commands_.size(); ++i)
    {
        auto result = commands_[i]->Apply();
        if (!result)
        {
            // Roll back ALL applied commands (both pre-applied and newly applied)
            for (size_t j = appliedCount_; j-- > 0;)
            {
                commands_[j]->Undo();
            }
            appliedCount_ = 0;
            return result;
        }
        ++appliedCount_;
    }

    return {};
}

std::expected<void, EngineError> Transaction::Undo()
{
    for (size_t i = appliedCount_; i-- > 0;)
    {
        auto result = commands_[i]->Undo();
        if (!result)
        {
            return result;
        }
    }
    appliedCount_ = 0;
    return {};
}

std::string Transaction::GetName() const
{
    if (commands_.empty())
    {
        return "";
    }

    std::string result;
    for (size_t i = 0; i < commands_.size(); ++i)
    {
        if (i > 0)
        {
            result += ',';
        }
        result += commands_[i]->GetName();
    }
    return result;
}

} // namespace Engine
