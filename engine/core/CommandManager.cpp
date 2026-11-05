#include "CommandManager.h"
#include <stdexcept>

namespace Engine
{

CommandManager& CommandManager::GetInstance()
{
    static CommandManager instance;
    return instance;
}

std::expected<void, EngineError> CommandManager::Execute(std::unique_ptr<ICommand> command)
{
    if (IsEditingLocked())
        return MakeError(EngineErrorCode::InvalidState, "Editing is locked during Play/Pause", "CommandManager");
    if (!command)
    {
        return MakeError(EngineErrorCode::InvalidParameter, "Null command", "CommandManager");
    }

    const auto applyResult = command->Apply();
    if (!applyResult)
    {
        return applyResult;
    }
    ++changeCount_;   // Apply가 성공한 순간 월드가 바뀌었다(병합 여부와 무관)

    // 증상: Inspector에서 X칸을 고치고 포커스를 옮겨 Y칸을 고친 뒤 Ctrl+Z를 한 번 누르면 X·Y가 같이 되돌아갔다.
    // Play/Stop 뒤에도 기록이 남게 된 뒤로는 Play 전 편집과 Stop 후 편집까지 한 칸으로 합쳐졌다. 에러 없이 Undo
    // 한 번이 두 편집을 지워서, 사용자는 "Undo가 너무 많이 되돌린다"로만 보게 된다.
    // 원인: 맨 위 명령이 언제 쌓였는지 보지 않고 CanMergeWith(같은 엔티티·필드)만 확인했다. 그래서 새 병합 세션의
    // 첫 편집이 이전 세션(또는 세션 밖)에서 쌓인 명령에 합쳐졌다. 이제 이번 세션에서 쌓은 명령에만 합친다.
    if (inMergeSession_ && topPushedInMergeSession_ && !undoStack_.empty()
        && undoStack_.back()->CanMergeWith(*command))
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
    topPushedInMergeSession_ = inMergeSession_;
    TrimUndoStack();
    return {};
}

std::expected<void, EngineError> CommandManager::Undo()
{
    if (IsEditingLocked())
        return MakeError(EngineErrorCode::InvalidState, "Editing is locked during Play/Pause", "CommandManager");
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
    topPushedInMergeSession_ = false;
    ++changeCount_;
    return {};
}

std::expected<void, EngineError> CommandManager::Redo()
{
    if (IsEditingLocked())
        return MakeError(EngineErrorCode::InvalidState, "Editing is locked during Play/Pause", "CommandManager");
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
    topPushedInMergeSession_ = false;
    TrimUndoStack();
    ++changeCount_;
    return {};
}

bool CommandManager::CanUndo() const
{
    return !IsEditingLocked() && !undoStack_.empty();
}

bool CommandManager::CanRedo() const
{
    return !IsEditingLocked() && !redoStack_.empty();
}

void CommandManager::Clear()
{
    if (IsEditingLocked()) throw std::runtime_error("CommandManager: cannot clear history during Play/Pause");
    undoStack_.clear();
    redoStack_.clear();
    inMergeSession_ = false;
    topPushedInMergeSession_ = false;
}

void CommandManager::SetMaxUndoDepth(size_t depth)
{
    if (IsEditingLocked()) throw std::runtime_error("CommandManager: cannot trim history during Play/Pause");
    maxUndoDepth_ = depth;
    TrimUndoStack();
}

size_t CommandManager::GetMaxUndoDepth() const
{
    return maxUndoDepth_;
}

void CommandManager::BeginMergeSession()
{
    if (IsEditingLocked()) return;
    inMergeSession_ = true;
    topPushedInMergeSession_ = false;   // 새 세션은 그 전에 쌓인 명령에 합치지 않는다(Execute 주석)
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

namespace Engine {
void CommandManager::SetEditorRegistry(ECSRegistry* registry) {
    if (IsEditingLocked() && registry != editorRegistry_)
        throw std::runtime_error("CommandManager: stop Play before switching registry");
    editorRegistry_ = registry;
}
std::expected<void, EngineError> CommandManager::BeginPlay(ECSRegistry* registry) {
    if (registry != editorRegistry_) return {};
    if (transactionOpen_)
        return MakeError(EngineErrorCode::InvalidState, "Finish or cancel the current edit before Play", "CommandManager");
    // UI locks alone allowed direct commands to consume or replace pre-Play history.
    // Keep the existing stacks intact and prohibit edit commands until restoration.
    EndMergeSession();
    playRegistry_ = registry;
    return {};
}
void CommandManager::EndPlay(ECSRegistry* registry) {
    if (playRegistry_ == registry) playRegistry_ = nullptr;
}
void CommandManager::ForgetRegistry(ECSRegistry* registry) {
    if (editorRegistry_ != registry) return;
    EndPlay(registry);
    Clear();
    editorRegistry_ = nullptr;
    transactionOpen_ = false;
}
}
