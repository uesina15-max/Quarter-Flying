#include "KeyboardMovementSystem.h"
#include "StaticCollision.h"
#include "KeyboardMovementComponent.h"
#include "ECSRegistry.h"
#include "ComponentArray.h"
#include "Components.h"
#include "ScriptComponent.h"
#include "CameraSystem.h"
#include "Hierarchy.h"
#include "../input/InputState.h"
#include <algorithm>
#include <cmath>

namespace Engine {
void KeyboardMovementSystem::Update(ECSRegistry& registry, float deltaTime) {
    if (!input_ || !std::isfinite(deltaTime) || deltaTime <= 0) return;
    // Editor shortcuts must not also move the prototype when Ctrl/Alt is held.
    if (input_->IsKeyDown(KeyCode::Control) || input_->IsKeyDown(KeyCode::Alt)) return;
    auto down = [&](KeyCode a, KeyCode b) { return input_->IsKeyDown(a) || input_->IsKeyDown(b); };
    float x = float(down(KeyCode::D,KeyCode::Right)) - float(down(KeyCode::A,KeyCode::Left));
    float z = float(down(KeyCode::W,KeyCode::Up)) - float(down(KeyCode::S,KeyCode::Down));
    const float magnitude = std::hypot(x,z);
    if (magnitude == 0) return;
    x /= magnitude; z /= magnitude; // Diagonal input has the same speed.
    float fx=0, fz=-1;
    const Entity camera = SelectActiveCamera(registry).entity;
    if (camera.IsValid()) {
        const auto rotation = ComputeWorldTransform(registry,camera).rotation;
        fx = -2*(rotation.x*rotation.z + rotation.w*rotation.y);
        fz = -(1-2*(rotation.x*rotation.x + rotation.y*rotation.y));
        const float length=std::hypot(fx,fz);
        if (std::isfinite(length) && length>1e-6f) { fx/=length; fz/=length; }
        else { fx=0; fz=-1; }
    }
    auto* controls=registry.GetComponentArray<KeyboardMovementComponent>();
    if (!controls) return;
    for (size_t i=0;i<controls->Size();++i) {
        const auto& control=controls->GetDenseArray()[i];
        Entity entity(controls->GetEntityIDs()[i]);
        // Parent-local coordinates and scripts would compete with this world-space control.
        // Guard the runtime too: saved scenes can bypass the Inspector's eligibility checks.
        if (!control.enabled || GetParent(registry,entity).IsValid() ||
            registry.HasComponent<ScriptComponent>(entity) || registry.HasComponent<CameraComponent>(entity)) continue;
        auto* transform=registry.GetTransformComponent(entity);
        if (!transform) continue;
        const float speed=control.speed;
        if (!std::isfinite(speed) || speed<0 || speed>1000) continue;
        const float distance=speed*std::min(deltaTime,0.1f);
        const auto& start = transform->position;
        const auto moved = MoveCharacter(registry, entity, {start.x,start.y,start.z},
            {(-fz*x + fx*z)*distance, 0, (fx*x + fz*z)*distance});
        transform->position.x = moved.feet.x;
        transform->position.z = moved.feet.z;
    }
}
}
