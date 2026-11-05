#pragma once
#include "System.h"
namespace Engine {
class InputState;
class KeyboardMovementSystem : public System {
public:
    explicit KeyboardMovementSystem(const InputState* input) : input_(input) {}
    void Update(ECSRegistry& registry, float deltaTime) override;
    const char* GetName() const override { return "KeyboardMovementSystem"; }
    int GetPriority() const override { return -20; } // Move before the follow camera.
    bool RunsInEditMode() const override { return false; }
    bool CanRunInParallel() const override { return false; }
private:
    const InputState* input_;
};
}
