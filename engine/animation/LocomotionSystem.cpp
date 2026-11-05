#include "LocomotionSystem.h"
#include "ClipJsonIO.h"
#include "SkeletonJsonIO.h"
#include "AnimationSampler.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Hierarchy.h"
#include "../core/Utf8Path.h"
#include "../core/logging/Logger.h"
#include <fstream>
#include <iterator>
#include <cmath>

namespace Engine {
void LocomotionSystem::Load(Player& player, const std::array<std::string,4>& paths) {
    player = {};
    player.paths = paths;
    if (paths[0].empty()) return;
    auto read = [&](const std::string& name) {
        std::ifstream file(PathFromUtf8(assetRoot) / PathFromUtf8(name), std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    auto rig = ParseSkeletonJson(read(paths[0]));
    if (!rig) { Logger::Warning("Locomotion: invalid skeleton {}", paths[0]); return; }
    player.rig = std::move(*rig);
    for (size_t slot = 0; slot < player.clips.size(); ++slot) {
        if (paths[slot+1].empty()) continue;
        auto clip = ParseClipJson(read(paths[slot+1]), *player.rig);
        if (clip) player.clips[slot] = std::move(*clip);
        else Logger::Warning("Locomotion: invalid clip {}", paths[slot+1]);
    }
}

void LocomotionSystem::Update(ECSRegistry& registry, float dt) {
    if (!(dt > 0) || !std::isfinite(dt)) return;
    auto* components = registry.GetComponentArray<LocomotionComponent>();
    if (!components) { players.clear(); return; }
    std::erase_if(players, [&](const auto& entry) { return !components->Has(entry.first); });
    for (EntityID id : components->GetEntityIDs()) {
        Entity actor(id);
        const auto& config = *registry.GetComponent<LocomotionComponent>(actor);
        auto& player = players[id];
        std::array paths{config.skeletonPath, config.idleClip, config.walkClip, config.runClip};
        if (paths != player.paths) Load(player, paths);
        if (!player.rig) continue;
        glm::vec3 position(ComputeWorldMatrix(registry, actor)[3]);
        const float speed = player.sampled ? std::hypot(position.x-player.previous.x, position.z-player.previous.z)/dt : 0;
        player.previous = position;
        player.sampled = true;
        int state = speed <= config.walkThreshold ? 0 : (speed >= config.runThreshold ? 2 : 1);
        while (state > 0 && !player.clips[state]) --state;
        const auto& clip = player.clips[state];
        if (!clip || !clip->totalFrames) continue;
        player.frame = player.state == state ? std::fmod(player.frame + dt*clip->fps, static_cast<float>(clip->totalFrames)) : 0;
        player.state = state;
        const Pose pose = Sample(*player.rig, *clip, player.frame);
        for (Entity joint : GetDescendants(registry, actor)) {
            const int bone = player.rig->FindBoneIndex(registry.GetEntityName(joint));
            auto* local = registry.GetTransformComponent(joint);
            if (!local || bone < 0) continue;
            const auto& sample = pose[bone];
            local->position = {sample.position.x,sample.position.y,sample.position.z};
            local->rotation = {sample.rotation.x,sample.rotation.y,sample.rotation.z,sample.rotation.w};
            // 크기는 건드리지 않는다. 키프레임에 크기가 없어서 sample.scale은 본의 바인드 크기(거의 항상 1)일 뿐이다.
            // 증상: 팔로 쓰는 자식 큐브(크기 0.2, 0.6, 0.2)가 Play를 누르는 순간 (1, 1, 1)로 커졌다. 에러 없음.
        }
    }
}
}
