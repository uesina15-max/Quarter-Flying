#include "CommandManager.h"

namespace Engine
{

CommandManager& CommandManager::GetInstance()
{
    static CommandManager instance;
    return instance;
}

std::expected<void, EngineError> CommandManager::Execute(std::unique_ptr<ICommand> command)
{
    if (!command)
    {
        return MakeError(EngineErrorCode::InvalidParameter, "Null command", "CommandManager");
    }

    const auto applyResult = command->Apply();
    if (!applyResult)
    {
        return applyResult;
    }

    if (inMergeSession_ && !undoStack_.empty() && undoStack_.back()->CanMergeWith(*command))
    {
        const auto mergeResult = undoStack_.back()->MergeWith(*command);
        if (mergeResult)
        {
            redoStack_.clear();
            return {};
        }
    }

    redoStack_.clear();
    undoStack_.push_back(std::move(command));
    TrimUndoStack();
    return {};
}

std::expected<void, EngineError> CommandManager::Undo()
{
    if (undoStack_.empty())
    {
        return MakeError(EngineErrorCode::InvalidState, "Nothing to undo", "CommandManager");
    }

    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();

    const auto undoResult = command->Undo();
    if (!undoResult)
    {
        undoStack_.push_back(std::move(command));
        return undoResult;
    }

    redoStack_.push_back(std::move(command));
    return {};
}

std::expected<void, EngineError> CommandManager::Redo()
{
    if (redoStack_.empty())
    {
        return MakeError(EngineErrorCode::InvalidState, "Nothing to redo", "CommandManager");
    }

    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();

    const auto applyResult = command->Apply();
    if (!applyResult)
    {
        redoStack_.push_back(std::move(command));
        return applyResult;
    }

    undoStack_.push_back(std::move(command));
    TrimUndoStack();
    return {};
}

bool CommandManager::CanUndo() const
{
    return !undoStack_.empty();
}

bool CommandManager::CanRedo() const
{
    return !redoStack_.empty();
}

void CommandManager::Clear()
{
    undoStack_.clear();
    redoStack_.clear();
    inMergeSession_ = false;
}

void CommandManager::SetMaxUndoDepth(size_t depth)
{
    maxUndoDepth_ = depth;
    TrimUndoStack();
}

size_t CommandManager::GetMaxUndoDepth() const
{
    return maxUndoDepth_;
}

void CommandManager::BeginMergeSession()
{
    inMergeSession_ = true;
}

void CommandManager::EndMergeSession()
{
    inMergeSession_ = false;
}

void CommandManager::TrimUndoStack()
{
    while (undoStack_.size() > maxUndoDepth_)
    {
        undoStack_.erase(undoStack_.begin());
    }
}

} // namespace Engine
