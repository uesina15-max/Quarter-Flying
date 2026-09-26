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

    // Non-copyable, non-movable singleton (holds std::vector<std::unique_ptr<ICommand>>,
    // which already makes the implicit copy ctor deleted -- but declaring it explicitly
    // avoids an MSVC quirk where determining an *implicit* special member is deleted
    // eagerly instantiates the member types involved (here vector<unique_ptr<ICommand>>'s
    // copy ctor) instead of just noting it's deleted, which failed with C2672 when
    // pybind11's class_<CommandManager> registration triggered that check).
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;
    CommandManager(CommandManager&&) = delete;
    CommandManager& operator=(CommandManager&&) = delete;

private:
    CommandManager() = default;

    void TrimUndoStack();

    std::vector<std::unique_ptr<ICommand>> undoStack_;
    std::vector<std::unique_ptr<ICommand>> redoStack_;
    size_t maxUndoDepth_ = 100;
    bool inMergeSession_ = false;
};

} // namespace Engine
