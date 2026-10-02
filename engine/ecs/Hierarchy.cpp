#include "Hierarchy.h"
#include "ECSRegistry.h"
#include "ComponentArray.h"
#include "../core/logging/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_set>

namespace Engine
{
    namespace
    {
        // 순환(또는 비정상적으로 깊은 계층)은 엔티티마다 한 번만 경고한다. 매 프레임 찍으면 로그가 묻힌다.
        void WarnDepthExceeded(Entity entity)
        {
            static std::mutex mutex;
            static std::unordered_set<EntityID> warned;
            std::lock_guard<std::mutex> lock(mutex);
            if (warned.insert(entity.id).second)
            {
                Logger::Log(LogLevel::Warning,
                    "Hierarchy - entity {} has more than {} ancestors; the parent chain is probably a cycle "
                    "(HierarchyComponent.parent set directly, not via SetParent). Treating the rest of the chain as root",
                    entity.id, kMaxHierarchyDepth);
            }
        }

        // entity부터 루트 방향으로 조상을 모은다(entity 자신 포함, [0] = entity). 깊이 제한에서 멈춘다.
        std::vector<Entity> ChainToRoot(ECSRegistry& registry, Entity entity)
        {
            std::vector<Entity> chain;
            Entity current = entity;
            while (current.IsValid())
            {
                if (static_cast<int>(chain.size()) > kMaxHierarchyDepth)
                {
                    WarnDepthExceeded(entity);
                    break;
                }
                chain.push_back(current);
                current = GetParent(registry, current);
            }
            return chain;
        }
    }

