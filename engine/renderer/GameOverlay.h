#pragma once
#include "../platform/IPlatform.h"
#include <memory>
#include <string>

namespace Engine {
class ECSRegistry;
class GameOverlay {
public:
    GameOverlay();
    ~GameOverlay();
    void QueuePointer(const InputEvent& event);
    // interactive=false(일시정지): 그리기만 한다. 스크립트가 멈춘 동안 UI 콜백을 부르지 않고 쌓인 입력은 버린다.
    void Frame(ECSRegistry& registry, const std::string& assetRoot, bool interactive = true);
private:
    struct State;
    std::unique_ptr<State> state;
};
}
