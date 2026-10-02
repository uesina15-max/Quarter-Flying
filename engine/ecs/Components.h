#pragma once

#include "../core/Types.h"
#include "Entity.h"
#include <string>  // AIComponent가 std::string을 쓰는데 이 헤더 자체는 그동안 <string>을
                   // 직접 include하지 않고 있었다 - 이걸 포함하는 다른 헤더가 항상 먼저
                   // <string>을 끌어와 준 덕에 우연히 컴파일됐던 것으로 보인다(RenderSystem.h가
                   // 그 우연이 깨지는 첫 include 순서였다 - C2039).

namespace Engine
{
    // ========================================
    // Example Components
    // ========================================
    
    // 예제 컴포넌트들
    // 실제 게임에서는 프로젝트별로 정의
    
    // Transform Component
    struct TransformComponent
    {
        Vec3 position;
        Quaternion rotation;
        Vec3 scale;

        TransformComponent()
            : position(0.0f, 0.0f, 0.0f)
            , rotation(0.0f, 0.0f, 0.0f, 1.0f)
            , scale(1.0f, 1.0f, 1.0f)
        {}
    };

    // Renderable Component
    struct RenderableComponent
    {
        uint32_t meshHandle;
        uint32_t materialHandle;
        bool castShadows;
        // 에셋 루트(EngineConfig::assetRoot) 기준 메시 파일 경로(예: "assets/models/cube.obj").
        // 비어 있지 않으면 meshHandle보다 우선한다 - RenderSystem::ResolveMeshHandle 참고.
        std::string meshPath;
        // 에셋 루트 기준 텍스처 파일 경로(예: "assets/textures/checker.png"). 비어 있으면 텍스처 없이
        // 인스턴스 색만으로 그린다. 로드/업로드는 RenderSystem::ResolveTexture가 처음 볼 때 한 번 한다.
        std::string texturePath;

        RenderableComponent()
            : meshHandle(0)
            , materialHandle(0)
            , castShadows(true)
        {}
    };

    // Camera Component
    struct CameraComponent
    {
        float fov;
        float nearPlane;
        float farPlane;
        bool isMainCamera;   // 활성 카메라 후보인가
        int priority;        // 후보 중 가장 높은 값이 활성 카메라가 된다(SelectActiveCamera, CameraSystem.h)
        float blendInSeconds; // 이 카메라가 활성이 될 때 이전 시점에서 넘어오는 시간(0 = 즉시 전환)

        // Cached matrices (flat array to avoid glm dependency in headers)
        float viewMatrix[16];
        float projMatrix[16];

        CameraComponent()
            : fov(45.0f)
            , nearPlane(0.1f)
            , farPlane(100.0f)
            , isMainCamera(false)
            , priority(0)
            , blendInSeconds(0.0f)
        {
            for(int i=0; i<16; ++i) {
                viewMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
                projMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
            }
        }
    };

    // AI State Enum
    enum class AIState {
        Idle,
        Patrol,
        Chase,
        Attack,
        Return
    };

    // AI Component
    struct AIComponent
    {
        bool enabled;
        float sight_range;
        float attack_range;
        std::string idle_action;
        std::string attack_action;
        std::string hit_action;
        AIState default_state;

        AIComponent()
            : enabled(true)
            , sight_range(10.0f)
            , attack_range(2.0f)
            , idle_action("Idle.action")
            , attack_action("Attack.action")
            , hit_action("Hit.action")
            , default_state(AIState::Patrol)
        {}
    };

    // ========================================
    // Sound Lite — Action Player Component
    // ========================================