    glm::mat4 ComposeWorldMatrix(const TransformComponent& transform)
    {
        glm::vec3 position(transform.position.x, transform.position.y, transform.position.z);
        glm::quat rotation(transform.rotation.w, transform.rotation.x, transform.rotation.y, transform.rotation.z);
        glm::vec3 scale(transform.scale.x, transform.scale.y, transform.scale.z);

        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 r = glm::mat4_cast(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }

    Entity GetParent(ECSRegistry& registry, Entity entity)
    {
        const HierarchyComponent* h = registry.GetComponent<HierarchyComponent>(entity);
        if (!h || !registry.IsValid(h->parent))
        {
            return Entity();
        }
        return h->parent;
    }

    std::vector<Entity> GetChildren(ECSRegistry& registry, Entity parent)
    {
        std::vector<Entity> children;
        auto* hierarchies = registry.GetComponentArray<HierarchyComponent>();
        if (!hierarchies || !parent.IsValid())
        {
            return children;
        }
        for (size_t i = 0; i < hierarchies->Size(); ++i)
        {
            if (hierarchies->GetDenseArray()[i].parent == parent)
            {
                children.emplace_back(hierarchies->GetEntityIDs()[i]);
            }
        }
        std::sort(children.begin(), children.end(),
                  [](Entity a, Entity b) { return a.id < b.id; });
        return children;
    }

    std::vector<Entity> GetDescendants(ECSRegistry& registry, Entity root)
    {
        std::vector<Entity> result;
        std::unordered_set<EntityID> visited{ root.id };
        // 명시적 스택으로 전위 순회. 자식은 id 오름차순으로 방문하도록 역순으로 쌓는다.
        std::vector<Entity> stack;
        auto pushChildren = [&](Entity e) {
            auto children = GetChildren(registry, e);
            for (auto it = children.rbegin(); it != children.rend(); ++it)
            {
                stack.push_back(*it);
            }
        };
        pushChildren(root);
        while (!stack.empty())
        {
            Entity e = stack.back();
            stack.pop_back();
            if (!visited.insert(e.id).second)
            {
                continue;  // 순환이 직접 쓰여 있으면 같은 엔티티를 다시 만난다
            }
            result.push_back(e);
            pushChildren(e);
        }
        return result;
    }

    bool IsSelfOrAncestor(ECSRegistry& registry, Entity ancestor, Entity entity)
    {
        for (Entity e : ChainToRoot(registry, entity))
        {
            if (e == ancestor)
            {
                return true;
            }
        }
        return false;
    }

    TransformComponent DecomposeToTransform(const glm::mat4& m)
    {
        TransformComponent t;
        t.position = Vec3(m[3].x, m[3].y, m[3].z);
        glm::vec3 c0(m[0]), c1(m[1]), c2(m[2]);
        glm::vec3 s(glm::length(c0), glm::length(c1), glm::length(c2));
        if (glm::determinant(glm::mat3(m)) < 0.0f)
        {
            s.x = -s.x;
        }
        auto safeDiv = [](const glm::vec3& v, float d) { return std::abs(d) > 1e-8f ? v / d : v; };
        const glm::mat3 r(safeDiv(c0, s.x), safeDiv(c1, s.y), safeDiv(c2, s.z));
        const glm::quat q = glm::normalize(glm::quat_cast(r));
        t.rotation = Quaternion(q.x, q.y, q.z, q.w);
        t.scale = Vec3(s.x, s.y, s.z);
        return t;
    }

    std::expected<void, EngineError> SetParent(ECSRegistry& registry, Entity child, Entity parent,
                                               bool keepWorldTransform)
    {
        if (!registry.IsValid(child))
        {
            return MakeError(EngineErrorCode::EntityNotFound, "SetParent: child entity is not valid", "Hierarchy");
        }

        // 순환/유효성 검사를 통과한 뒤에만 적용한다(아래). 월드 행렬은 부모를 바꾸기 전에 잡아 둔다.
        const bool keepWorld = keepWorldTransform && registry.HasComponent<TransformComponent>(child);
        const glm::mat4 worldBefore = keepWorld ? ComputeWorldMatrix(registry, child) : glm::mat4(1.0f);
        auto applyWorld = [&]() {
            if (!keepWorld)
            {
                return;
            }
            const Entity newParent = GetParent(registry, child);
            const glm::mat4 parentWorld = newParent.IsValid() ? ComputeWorldMatrix(registry, newParent) : glm::mat4(1.0f);
            *registry.GetComponent<TransformComponent>(child) = DecomposeToTransform(glm::inverse(parentWorld) * worldBefore);
        };

        if (!parent.IsValid())
        {
            if (registry.HasComponent<HierarchyComponent>(child))
            {
                registry.RemoveComponent<HierarchyComponent>(child);
            }
            applyWorld();
            return {};
        }

        if (!registry.IsValid(parent))
        {
            return MakeError(EngineErrorCode::EntityNotFound, "SetParent: parent entity is not valid", "Hierarchy");
        }
        if (IsSelfOrAncestor(registry, child, parent))
        {
            return MakeError(EngineErrorCode::InvalidParameter,
                parent == child ? "SetParent: an entity cannot be its own parent"
                                : "SetParent: the new parent is a descendant of the child (would create a cycle)",
                "Hierarchy");
        }

        if (HierarchyComponent* h = registry.GetComponent<HierarchyComponent>(child))
        {
            h->parent = parent;
        }
        else
        {
            HierarchyComponent component;
            component.parent = parent;
            registry.AddComponent(child, component);
        }
        applyWorld();
        return {};
    }

    glm::mat4 ComputeWorldMatrix(ECSRegistry& registry, Entity entity)
    {
        glm::mat4 world(1.0f);
        const auto chain = ChainToRoot(registry, entity);
        // chain[0] = entity, 끝이 루트. 루트부터 곱한다: world = root * ... * parent * local
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            if (const TransformComponent* tc = registry.GetComponent<TransformComponent>(*it))
            {
                world = world * ComposeWorldMatrix(*tc);
            }
        }
        return world;
    }

    TransformComponent ComputeWorldTransform(ECSRegistry& registry, Entity entity)
    {
        const TransformComponent* local = registry.GetComponent<TransformComponent>(entity);
        TransformComponent world = local ? *local : TransformComponent();

        const Entity parent = GetParent(registry, entity);
        if (!parent.IsValid())
        {
            return world;
        }

        const glm::mat4 parentMatrix = ComputeWorldMatrix(registry, parent);
        const glm::vec4 p = parentMatrix * glm::vec4(world.position.x, world.position.y, world.position.z, 1.0f);
        world.position = Vec3(p.x, p.y, p.z);

        // 회전과 스케일은 조상 체인을 따라 누적한다(행렬 분해 없이).
        glm::quat rotation(world.rotation.w, world.rotation.x, world.rotation.y, world.rotation.z);
        glm::vec3 scale(world.scale.x, world.scale.y, world.scale.z);
        const auto chain = ChainToRoot(registry, parent);
        for (Entity ancestor : chain)
        {
            if (const TransformComponent* tc = registry.GetComponent<TransformComponent>(ancestor))
            {
                rotation = glm::quat(tc->rotation.w, tc->rotation.x, tc->rotation.y, tc->rotation.z) * rotation;
                scale *= glm::vec3(tc->scale.x, tc->scale.y, tc->scale.z);
            }
        }
        rotation = glm::normalize(rotation);
        world.rotation = Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
        world.scale = Vec3(scale.x, scale.y, scale.z);
        return world;
    }
}
