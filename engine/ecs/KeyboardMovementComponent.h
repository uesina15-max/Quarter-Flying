#pragma once
namespace Engine {
// Local prototype control, serialized with the entity's existing scene data.
struct KeyboardMovementComponent {
    bool enabled = true;
    float speed = 4.0f; // World units per second; absent fields keep this default.
};
}
