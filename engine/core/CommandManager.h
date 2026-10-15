#pragma once

#include "ICommand.h"
#include <cstdint>
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

    // 성공한 Execute/Undo/Redo마다 1씩 오르는 값. 줄어들거나 0으로 돌아가지 않는다(Clear 포함).
    // 에디터의 "저장 안 된 변경" 표시가 쓴다: 저장/불러오기 시점의 값과 지금 값이 다르면 변경이 있다.
    // 스택 크기는 쓸 수 없다 - Undo 깊이 상한(TrimUndoStack)과 Clear 때문에 같은 크기가 다른 상태를 뜻할 수 있다.
    uint64_t GetChangeCount() const { return changeCount_; }

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
    uint64_t changeCount_ = 0;
};

} // namespace Engine
