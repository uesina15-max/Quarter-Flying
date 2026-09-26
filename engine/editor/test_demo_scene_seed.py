"""
demo_scene_seed.seed_demo_scene() + engine_binding.to_entity() 유닛테스트.

seed_demo_scene()은 가짜 editor_api/registry로 검증한다(Qt, 엔진 불필요).
to_entity()는 실제 엔진 모듈이 import 가능할 때만 검증하고, 없으면 건너뛴다.
엔진 모듈을 쓰려면 engine/build/Release를 PYTHONPATH에 넣고 실행한다.

기존 engine/editor/test_*.py 관례를 따른다(pytest 미사용, 직접 실행).
    python test_demo_scene_seed.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import demo_scene_seed
from demo_scene_seed import seed_demo_scene, SEED_STEPS
import engine_binding


# ── 가짜 엔진 객체 ────────────────────────────────────────────────────────────

class _FakeEntity:
    def __init__(self, eid):
        self.id = eid


class _FakeEditorAPI:
    def __init__(self, fail_on_create=()):
        self._next = 1
        self.fail_on_create = set(fail_on_create)
        self.created = []

    def create_entity(self, name):
        if name in self.fail_on_create:
            raise RuntimeError(f"boom:{name}")
        self.created.append(name)
        e = _FakeEntity(self._next)
        self._next += 1
        return e

    def add_component(self, entity, comp):
        pass

    def instantiate_prefab(self, path, pos):
        self.created.append(os.path.basename(path))
        e = _FakeEntity(self._next)
        self._next += 1
        return e


class _FakeRegistry:
    def SetTransformPosition(self, *a):
        pass

    def SetComponentJson(self, *a):
        pass


class _FakeVec3:
    def __init__(self, *a):
        pass


class _FakeBinding:
    Vec3 = _FakeVec3


def _with_fake_binding(fn):
    """demo_scene_seed가 참조하는 ge_python을 테스트 동안만 가짜로 바꾼다."""
    def wrapper():
        saved = demo_scene_seed.ge_python
        demo_scene_seed.ge_python = _FakeBinding
        try:
            fn()
        finally:
            demo_scene_seed.ge_python = saved
    wrapper.__name__ = fn.__name__
    return wrapper


# ── seed_demo_scene ──────────────────────────────────────────────────────────

@_with_fake_binding
def test_all_steps_succeed():
    logs = []
    results = seed_demo_scene(_FakeEditorAPI(), _FakeRegistry(), logs.append)
    assert [r.name for r in results] == [n for n, _ in SEED_STEPS]
    assert all(r.ok for r in results), results
    assert len(logs) == len(SEED_STEPS)
    assert all(l.startswith("[DemoSeed]") for l in logs)


@_with_fake_binding
def test_one_failure_does_not_stop_others():
    # 예전 main.py에서는 Main Camera 생성이 실패하면 뒤의 엔티티를 전부 건너뛰고
    # "ECS 연결 실패"로 기록했다. 지금은 실패한 단계만 실패로 남아야 한다.
    api = _FakeEditorAPI(fail_on_create={"Main Camera"})
    logs = []
    results = seed_demo_scene(api, _FakeRegistry(), logs.append)
    by_name = {r.name: r for r in results}
    assert not by_name["Main Camera"].ok
    assert "RuntimeError" in by_name["Main Camera"].detail
    assert "boom:Main Camera" in by_name["Main Camera"].detail
    for name in ("Test Cube", "Barrel", "VFX Test", "Fire", "Sound Test"):
        assert by_name[name].ok, (name, by_name[name])
    fail_logs = [l for l in logs if "실패" in l]
    assert len(fail_logs) == 1 and "Main Camera" in fail_logs[0], fail_logs
    assert not any("ECS 연결" in l for l in logs)


@_with_fake_binding
def test_missing_args_raise_clearly():
    for api, reg in ((None, _FakeRegistry()), (_FakeEditorAPI(), None)):
        try:
            seed_demo_scene(api, reg, lambda m: None)
        except ValueError as e:
            assert "editor_api와 registry" in str(e)
        else:
            raise AssertionError("None 인자에서 ValueError가 나야 한다")


# ── to_entity ────────────────────────────────────────────────────────────────

def _expect(exc_type, fn, *args, contains=""):
    try:
        fn(*args)
    except exc_type as e:
        assert contains in str(e), (contains, str(e))
    else:
        raise AssertionError(f"{exc_type.__name__}가 나야 한다: {args!r}")


def test_to_entity_without_engine_raises_runtime_error():
    saved = engine_binding.HAS_ENGINE
    engine_binding.HAS_ENGINE = False
    try:
        _expect(RuntimeError, engine_binding.to_entity, 3, contains="HAS_ENGINE")
    finally:
        engine_binding.HAS_ENGINE = saved


def test_to_entity_with_real_engine():
    if not engine_binding.HAS_ENGINE:
        print("  (skip: 엔진 모듈 없음 - PYTHONPATH에 engine/build/Release 추가 시 실행)")
        return
    Entity = engine_binding.binding.Entity
    to_entity = engine_binding.to_entity

    e = to_entity(5)
    assert isinstance(e, Entity) and e.id == 5
    assert to_entity(e) is e                       # 이미 Entity면 그대로
    assert to_entity(0xFFFFFFFF).id == 0xFFFFFFFF   # uint32 상한

    _expect(ValueError, to_entity, -1, contains="선택 없음")
    _expect(ValueError, to_entity, 0x1_0000_0000, contains="uint32")
    _expect(TypeError, to_entity, True, contains="bool")
    _expect(TypeError, to_entity, "3", contains="str")
    _expect(TypeError, to_entity, None, contains="NoneType")


# ── 실행 ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    tests = [v for k, v in list(globals().items()) if k.startswith("test_") and callable(v)]
    failed = 0
    for t in tests:
        try:
            t()
            print(f"PASS  {t.__name__}")
        except Exception as e:
            failed += 1
            print(f"FAIL  {t.__name__}: {type(e).__name__}: {e}")
    print(f"\n{len(tests) - failed}/{len(tests)} passed")
    sys.exit(1 if failed else 0)
