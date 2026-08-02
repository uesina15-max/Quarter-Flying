#pragma once

#include "ICommand.h"
#include <expected>
#include <memory>
#include <vector>

namespace Engine
{

class CommandManager
{
public:
    static CommandManager& GetInstance();

    std::expected<void, EngineError> Execute(std::unique_ptr<ICommand> command);
    std::expected<void, EngineError> Undo();
    std::expected<void, EngineError> Redo();

    bool CanUndo() const;
    bool CanRedo() const;

    void Clear();

    void SetMaxUndoDepth(size_t depth);
    size_t GetMaxUndoDepth() const;

    void BeginMergeSession();
    void EndMergeSession();
    bool IsInMergeSession() const { return inMergeSession_; }

private:
    CommandManager() = default;

    void TrimUndoStack();

    std::vector<std::unique_ptr<ICommand>> undoStack_;
    std::vector<std::unique_ptr<ICommand>> redoStack_;
    size_t maxUndoDepth_ = 100;
    bool inMergeSession_ = false;
};

} // namespace Engine
