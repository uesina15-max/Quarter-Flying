// Sound Lite Phase 5 — ActionPlayerComponent 리플렉션/직렬화 테스트.
// docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §3 Phase 5.
//
// 이 컴포넌트는 "이 엔티티는 이 액션을 재생한다"만 표현한다. 실제 재생은 Phase 6에서
// 파이썬 쪽 재생 루프가 하고(범위 정의서 §6.2의 (b) 확정), 엔진은 값만 들고 있는다 -
// VFX Lite의 ParticleEffectComponent가 설정만 들고 시뮬레이션은 ParticleSystem이 하는
// 것과 같은 구조다.

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"

using namespace Engine;

namespace
{
    const ComponentInfo* ActionPlayerInfo()
    {
        return ComponentRegistry::GetComponentInfo("ActionPlayerComponent");
    }
}

// ── 등록 및 필드 스키마 ────────────────────────────────────────────────────────

TEST(ActionPlayerComponentTest, IsRegisteredInComponentRegistry)
{
    const ComponentInfo* info = ActionPlayerInfo();
    ASSERT_NE(info, nullptr)
        << "ActionPlayerComponent가 등록되지 않았다 — Reflection.cpp의 InitializeReflection()이 "
           "RegisterActionPlayerComponentReflection()을 부르는지 확인할 것";
    EXPECT_EQ(info->name, "ActionPlayerComponent");
    EXPECT_NE(info->serialize, nullptr);
    EXPECT_NE(info->deserialize, nullptr);
    EXPECT_NE(info->patchField, nullptr);
}

TEST(ActionPlayerComponentTest, HasExactlyThreeFields)
{
    const ComponentInfo* info = ActionPlayerInfo();
    ASSERT_NE(info, nullptr);

    // 범위 정의서 §6.2.1이 "3개 필드로 끝낸다"고 확정했다. 늘어난다면 그건 액션 재생
    // 기능이 사운드 범위 밖으로 번지고 있다는 신호이므로 문서를 먼저 고쳐야 한다.
    EXPECT_EQ(info->fields.size(), 3u)
        << "필드 수가 바뀌었다 — docs/SOUND_LITE_PLAN.md §6.2.1을 함께 갱신했는지 확인할 것";
}

TEST(ActionPlayerComponentTest, NoFieldUsesEnumType)
{
    // 회귀 가드. GE_BEGIN_COMPONENT의 직렬화 switch에 FieldType::Enum case가 없어서
    // enum 필드는 예외도 에러도 없이 값이 누락된다(docs/VFX_LITE_PLAN.md §7.3).
    const ComponentInfo* info = ActionPlayerInfo();
    ASSERT_NE(info, nullptr);

    for (const auto& f : info->fields)
    {
        EXPECT_NE(f.type, FieldType::Enum)
            << "필드 '" << f.name << "'이 Enum이다 — 매크로 직렬화 경로가 Enum을 처리하지 "
               "않아 조용히 누락된다";
    }
}

// ── 직렬화 라운드트립 ──────────────────────────────────────────────────────────

TEST(ActionPlayerComponentTest, RoundTripsAllFieldsThroughJson)
{
    const ComponentInfo* info = ActionPlayerInfo();
    ASSERT_NE(info, nullptr);

    ECSRegistry registry;
    Entity source = registry.CreateEntity();

    // 기본값과 전부 다른 값으로 채운다 — 기본 생성자가 우연히 같은 값을 넣어줘서
    // "역직렬화가 아무 일도 안 해도 통과"하는 것을 막는다.
    ActionPlayerComponent original;
    original.action      = "assets/actions/SoundTest.action.json";
    original.playOnStart = false;   // 기본값 true와 다름
    original.loop        = false;   // 기본값 true와 다름
    registry.AddComponent(source, original);

    nlohmann::json out;
    info->serialize(registry, source, out);
    ASSERT_TRUE(out.contains("ActionPlayerComponent"));

    // 원본이 아닌 다른 엔티티로 복원한다 — 같은 엔티티에 덮어쓰면 값이 그대로 남아
    // 역직렬화가 아무 일도 안 해도 통과해버린다.
    Entity target = registry.CreateEntity();
    info->deserialize(registry, target, out["ActionPlayerComponent"]);

    ASSERT_TRUE(registry.HasComponent<ActionPlayerComponent>(target));
    const ActionPlayerComponent* restored = registry.GetComponent<ActionPlayerComponent>(target);
    ASSERT_NE(restored, nullptr);

    EXPECT_EQ(restored->action, original.action);
    EXPECT_EQ(restored->playOnStart, original.playOnStart);
    EXPECT_EQ(restored->loop, original.loop);
}

