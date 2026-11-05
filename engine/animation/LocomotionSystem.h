#pragma once
#include "../ecs/System.h"
#include "../ecs/Entity.h"
#include "Skeleton.h"
#include "KeyframeClip.h"
#include <array>
#include <optional>
#include <unordered_map>

namespace Engine {
struct LocomotionComponent {
    std::string skeletonPath, idleClip, walkClip, runClip;
    float walkThreshold = 0.05f;
    float runThreshold = 5.f;
};

class LocomotionSystem final : public System {
public:
    explicit LocomotionSystem(const std::string& projectRoot) : assetRoot(projectRoot) {}
    void SetAssetRoot(const std::string& root) { assetRoot = root; players.clear(); }
    void Update(ECSRegistry& registry, float dt) override;
    void OnStart(ECSRegistry&) override { players.clear(); }
    void OnStop(ECSRegistry&) override { players.clear(); }
    const char* GetName() const override { return "LocomotionSystem"; }
    bool RunsInEditMode() const override { return false; }
    bool CanRunInParallel() const override { return false; }
    int GetPriority() const override { return -15; }
private:
    struct Player {
        std::array<std::string,4> paths;
        std::optional<Skeleton> rig;
        std::array<std::optional<KeyframeClip>,3> clips;
        glm::vec3 previous{};
        bool sampled = false;
        int state = -1;
        float frame = 0;
    };
    void Load(Player& player, const std::array<std::string,4>& paths);
    std::string assetRoot;
    std::unordered_map<EntityID,Player> players;
};
}
