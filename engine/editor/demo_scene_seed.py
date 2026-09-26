"""
editor/demo_scene_seed.py

에디터가 엔진에 처음 연결될 때 빈 World에 넣는 검증용 기본 엔티티들.
(Main Camera, Test Cube, Barrel 프리팹, VFX Test, Fire 프리팹, Sound Test)

main.py에서 분리한 이유: 원래 이 코드는 EditorMainWindow._connect_engine_later() 안에서
World 활성화, 패널 연결과 함께 바깥 try 하나로 묶여 있었다. 그래서 Main Camera나
Test Cube 생성이 실패해도 로그에는 "ECS 연결 실패"가 찍혔다. 연결 자체는 이미 끝난
뒤였는데도 그랬다. 이 로그를 본 사람은 원인을 엔진 연결 쪽에서 찾게 된다. 또
카메라 생성이 실패하면 아래의 무관한 엔티티들까지 전부 건너뛰었다.

지금은 엔티티마다 독립된 단계로 실행한다. 한 단계가 실패해도 나머지는 계속 만들고,
로그에는 어느 단계가 어떤 예외로 실패했는지가 "[DemoSeed]" 태그와 함께 남는다.

Qt 의존성이 없다. editor_api, registry, log 콜백만 받으므로 가짜 객체로 단위 테스트할 수
있다(test_demo_scene_seed.py).
"""

import os
from typing import Callable, List, NamedTuple

from engine_binding import binding as ge_python

_ASSETS_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets")


class SeedResult(NamedTuple):
    name: str
    ok: bool
    detail: str   # 성공 시 부가 정보(빈 문자열 가능), 실패 시 "예외타입: 메시지"


# ── 개별 단계 ────────────────────────────────────────────────────────────────
# 각 함수는 성공 시 로그에 덧붙일 부가 정보 문자열을 돌려주고, 실패하면 예외를 그대로 던진다
# (잡는 건 seed_demo_scene()의 몫 - 단계 함수가 각자 삼키면 결과 집계가 틀어진다).

def _seed_main_camera(editor_api, registry) -> str:
    # ROADMAP.md P0-2: RenderSystem이 이 엔티티의 Transform을 매 프레임 읽어 뷰포트 카메라에
    # 반영한다 - engine/ecs/RenderSystem.cpp.
    camera_entity = editor_api.create_entity("Main Camera")
    editor_api.add_component(camera_entity, "TransformComponent")
    registry.SetTransformPosition(camera_entity, 0.0, 5.0, 15.0)
    editor_api.add_component(camera_entity, "CameraComponent")
    registry.SetComponentJson(
        camera_entity, "CameraComponent",
        '{"fov": 60.0, "nearPlane": 0.1, "farPlane": 1000.0, "isMainCamera": true}'
    )
    return ""


def _seed_test_cube(editor_api, registry) -> str:
    # 임시 검증용 엔티티 (ROADMAP.md P0-2: RenderSystem이 실제로 draw call을 내는지 눈으로
    # 확인하기 위한 것). RenderableComponent만 붙이면 meshHandle 기본값(0)에 대해
    # RenderSystem이 절차적 큐브로 대체해서 그려준다 - 실제 메시 에셋 파이프라인은 아직
    # 없음(RenderSystem.h 주석 참고).
    cube_entity = editor_api.create_entity("Test Cube")
    editor_api.add_component(cube_entity, "TransformComponent")
    editor_api.add_component(cube_entity, "RenderableComponent")
    return "RenderSystem 검증용"


def _seed_barrel_prefab(editor_api, registry) -> str:
    # 프리팹 시스템 Phase 2 실측 검증용 (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 2).
    # 원점의 수동 생성 "Test Cube"와 나란히 놓아서, 프리팹에서 스폰된 엔티티도 같은
    # RenderSystem 경로로 똑같이 그려지는지 한 화면에서 비교 확인한다.
    prefab_path = os.path.join(_ASSETS_DIR, "prefabs", "Barrel.prefab.json")
    entity = editor_api.instantiate_prefab(prefab_path, ge_python.Vec3(3.0, 0.0, 0.0))
    return f"entity id={entity.id}"