TEST(ActionPlayerComponentTest, PathWithBackslashesAndSpacesSurvives)
{
    // 액션 경로는 사용자가 파일 대화상자로 고른 실제 경로다. Windows 경로의 역슬래시가
    // JSON 이스케이프를 거쳐 그대로 돌아오는지 확인한다.
    const ComponentInfo* info = ActionPlayerInfo();
    ASSERT_NE(info, nullptr);

    ECSRegistry registry;
    Entity e = registry.CreateEntity();

    ActionPlayerComponent comp;
    comp.action = "C:\\Quarter Flying\\assets\\actions\\Sword Swing.action.json";
    registry.AddComponent(e, comp);

    nlohmann::json out;
    info->serialize(registry, e, out);

    Entity target = registry.CreateEntity();
    info->deserialize(registry, target, out["ActionPlayerComponent"]);

    EXPECT_EQ(registry.GetComponent<ActionPlayerComponent>(target)->action, comp.action);
}

TEST(ActionPlayerComponentTest, DeserializeCreatesComponentAndKeepsDefaultsForMissingFields)
{
    const ComponentInfo* info = ActionPlayerInfo();
    ASSERT_NE(info, nullptr);

    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    ASSERT_FALSE(registry.HasComponent<ActionPlayerComponent>(e));

    nlohmann::json data;
    data["action"] = "only_this.action.json";

    info->deserialize(registry, e, data);

    ASSERT_TRUE(registry.HasComponent<ActionPlayerComponent>(e));
    const ActionPlayerComponent* comp = registry.GetComponent<ActionPlayerComponent>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->action, "only_this.action.json");
    // JSON에 없던 필드는 기본 생성자 값을 유지해야 한다(부분 데이터 허용).
    EXPECT_EQ(comp->playOnStart, ActionPlayerComponent().playOnStart);
    EXPECT_EQ(comp->loop, ActionPlayerComponent().loop);
}

TEST(ActionPlayerComponentTest, PatchFieldUpdatesSingleField)
{
    // 에디터 Inspector가 필드 하나만 고칠 때 쓰는 경로.
    const ComponentInfo* info = ActionPlayerInfo();
    ASSERT_NE(info, nullptr);

    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, ActionPlayerComponent{});

    info->patchField(registry, e, StringHash("loop"), nlohmann::json(false));

    const ActionPlayerComponent* comp = registry.GetComponent<ActionPlayerComponent>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_FALSE(comp->loop);
    // 건드리지 않은 필드는 그대로여야 한다.
    EXPECT_EQ(comp->playOnStart, ActionPlayerComponent().playOnStart);
}

// ── 회귀 게이트 ────────────────────────────────────────────────────────────────

TEST(ActionPlayerComponentTest, AbsentComponentDoesNotAppearInSerializedOutput)
{
    // 신규 컴포넌트 타입을 등록한 것 때문에 그 컴포넌트를 갖지 않은 엔티티의 직렬화
    // 결과가 달라지면 안 된다.
    ECSRegistry registry;
    Entity e = registry.CreateEntity();

    TransformComponent t;
    t.position = Vec3(1.0f, 2.0f, 3.0f);
    registry.AddComponent(e, t);

    nlohmann::json snapshot = SerializeRegistry(registry);

    ASSERT_TRUE(snapshot.contains("entities"));
    ASSERT_EQ(snapshot["entities"].size(), 1u);
    const auto& comps = snapshot["entities"][0]["components"];

    EXPECT_TRUE(comps.contains("TransformComponent"));
    EXPECT_FALSE(comps.contains("ActionPlayerComponent"))
        << "액션 플레이어 컴포넌트가 없는 엔티티인데 직렬화 결과에 나타났다";
}

TEST(ActionPlayerComponentTest, DoesNotDisturbAIComponentActionFields)
{
    // 범위 정의서 §6.2.1 — AIComponent의 action 필드는 건드리지 않기로 했다.
    // 두 컴포넌트가 같은 엔티티에 공존해도 서로의 값을 침범하지 않는지 확인한다.
    ECSRegistry registry;
    Entity e = registry.CreateEntity();

    AIComponent ai;
    ai.idle_action   = "AI_Idle.action";
    ai.attack_action = "AI_Attack.action";
    registry.AddComponent(e, ai);

    ActionPlayerComponent player;
    player.action = "Player.action.json";
    registry.AddComponent(e, player);

    nlohmann::json snapshot = SerializeRegistry(registry);
    const auto& comps = snapshot["entities"][0]["components"];

    ASSERT_TRUE(comps.contains("AIComponent"));
    ASSERT_TRUE(comps.contains("ActionPlayerComponent"));
    EXPECT_EQ(comps["AIComponent"]["idle_action"], "AI_Idle.action");
    EXPECT_EQ(comps["ActionPlayerComponent"]["action"], "Player.action.json");
}
