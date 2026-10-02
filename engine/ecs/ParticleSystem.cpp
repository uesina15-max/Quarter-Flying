#include "ParticleSystem.h"
#include "ECSRegistry.h"
#include "Hierarchy.h"
#include "../renderer/InstancedBatchManager.h"
#include "../renderer/Camera.h"
#include "../renderer/Mesh.h"
#include "../renderer/Vertex.h"
#include "../core/logging/Logger.h"
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Engine
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;

        // 파티클 전용 배치 키(Phase 3 계획서 §3.2). meshGuid/materialId는 다른 메시 핸들과
        // 절대 겹치지 않도록 큰 상수를 쓴다("VFXLITE"/"VFXQUAD"의 ASCII).
        //   - materialId는 지금 아무 의미가 없다. 나중에 텍스처가 생기면 그 핸들이 들어갈
        //     자리로 예약해둔 것이다(§2.1) - 배치 키 구조와 분리 규칙은 그때도 안 바뀐다.
        //   - passType이 ForwardTransparent라서, 기존 SceneMeshRenderer의
        //     RenderBatches(ForwardOpaque, ...)는 이 배치를 아예 건드리지 않는다.
        //     이것이 "기존 렌더 경로 무영향"의 실질적 근거다.
        const InstancedBatchKey& ParticleBatchKey()
        {
            static const InstancedBatchKey key = []
            {
                InstancedBatchKey k;
                k.meshGuid       = 0x5646585155414421ull;  // "VFXQUAD!"
                k.materialId     = 0x5646584C49544521ull;  // "VFXLITE!"
                k.shaderId       = 0;                      // 셰이더는 RenderBatches 인자로 넘어간다
                k.passType       = PassType::ForwardTransparent;
                k.materialLayout = MaterialLayout::Unlit;
                k.features       = PipelineFeature::None;
                return k;
            }();
            return key;
        }

        glm::vec3 ToGlm(const Vec3& v)
        {
            return glm::vec3(v.x, v.y, v.z);
        }

        float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        // [-1, 1] 균등 분포. "±20%" 같은 변동폭(§2.6)을 만드는 데 쓴다.
        float SymmetricUnit(std::mt19937& rng)
        {
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            return dist(rng);
        }

        // directionBase를 축으로 하는 원뿔 안에서 방향 하나를 고른다.
        //
        // 범위 정의서 §2.2가 Direction을 "고정 벡터 하나가 아니라 기준 방향 + 퍼짐 각도"로
        // 정의했기 때문에 필요한 함수다. spreadDegrees == 0이면 기준 방향 그대로(스파크처럼
        // 좁은 분사), 360에 가까우면 전방위(폭발/먼지)가 된다 — 파라미터 하나로 양 극단을
        // 모두 표현하는 것이 §2.2의 의도다.
        //
        // 반각 기준으로 cos(theta)를 [cos(halfAngle), 1]에서 균등하게 뽑는다. 이렇게 해야
        // 원뿔 표면에 고르게 퍼진다 — theta를 각도로 직접 균등 추출하면 중심축 쪽에
        // 뭉친다.
        glm::vec3 SampleDirectionInCone(const glm::vec3& baseDir, float spreadDegrees, std::mt19937& rng)
        {
            glm::vec3 axis = baseDir;
            float len = glm::length(axis);
            if (len < 1e-6f)
            {
                // 기준 방향이 영벡터면 방향을 정의할 수 없다. 위쪽을 기본으로 삼는다
                // (조용히 0벡터 속도를 만들어 파티클이 제자리에 뭉치는 것보다 낫다).
                axis = glm::vec3(0.0f, 1.0f, 0.0f);
            }
            else
            {
                axis /= len;
            }

            float halfAngle = std::clamp(spreadDegrees, 0.0f, 360.0f) * 0.5f * (kPi / 180.0f);
            halfAngle = std::min(halfAngle, kPi);
            if (halfAngle <= 0.0f)
            {
                return axis;
            }

            std::uniform_real_distribution<float> u01(0.0f, 1.0f);
            float cosHalf  = std::cos(halfAngle);
            float cosTheta = Lerp(cosHalf, 1.0f, u01(rng));
            float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
            float phi      = u01(rng) * 2.0f * kPi;

            // axis에 수직인 정규직교 기저 두 개.
            glm::vec3 helper = (std::fabs(axis.x) < 0.9f) ? glm::vec3(1.0f, 0.0f, 0.0f)
                                                          : glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 tangent   = glm::normalize(glm::cross(helper, axis));
            glm::vec3 bitangent = glm::cross(axis, tangent);

            return axis * cosTheta + (tangent * std::cos(phi) + bitangent * std::sin(phi)) * sinTheta;
        }
    }

    ParticleSystem::ParticleSystem(InstancedBatchManager* batchManager, Camera* camera)
        : batchManager_(batchManager)
        , camera_(camera)
    {
    }

    std::vector<size_t> ParticleSystem::GetReadComponentTypes() const
    {
        return { typeid(ParticleEffectComponent).hash_code() };
    }

    const EffectInstance* ParticleSystem::FindEffect(EntityID owner) const
    {
        auto it = effects_.find(owner);
        return (it == effects_.end()) ? nullptr : &it->second;
    }

    size_t ParticleSystem::GetActiveEffectCount() const
    {
        size_t n = 0;
        for (const auto& pair : effects_)
        {
            if (pair.second.active)
            {
                ++n;
            }
        }
        return n;
    }

    size_t ParticleSystem::GetTotalAliveParticles() const
    {
        size_t n = 0;
        for (const auto& pair : effects_)
        {
            n += pair.second.particles.size();
        }
        return n;
    }

    void ParticleSystem::Update(ECSRegistry& registry, float deltaTime)
    {
        SyncEffectsWithRegistry(registry);

        // deltaTime <= 0이면 스폰도 적분도 하지 않는다(일시정지/첫 프레임). 다만 아래
        // 인스턴스 수집은 건너뛰지 않는다 - 일시정지 중에도 이미 살아있는 파티클은
        // 계속 화면에 보여야 하기 때문이다.
        if (deltaTime > 0.0f)
        {
            for (auto& pair : effects_)
            {
                EffectInstance& effect = pair.second;

                // §5.8: 비활성 이펙트는 Update 자체를 스킵한다. Burst 이펙트가 "터짐 → 전부
                // 소멸" 이후 매 프레임 빈 배열을 검사하는 것조차 하지 않는 것이 목표다.
                if (!effect.active)
                {
                    continue;
                }

                SpawnParticles(effect, deltaTime);
                IntegrateAndRemoveDead(effect, deltaTime);
                UpdateActiveState(effect);
            }
        }

        CollectInstances();
    }

    // ========================================================================
    // 인스턴스 수집 (Phase 3B)
    // ========================================================================

    std::shared_ptr<Mesh> ParticleSystem::CreateQuadMesh()
    {
        // XY 평면의 단위 쿼드, 중심이 원점. UV가 핵심이다 - 파티클 프래그먼트 셰이더가
        // 이 UV로 절차적 원형 마스크를 만든다(텍스처를 쓰지 않기로 한 §2.1 결정).
        // normal은 파티클 셰이더가 읽지 않지만 Vertex 레이아웃(attribute 1)이 요구한다.
        const std::vector<Vertex> vertices = {
            { -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f },
            {  0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f },
            {  0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f },
            { -0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f },
        };
        const std::vector<unsigned int> indices = { 0, 1, 2,  2, 3, 0 };

        auto mesh = std::make_shared<Mesh>();
        mesh->create(vertices, indices);
        return mesh;
    }

    bool ParticleSystem::EnsureBatch()
    {
        if (batchReady_)
        {
            return true;
        }

        if (!quadMesh_)
        {
            quadMesh_ = CreateQuadMesh();
        }

        // CreateBatch()의 Result를 무시하지 않는다. 실패한 배치를 그대로 두면
        // instanceVAO == 0인 배치가 맵에 남아 RenderBatch()의 방어 체크에 걸린다 -
        // 애초에 그 상황을 안 만든다.
        auto result = batchManager_->CreateBatch(ParticleBatchKey(), BatchType::Dynamic, quadMesh_);
        if (!result)
        {
            Logger::Log(LogLevel::Error,
                "ParticleSystem::EnsureBatch - CreateBatch failed: {} (파티클이 이 프레임에 "
                "수집되지 않는다)", result.error().message);
            return false;
        }

        batchReady_ = true;
        Logger::Log(LogLevel::Info, "ParticleSystem::EnsureBatch - Particle instance batch created");
        return true;
    }

    void ParticleSystem::CollectInstances()
    {
        // 유닛테스트(ParticleSystem()) 경로는 둘 다 nullptr이라 여기서 조용히 빠진다 -
        // Phase 2의 테스트가 그대로 통과하는 이유다.
        if (!batchManager_ || !camera_)
        {
            return;
        }

        if (!EnsureBatch())
        {
            return;
        }

        // AddInstance는 중복 방지 없이 append라, 매 프레임 비우지 않으면 인스턴스가
        // 계속 누적된다. ClearInstances는 vector::clear라 capacity를 유지하므로,
        // 이 배치의 instanceData가 곧 "재사용되는 CPU 인스턴스 버퍼"다(§5.6이 요구한
        // "매 프레임 새로 만들지 않는다"를 별도 버퍼 없이 만족한다).
        batchManager_->ClearInstances(ParticleBatchKey());

        // 빌보드 회전은 프레임당 1회만 구해서 모든 파티클이 재사용한다. 뷰 행렬 회전부의
        // 전치가 곧 그 역회전이라, 쿼드가 항상 카메라를 마주보게 된다.
        const glm::mat4 billboard = glm::transpose(glm::mat4(glm::mat3(camera_->getViewMatrix())));

        for (const auto& pair : effects_)
        {
            const EffectInstance& effect = pair.second;
            if (!effect.active)
            {
                continue;
            }

            for (const Particle& p : effect.particles)
            {
                InstanceData data{};

                // model = T(위치) * R_billboard * R_z(회전) * S(크기).
                // 크기는 월드 단위다(Phase 3 계획서 §8 ②) - 카메라에서 멀어지면 작아진다.
                glm::mat4 model = glm::translate(glm::mat4(1.0f), p.position);
                model = model * billboard;
                model = glm::rotate(model, glm::radians(p.rotation), glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, glm::vec3(p.size));

                data.model = model;
                data.color = glm::vec3(p.color);
                data.alpha = p.color.a;   // Phase 0에서 추가한 attribute 12

                // entityId/isSelected/roughness/metallic은 파티클 셰이더가 읽지 않는다.
                // 파티클은 ECS 엔티티가 아니라 피킹 대상도 아니므로 0으로 둔다.
                auto added = batchManager_->AddInstance(ParticleBatchKey(), data, 0);
                if (!added)
                {
                    Logger::Log(LogLevel::Warning,
                        "ParticleSystem::CollectInstances - AddInstance failed: {}",
                        added.error().message);
                    return;  // 배치가 사라진 상황. 이 프레임은 포기한다.
                }
            }
        }
    }

    void ParticleSystem::SyncEffectsWithRegistry(ECSRegistry& registry)
    {
        auto* components = registry.GetComponentArray<ParticleEffectComponent>();

        // 컴포넌트가 사라진(엔티티 삭제/컴포넌트 제거) 이펙트를 먼저 정리한다.
        for (auto it = effects_.begin(); it != effects_.end();)
        {
            const bool stillExists = components && components->Get(it->first) != nullptr;
            it = stillExists ? std::next(it) : effects_.erase(it);
        }

        if (!components)
        {
            return;
        }

        // 이미터의 현재 월드 위치를 매 프레임 갱신한다. 스폰은 이 값을 시작 위치로 굽고
        // (월드 공간 시뮬레이션, Phase 3 계획서 §3.10), 이미 뿜은 파티클은 이미터가
        // 움직여도 따라오지 않는다.
        auto* transforms = registry.GetComponentArray<TransformComponent>();
        for (auto& pair : effects_)
        {
            if (transforms && transforms->Get(pair.first))
            {
                // 부모 아래 이미터(예: 횃불 프리팹의 불꽃 자식)는 월드 위치에서 뿜는다(Hierarchy.h).
                const Vec3 p = ComputeWorldTransform(registry, Entity(pair.first)).position;
                pair.second.emitterWorldPos = glm::vec3(p.x, p.y, p.z);
            }
            // TransformComponent가 없으면 직전 값을 유지한다(기본값 원점). 이미터에
            // Transform이 없는 것은 설정 실수에 가깝지만, 여기서 파티클을 원점으로
            // 순간이동시키는 것보다는 낫다.
        }

        for (size_t i = 0; i < components->Size(); ++i)
        {
            EntityID id = components->GetEntityIDs()[i];
            if (effects_.find(id) != effects_.end())
            {
                continue;
            }

            // §5.7: 이펙트 단위 전역 상한. 이펙트 하나가 조절할 값이 아니라 엔진 정책이라
            // 조용히 넘기지 않고 여기서 막는다.
            if (effects_.size() >= kMaxActiveEffects)
            {
                break;
            }

            const ParticleEffectComponent& settings = components->GetDenseArray()[i];

            EffectInstance effect;
            effect.owner    = id;
            effect.settings = settings;
            effect.rng.seed(static_cast<std::mt19937::result_type>(id * 2654435761u + 1u));

            // §5.5: 여기서 딱 한 번. 이후 Spawn/Update/Remove 어디에서도 재할당이 없어야
            // 한다. maxParticle이 음수/0으로 들어오는 경우를 여기서 정규화해 두면 아래
            // 상한 계산이 전부 안전해진다. Preview 상한(§5.10)을 여기에도 적용하는 것이
            // 중요하다 - 안 그러면 Max Particle=100000짜리 오설정 하나가, 실제로는 2048개만
            // 쓸 거면서 100000개 분량(약 5.6MB)을 미리 잡아버린다.
            const size_t capacity = std::min(
                static_cast<size_t>(std::max(0, settings.maxParticle)), kPreviewMaxParticle);
            effect.particles.reserve(capacity);

            // 새 이펙트는 스폰 전에 이미터 위치를 한 번 맞춰둔다 - 안 그러면 생성된 첫
            // 프레임의 파티클만 원점에서 튀어나온다.
            if (transforms && transforms->Get(id))
            {
                const Vec3 p = ComputeWorldTransform(registry, Entity(id)).position;
                effect.emitterWorldPos = glm::vec3(p.x, p.y, p.z);
            }

            effects_.emplace(id, std::move(effect));
        }
    }

    size_t ParticleSystem::SpawnBudget(EffectInstance& effect)
    {
        // §5.2: Max Particle은 게임플레이 옵션이 아니라 하드 리소스 상한이다. 초과분은
        // "신규 생성 거부"로 처리하고, 이미 살아있는 파티클은 절대 강제 소멸시키지 않는다
        // (순환 버퍼 금지 — 폭발/피격처럼 짧은 이펙트에서 파티클이 수명을 못 채우고 갑자기
        // 사라지는 부자연스러움을 만들기 때문).
        size_t maxParticle = static_cast<size_t>(std::max(0, effect.settings.maxParticle));

        // §5.10의 에디터 Preview 하드 상한. 사용자가 무엇을 넣든 이펙트 하나가 이보다
        // 많이 갖지 못한다. 조용히 다르게 동작하면 안 되므로 실제로 걸릴 때 한 번 알린다.
        if (maxParticle > kPreviewMaxParticle)
        {
            if (!effect.previewCapWarned)
            {
                effect.previewCapWarned = true;
                Logger::Log(LogLevel::Warning,
                    "ParticleSystem - entity {}: Max Particle {} exceeds the editor preview cap {}; "
                    "this effect is limited to {} particles",
                    effect.owner, maxParticle, kPreviewMaxParticle, kPreviewMaxParticle);
            }
            maxParticle = kPreviewMaxParticle;
        }

        const size_t alive = effect.particles.size();
        if (alive >= maxParticle)
        {
            return 0;
        }
        size_t budget = maxParticle - alive;

        // §5.7: 전역 파티클 상한. 개별 이펙트의 무분별한 중복 발동으로 전체 프레임이
        // 무너지는 것을 막는 두 번째 방어선이다.
        const size_t total = GetTotalAliveParticles();
        if (total >= kMaxTotalParticles)
        {
            return 0;
        }
        budget = std::min(budget, kMaxTotalParticles - total);

        return budget;
    }

    void ParticleSystem::SpawnParticles(EffectInstance& effect, float deltaTime)
    {
        // Burst는 이펙트당 한 번(§5.11). Rate와 동시에 쓸 수 있다(§2.1의 "Burst + Spawn Rate
        // 동시 사용 확정") — 폭발 순간 파편을 Burst로 뿜으면서 잔불 연기를 Rate로 계속
        // 방출하는 식이다.
        if (!effect.burstFired)
        {
            effect.burstFired = true;
            const int burst = std::max(0, effect.settings.burst);
            for (int i = 0; i < burst; ++i)
            {
                if (SpawnBudget(effect) == 0)
                {
                    break;
                }
                SpawnOne(effect);
            }
        }

        const float rate = effect.settings.spawnRate;
        if (rate <= 0.0f)
        {
            return;
        }

        // 구현 계획서 §2.8. 소수부를 누적하는 이유: dt가 작을 때 rate*dt < 1이라고 매 프레임
        // 0개를 만들면 지속 방출이 아예 멈춘다.
        effect.spawnAccumulator += rate * deltaTime;
        int pending = static_cast<int>(effect.spawnAccumulator);
        if (pending <= 0)
        {
            return;
        }
        effect.spawnAccumulator -= static_cast<float>(pending);

        for (int i = 0; i < pending; ++i)
        {
            // 상한에 걸려 못 만든 몫은 **누적하지 않고 버린다**. 누적해두면 파티클이 죽어
            // 자리가 나는 순간 밀린 물량이 한꺼번에 터져서, §5.2가 원한 "초과분은 그냥
            // 생성되지 않음"이 아니라 "초과분이 나중에 몰아서 생성됨"이 되어버린다.
            if (SpawnBudget(effect) == 0)
            {
                break;
            }
            SpawnOne(effect);
        }
    }

    void ParticleSystem::SpawnOne(EffectInstance& effect)
    {
        const ParticleEffectComponent& s = effect.settings;

        Particle p{};

        // 월드 공간 시뮬레이션(Phase 3 계획서 §3.10, 확정): 스폰하는 "그 순간의" 이미터
        // 위치를 파티클에 구워 넣는다. 이후 이미터가 움직여도 이 파티클은 여기 남는다.
        p.position = effect.emitterWorldPos;

        const glm::vec3 dir   = SampleDirectionInCone(ToGlm(s.directionBase), s.directionSpread, effect.rng);
        const float     speed = s.initialSpeed * (1.0f + SymmetricUnit(effect.rng) * s.speedVariance);
        p.velocity = dir * speed;

        p.lifetime  = std::max(0.0f, s.lifetime);
        p.sizeScale = std::max(0.0f, 1.0f + SymmetricUnit(effect.rng) * s.sizeVariance);
        p.size      = s.startSize * p.sizeScale;
        p.rotation  = s.startRotation + SymmetricUnit(effect.rng) * s.rotationVariance;
        p.color     = glm::vec4(ToGlm(s.startColor), s.startAlpha);

        // reserve()된 용량 안에서만 push_back한다 — SpawnBudget()이 maxParticle을 넘지
        // 않도록 이미 막았으므로 여기서 재할당이 일어나면 안 된다(§5.5). 유닛테스트가
        // capacity 불변을 검사한다.
        effect.particles.push_back(p);
    }

    void ParticleSystem::IntegrateAndRemoveDead(EffectInstance& effect, float deltaTime)
    {
        const ParticleEffectComponent& s = effect.settings;
        const float maxLifetime = std::max(1e-6f, s.lifetime);

        // 속도 감쇠(Drag). 범위 정의서 §4.1의 스케치는 `velocity *= drag`라고 적혀 있지만
        // 그대로 쓰면 drag의 기본값 0.1에서 매 프레임 속도의 90%가 사라지고, drag=0이면
        // 파티클이 즉시 멈춘다 — 즉 "감쇠 계수"(§2.2)라는 이름과 정반대로 동작한다. 게다가
        // 프레임률에 따라 결과가 달라진다. 여기서는 dt에 비례하는 감쇠로 구현한다:
        // drag=0이면 감쇠 없음, drag=0.1이면 초당 10% 감쇠.
        const float dragFactor = std::clamp(1.0f - s.drag * deltaTime, 0.0f, 1.0f);

        size_t i = 0;
        while (i < effect.particles.size())
        {
            Particle& p = effect.particles[i];

            p.lifetime -= deltaTime;
            if (p.lifetime <= 0.0f)
            {
                // §5.5: swap-remove로 O(1) 제거. erase()로 배열을 밀지 않는다.
                // 파티클 순서는 시뮬레이션 결과에 영향을 주지 않으므로 안전하다.
                // 마지막 원소를 당겨온 자리는 다시 검사해야 하므로 i를 증가시키지 않는다.
                p = effect.particles.back();
                effect.particles.pop_back();
                continue;
            }

            // §4.1의 적분 순서 그대로.
            p.velocity.y += s.gravity * deltaTime;
            p.velocity   *= dragFactor;
            p.position   += p.velocity * deltaTime;

            // t = 1 - life/maxLife  (0 = 갓 태어남, 1 = 소멸 직전)
            const float t = std::clamp(1.0f - (p.lifetime / maxLifetime), 0.0f, 1.0f);

            p.size      = Lerp(s.startSize, s.endSize, t) * p.sizeScale;
            p.rotation += s.rotationSpeed * deltaTime;
            p.color     = glm::vec4(
                Lerp(s.startColor.x, s.endColor.x, t),
                Lerp(s.startColor.y, s.endColor.y, t),
                Lerp(s.startColor.z, s.endColor.z, t),
                Lerp(s.startAlpha,   s.endAlpha,   t));

            ++i;
        }
    }

    void ParticleSystem::UpdateActiveState(EffectInstance& effect)
    {
        // §5.11: 이펙트 자체의 Lifetime 파라미터는 없다. Burst 이펙트는 마지막 파티클이
        // 죽는 순간 여기서 자동으로 끝나고, 지속형(Rate>0)은 Rate가 꺼지거나 컴포넌트가
        // 제거되기 전까지 계속 살아있다.
        const bool stillEmitting = effect.settings.spawnRate > 0.0f;
        effect.active = stillEmitting || !effect.particles.empty();
    }
}
