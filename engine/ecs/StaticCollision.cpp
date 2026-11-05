#include "StaticCollision.h"
#include "Hierarchy.h"
#include "ECSRegistry.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace Engine {
namespace {
struct Contact { float time = 1; int axis = -1; float plane = 0; };

// 면에 닿아 있다고 볼 거리(월드 단위). 부동소수점 오차(좌표 1000 근처의 float 한 칸이 약 0.00006)보다 크고,
// 눈에 보이는 파고듦보다 작아야 한다.
//
// 증상: 캐릭터가 착지한 뒤 가끔 바닥을 뚫고 끝없이 떨어지거나(240fps, 높이 4.13에서 낙하), 타일 경계에서 영구히
// 멈췄다(dt가 흔들리는 시뮬레이션 400회 중 1회). 컴파일도 되고 크래시도 없으며 기존 테스트는 전부 통과했다 -
// 정확히 어떤 float 값에 착지했느냐에 달려 있어 재현이 어렵다.
// 원인: 착지 위치(start + t*delta)는 반올림 때문에 면보다 한 칸 아래일 수 있는데, 허용 오차가 거리가 아니라
// 시간 비율(entry >= -0.00001)이었다. 실제 허용 거리는 0.00001 * |delta|라서, 중력 이동량이 작은 고프레임에서는
// float 한 칸보다 좁아져 "이미 상자 안"으로 판정됐다. 또 움직이지 않는 축(delta == 0)은 오차 없이 비교해서,
// 한 칸 아래에 서 있으면 옆 타일의 옆면에 걸렸다.
constexpr float kContactSkin = 0.001f;

Contact Sweep(const CollisionBounds& box, glm::vec3 start, glm::vec3 delta) {
    float entry = -std::numeric_limits<float>::infinity();
    float exit = std::numeric_limits<float>::infinity();
    int normalAxis = -1;
    float plane = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (delta[axis] == 0) {
            if (start[axis] <= box.low[axis] + kContactSkin || start[axis] >= box.high[axis] - kContactSkin) return {};
            continue;
        }
        // 이 축으로 멀어지는 중이고 이미 나가는 면에 붙어 있으면 부딪칠 일이 없다(바닥에서 점프, 벽에서 떨어지기).
        const float leaving = delta[axis] > 0 ? box.high[axis] - start[axis] : start[axis] - box.low[axis];
        if (leaving <= kContactSkin) return {};
        float near = (box.low[axis] - start[axis]) / delta[axis];
        float far = (box.high[axis] - start[axis]) / delta[axis];
        if (near > far) std::swap(near, far);
        if (near > entry) {
            entry = near;
            normalAxis = axis;
            plane = delta[axis] > 0 ? box.low[axis] : box.high[axis];
        }
        exit = std::min(exit, far);
    }
    if (normalAxis < 0 || entry > 1 || entry >= exit) return {};
    // 시작점이 이미 면 안쪽이다. 오차 범위면 그 면에 닿은 것으로, 더 깊으면(상자 안에서 시작) 빠져나갈 수 있게 무시한다.
    if (entry < 0 && -entry * std::abs(delta[normalAxis]) > kContactSkin) return {};
    return {std::max(0.f, entry), normalAxis, plane};
}
}

CharacterMoveResult MoveCharacter(ECSRegistry& registry, Entity character, glm::vec3 feet,
                                  glm::vec3 delta, float radius, float height) {
    return SweepCharacter(CollectCollisionBounds(registry, character, radius, height), feet, delta);
}

std::vector<CollisionBounds> CollectCollisionBounds(ECSRegistry& registry, Entity character, float radius, float height) {
    auto* shapes = registry.GetComponentArray<BoxColliderComponent>();
    std::vector<CollisionBounds> obstacles;
    if (shapes) for (EntityID id : shapes->GetEntityIDs()) {
        const Entity entity(id);
        const auto& shape = *registry.GetComponent<BoxColliderComponent>(entity);
        if (entity == character || !shape.enabled || !registry.HasTransformComponent(entity)) continue;
        const auto matrix = ComputeWorldMatrix(registry, entity);
        const auto center = glm::vec3(matrix * glm::vec4(shape.offset.x, shape.offset.y, shape.offset.z, 1));
        const auto extent = glm::abs(glm::vec3(matrix[0]) * shape.halfExtents.x)
                          + glm::abs(glm::vec3(matrix[1]) * shape.halfExtents.y)
                          + glm::abs(glm::vec3(matrix[2]) * shape.halfExtents.z);
        if (!(extent.x > 0 && extent.y > 0 && extent.z > 0)) continue;
        obstacles.push_back({center - extent - glm::vec3(radius,height,radius),
                             center + extent + glm::vec3(radius,0,radius)});
    }
    return obstacles;
}

CharacterMoveResult SweepCharacter(const std::vector<CollisionBounds>& obstacles, glm::vec3 feet, glm::vec3 delta) {
    CharacterMoveResult result{feet};
    result.hasGeometry = !obstacles.empty();
    for (int pass = 0; pass < 3; ++pass) {
        Contact first;
        for (const auto& box : obstacles) {
            const auto hit = Sweep(box, result.feet, delta);
            if (hit.axis >= 0 && hit.time <= first.time) first = hit;
        }
        result.feet += first.time * delta;
        if (first.axis < 0) break;
        result.feet[first.axis] = first.plane;   // 면에 정확히 맞춘다 - 반올림 오차가 다음 프레임으로 쌓이지 않게
        if (first.axis == 1) {
            result.grounded = delta.y < 0;
            result.hitCeiling = delta.y > 0;
        }
        delta *= 1 - first.time;
        delta[first.axis] = 0;
    }
    return result;
}
}