    // "이 엔티티는 이 액션을 재생한다"만 표현하는 컴포넌트
    // (docs/SOUND_LITE_PLAN.md §6.2.1, docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §2.3).
    //
    // Scene Editor의 Play에서 액션이 재생되려면 "어느 엔티티가 어느 액션을 재생하는가"가
    // 데이터로 있어야 하는데, 이 저장소에는 그게 없었다. AIComponent가
    // idle_action/attack_action/hit_action과 AIState를 이미 갖고 있지만 **읽는 시스템이
    // 하나도 없고**, 그걸 재활용하면 "Play를 눌렀을 때 이 엔티티가 왜 Attack 상태인가"에
    // 답해야 한다 - 상태 전이 규칙 -> 조건 -> 타깃 탐색으로 이어지는 AI 시스템 작업이고
    // 사운드와 아무 상관이 없다. 그래서 목적이 하나인 작은 컴포넌트를 따로 둔다.
    // AIComponent의 action 필드는 건드리지 않는다 - 나중에 AI 시스템이 생기면 그때
    // 이 컴포넌트와 어떻게 연결할지 정한다.
    //
    // 이 3개 필드는 범위 정의서 §2의 **사운드 값 3개(Clip/Volume/Volume ±)와 별개 축**이다.
    // 사운드 파라미터가 6개가 된 것이 아니다.
    //
    // FieldType::Enum을 쓰지 않는다 - GE_BEGIN_COMPONENT가 만드는 직렬화 switch에 Enum
    // case가 없어서 조용히 누락된다(docs/VFX_LITE_PLAN.md §7.3). 세 필드 모두
    // String/Bool이라 해당 없음.
    struct ActionPlayerComponent
    {
        std::string action;    // 재생할 .action.json 경로
        bool playOnStart;      // Play 진입 시 자동 재생
        bool loop;             // 끝나면 처음으로 되감기

        ActionPlayerComponent()
            : action("")
            , playOnStart(true)
            , loop(true)
        {}
    };

    // ========================================
    // 카메라 리그 (docs/INGAME_CAMERA_PLAN.md C5, C6) - CameraRigSystem이 처리한다.
    // 셋 다 카메라 엔티티(CameraComponent + TransformComponent)에 붙여서 그 Transform을 계산한다.
    // Play(PIE)에서만 동작한다(CameraRigSystem::RunsInEditMode() == false).
    // target은 EntityRef라 UUID로 직렬화되어 프리팹/PIE 스냅샷을 통과한다.
    // ========================================

    // ========================================
    // 계층(부모-자식) - 프리팹 Phase 5 (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 5)
    // 부모만 저장한다. 자식 목록은 GetChildren()이 조회 시 계산한다(ecs/Hierarchy.h). 자식 배열까지
    // 들고 있으면 두 쪽이 어긋날 수 있고(부모 쪽만 갱신 등), 리플렉션이 배열 필드를 직렬화하지도 못한다.
    // TransformComponent는 부모 기준 로컬 값이 된다. 월드 값은 ComputeWorldMatrix/ComputeWorldTransform.
    // 부모 설정은 순환 검사를 하는 SetParent()로 한다. 이 컴포넌트가 없거나 parent가 무효면 루트다.
    // ========================================
    struct HierarchyComponent
    {
        Entity parent;
    };

    // C5: 대상 위치 + offset으로 따라간다. damping은 시간 상수(초)로 0이면 즉시 붙는다.
    struct CameraFollowComponent
    {
        Entity target;
        Vec3 offset;
        float damping;

        CameraFollowComponent() : target(), offset(0.0f, 3.0f, 8.0f), damping(0.2f) {}
    };

    // C5: 대상(+targetOffset)을 바라보도록 회전만 정한다. Follow와 같이 쓰면 "따라가며 바라보기"가 된다.
    struct CameraLookAtComponent
    {
        Entity target;
        Vec3 targetOffset;

        CameraLookAtComponent() : target(), targetOffset(0.0f, 1.0f, 0.0f) {}
    };

    // C6: 플레이어가 마우스로 대상 주위를 돌리는 카메라(3인칭 궤도). 입력은 Engine의 InputState.
    // 이 컴포넌트가 있으면 같은 엔티티의 Follow/LookAt보다 우선한다(위치와 회전을 모두 정함).
    struct CameraOrbitControlComponent
    {
        Entity target;
        Vec3 targetOffset;
        float distance;
        float minDistance;
        float maxDistance;
        float yaw;              // 도. 0이면 대상의 +Z 쪽에서 바라본다
        float pitch;            // 도. +면 위에서 내려다본다
        float sensitivity;      // 마우스 1픽셀당 회전(도)
        float zoomPerStep;      // 휠 한 칸당 거리 배율(1보다 작으면 가까워짐)
        bool requireRightMouse; // true면 우클릭을 누른 동안만 회전

        CameraOrbitControlComponent()
            : target(), targetOffset(0.0f, 1.0f, 0.0f), distance(10.0f), minDistance(2.0f), maxDistance(50.0f)
            , yaw(0.0f), pitch(20.0f), sensitivity(0.3f), zoomPerStep(0.85f), requireRightMouse(true) {}
    };

