#pragma once
#include "../core/Types.h"
#include "Entity.h"
#include <glm/vec3.hpp>
#include <vector>

namespace Engine {
class ECSRegistry;
struct BoxColliderComponent {
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    Vec3 offset{0, 0, 0};
    bool enabled = true;
};
struct CharacterMoveResult {
    glm::vec3 feet;
    bool hasGeometry = false;
    bool grounded = false;
    bool hitCeiling = false;
};
CharacterMoveResult MoveCharacter(ECSRegistry& registry, Entity character, glm::vec3 feet,
                                  glm::vec3 delta, float radius = 0.3f, float height = 1.8f);
struct CollisionBounds { glm::vec3 low, high; };
std::vector<CollisionBounds> CollectCollisionBounds(ECSRegistry& registry, Entity exclude, float radius, float height);
CharacterMoveResult SweepCharacter(const std::vector<CollisionBounds>& obstacles, glm::vec3 feet, glm::vec3 delta);
}