def _seed_vfx_test(editor_api, registry) -> str:
    # VFX Lite Phase 3 실측 검증용 (docs/VFX_LITE_PHASE3_RENDERING_PLAN.md §4).
    # Test Cube(원점)/Barrel(x=3) 옆인 x=-3에 지속 방출 이펙트를 하나 둔다 - 파티클이
    # 그려지는지, 그리고 기존 두 오브젝트가 그대로 나오는지(회귀)를 한 화면에서 같이 본다.
    #
    # 값은 눈으로 확인하기 쉬운 쪽으로 고른다: 위로 천천히 솟는 큰 입자.
    # maxParticle=64는 §5.3의 어림값(Rate 20 x Lifetime 2.0 = 40)보다 넉넉하다.
    fx_entity = editor_api.create_entity("VFX Test")
    editor_api.add_component(fx_entity, "TransformComponent")
    registry.SetTransformPosition(fx_entity, -3.0, 0.0, 0.0)
    editor_api.add_component(fx_entity, "ParticleEffectComponent")
    registry.SetComponentJson(
        fx_entity, "ParticleEffectComponent",
        '{"spawnRate": 20.0, "maxParticle": 64, "lifetime": 2.0, "burst": 0,'
        ' "initialSpeed": 1.5, "directionBase": [0.0, 1.0, 0.0],'
        ' "directionSpread": 40.0, "gravity": 0.0, "drag": 0.0,'
        ' "startSize": 0.4, "endSize": 0.1,'
        ' "startColor": [1.0, 0.6, 0.15], "startAlpha": 1.0,'
        ' "endColor": [0.9, 0.1, 0.0], "endAlpha": 0.0,'
        ' "startRotation": 0.0, "rotationSpeed": 0.0,'
        ' "speedVariance": 0.3, "sizeVariance": 0.2, "rotationVariance": 0.0}'
    )
    return "ParticleSystem 검증용"


def _seed_fire_prefab(editor_api, registry) -> str:
    # VFX Lite Phase 5 검증용 (docs/VFX_LITE_IMPLEMENTATION_PLAN.md §3 Phase 5).
    # 위 "VFX Test"가 손으로 만든 이펙트라면 이건 **프리팹에서 스폰된** 이펙트다 -
    # Test Cube(수동) 옆에 Barrel(프리팹)을 놓아 비교했던 것과 같은 구성이다.
    # §4.4의 "이펙트 하나 = 프리팹 하나로 두면 저장/스폰이 공짜로 따라온다"가 실제로
    # 성립하는지(= 프리팹에서 나온 이펙트도 똑같이 시뮬레이션되고 그려지는지)를 한 화면에서
    # 확인한다.
    fire_path = os.path.join(_ASSETS_DIR, "prefabs", "Fire.prefab.json")
    entity = editor_api.instantiate_prefab(fire_path, ge_python.Vec3(6.0, 0.0, 0.0))
    return f"entity id={entity.id}"


def _seed_sound_test(editor_api, registry) -> str:
    # Sound Lite Phase 6 검증용 (docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §3).
    # ActionPlayerComponent를 붙인 엔티티 하나를 만들어 두면, Play를 눌렀을 때 액션이
    # 재생되며 소리가 나는지 바로 확인할 수 있다. Motion Editor의 미리듣기와 달리 이건
    # **Scene Editor의 Play**에서 나는 소리다.
    sfx_entity = editor_api.create_entity("Sound Test")
    editor_api.add_component(sfx_entity, "TransformComponent")
    editor_api.add_component(sfx_entity, "ActionPlayerComponent")
    registry.SetComponentJson(
        sfx_entity, "ActionPlayerComponent",
        '{"action": "actions/SoundTest.action.json",'
        ' "playOnStart": true, "loop": true}'
    )
    return "Play에서 액션 재생 검증용"


# 생성 순서는 원래 main.py에 있던 순서 그대로다.
SEED_STEPS = [
    ("Main Camera", _seed_main_camera),
    ("Test Cube",   _seed_test_cube),
    ("Barrel",      _seed_barrel_prefab),
    ("VFX Test",    _seed_vfx_test),
    ("Fire",        _seed_fire_prefab),
    ("Sound Test",  _seed_sound_test),
]


def seed_demo_scene(editor_api, registry, log: Callable[[str], None]) -> List[SeedResult]:
    """SEED_STEPS를 순서대로 실행한다. 각 단계는 독립적이라 하나가 실패해도 나머지는 계속한다.

    Args:
        editor_api: ge_python.EditorAPI 인스턴스 (set_registry()가 이미 호출된 상태)
        registry:   같은 World의 ECSRegistry
        log:        한 줄짜리 메시지를 받는 콜백 (상태바 + 콘솔 등)
    Returns:
        단계별 SeedResult 목록 (호출부/테스트가 무엇이 실패했는지 확인하는 용도)
    """
    if editor_api is None or registry is None:
        # 호출 순서 버그(패널 연결 전에 시딩 호출)를 조용히 지나치지 않는다.
        raise ValueError(
            "seed_demo_scene(): editor_api와 registry가 모두 필요합니다 "
            f"(editor_api={editor_api!r}, registry={registry!r})"
        )

    results: List[SeedResult] = []
    for name, step in SEED_STEPS:
        try:
            detail = step(editor_api, registry)
            results.append(SeedResult(name, True, detail))
            log(f"[DemoSeed] {name} 생성 완료" + (f" ({detail})" if detail else ""))
        except Exception as e:
            detail = f"{type(e).__name__}: {e}"
            results.append(SeedResult(name, False, detail))
            log(f"[DemoSeed] {name} 생성 실패 - {detail}")
    return results