    // ========================================
    // VFX Lite — Particle Effect Component
    // ========================================

    // 파티클 "이펙트 하나"의 설정값만 담는다. 개별 파티클은 여기 없다 --
    // docs/VFX_LITE_PLAN.md §5.1대로 파티클은 ECS 엔티티도 컴포넌트도 아니고
    // ParticleSystem이 소유하는 EffectInstance 내부의 연속 배열로만 존재한다.
    //
    // 필드가 20개인데 범위 정의서 §2의 파라미터는 18개다. 늘어난 게 아니라,
    // Direction("기준 방향 + 퍼짐 각도")과 Alpha Fade("수명에 따른 투명도 변화")가
    // 각각 값 하나를 필드 두 개로 표현하기 때문이다
    // (docs/VFX_LITE_IMPLEMENTATION_PLAN.md §2.4의 대조표).
    //
    // 색상이 Vec3 + Float로 쪼개져 있는 이유: FieldType에 Vec4/Color가 없다
    // (docs/VFX_LITE_PLAN.md §7.3). 리플렉션에 Color 타입을 추가하는 대신 여기서
    // 쪼갠다 -- FieldType을 늘리면 GE_BEGIN_COMPONENT의 serialize/deserialize/
    // patchField switch 세 곳과, 필드 타입을 정수 인덱스로 하드코딩하고 있는
    // editor/panels/inspector.py까지 함께 건드려야 한다.
    //
    // FieldType::Enum을 쓰는 필드는 일부러 하나도 없다 -- GE_BEGIN_COMPONENT의
    // 직렬화 switch에 Enum case가 없어서, enum 필드는 예외도 에러도 없이 조용히
    // 저장이 누락된다(AIComponent는 매크로를 안 쓰고 직렬화 람다를 손으로 써서
    // 이 구멍을 피해간 경우다). ParticleEffectComponentTests가 이걸 회귀 검사한다.
    struct ParticleEffectComponent
    {
        // 생성 (Spawn) — 파라미터 4개 / 필드 4개
        float spawnRate;         // 초당 생성 개수
        int   maxParticle;       // 동시 존재 가능한 최대 개수 (하드 상한, §5.2)
        float lifetime;          // 파티클 하나의 수명(초)
        int   burst;             // 한 번에 터뜨리는 개수

        // 이동 (Motion) — 파라미터 4개 / 필드 5개 (Direction이 2필드)
        float initialSpeed;
        Vec3  directionBase;     // 기준 발사 방향
        float directionSpread;   // 퍼짐 각도(도). 0=한 방향, 360에 가까울수록 전방위
        float gravity;
        float drag;

        // 크기 (Size) — 파라미터 2개 / 필드 2개
        float startSize;
        float endSize;

        // 색상 (Color) — 파라미터 3개 / 필드 4개 (Alpha Fade가 2필드)
        Vec3  startColor;
        float startAlpha;
        Vec3  endColor;
        float endAlpha;

        // 회전 (Rotation) — 파라미터 2개 / 필드 2개
        float startRotation;
        float rotationSpeed;

        // 랜덤 (Random) — 파라미터 3개 / 필드 3개
        float speedVariance;     // 0.2 = ±20%
        float sizeVariance;
        float rotationVariance;

        // 기본값은 범위 정의서 §4.3의 에디터 폼 예시(작은 불꽃)와 §5.3의
        // "Max: 256" 예시를 그대로 따른다.
        ParticleEffectComponent()
            : spawnRate(30.0f)
            , maxParticle(256)
            , lifetime(0.8f)
            , burst(10)
            , initialSpeed(3.0f)
            , directionBase(0.0f, 1.0f, 0.0f)
            , directionSpread(30.0f)
            , gravity(0.0f)
            , drag(0.1f)
            , startSize(0.2f)
            , endSize(0.0f)
            , startColor(1.0f, 0.5f, 0.1f)
            , startAlpha(1.0f)
            , endColor(0.8f, 0.1f, 0.0f)
            , endAlpha(0.0f)
            , startRotation(0.0f)
            , rotationSpeed(0.0f)
            , speedVariance(0.2f)
            , sizeVariance(0.1f)
            , rotationVariance(0.0f)
        {}
    };

} // namespace Engine
