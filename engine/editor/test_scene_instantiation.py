"""
scene_instantiation 유닛테스트 (Qt, 엔진 불필요).

기존 engine/editor/test_*.py 관례를 따른다(pytest 미사용, 직접 실행).
    python test_scene_instantiation.py
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from scene_instantiation import expand_scene_objects, instantiate_scene
from demo_scene_integration import DemoSceneIntegration


# ── expand_scene_objects ─────────────────────────────────────────────────────

def test_grid_expands_to_count_product_with_spacing():
    specs = expand_scene_objects([{
        'name': 'g', 'model': 'm.obj',
        'grid': {'count': [3, 1, 2], 'spacing': [2.0, 0.0, 5.0], 'origin': [-1.0, 0.0, 10.0]},
    }])
    assert len(specs) == 6
    positions = sorted(tuple(s.position) for s in specs)
    assert (-1.0, 0.0, 10.0) in positions and (3.0, 0.0, 15.0) in positions
    assert all(s.mesh_path == 'm.obj' for s in specs)
    assert specs[0].name == 'g[0,0,0]'


def test_instances_keep_their_transforms():
    specs = expand_scene_objects([{
        'name': 'p', 'model': 'p.obj',
        'instances': [{'position': [0, 3, 0], 'rotation': [0, 45, 0], 'scale': [2, 2, 2]}],
    }])
    assert len(specs) == 1
    s = specs[0]
    assert s.name == 'p' and list(s.position) == [0.0, 3.0, 0.0]
    assert list(s.rotation) == [0.0, 45.0, 0.0] and list(s.scale) == [2.0, 2.0, 2.0]


def test_missing_model_is_rejected():
    try:
        expand_scene_objects([{'name': 'x', 'model': ''}])
    except ValueError as e:
        assert "'x'" in str(e)
    else:
        raise AssertionError("model이 비면 ValueError여야 한다")


def test_real_scene_json_expands_to_101_entities():
    # engine/assets/scene.json: cube_grid 10x1x10 + center_pyramid 1
    scene_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                              "assets", "scene.json")
    integ = DemoSceneIntegration()
    integ.load_scene(scene_path)
    specs = expand_scene_objects(integ.get_scene_objects())
    assert len(specs) == 101, len(specs)
    # 모든 모델 파일이 실제로 존재해야 한다(에셋 루트 = engine/)
    engine_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for path in {s.mesh_path for s in specs}:
        assert os.path.exists(os.path.join(engine_dir, path)), path


# ── instantiate_scene ────────────────────────────────────────────────────────

class _FakeEntity:
    def __init__(self, i):
        self.id = i


class _FakeAPI:
    def __init__(self, fail_names=()):
        self.fail_names = set(fail_names)
        self.n = 0

    def create_entity(self, name):
        if name in self.fail_names:
            raise RuntimeError("boom")
        self.n += 1
        return _FakeEntity(self.n)

    def add_component(self, e, c):
        pass


class _FakeRegistry:
    def __init__(self):
        self.json_calls = []
        self.rotations = []

    def SetTransformPosition(self, *a): pass
    def SetTransformScale(self, *a): pass

    def SetTransformRotation(self, e, *r):
        self.rotations.append(r)

    def SetComponentJson(self, e, comp, js):
        self.json_calls.append((comp, json.loads(js)))


def test_instantiate_sets_mesh_path_and_continues_after_failure():
    specs = expand_scene_objects([
        {'name': 'a', 'model': 'a.obj', 'transform': {}},
        {'name': 'b', 'model': 'b.obj', 'instances': [{'rotation': [0, 90, 0]}]},
    ])
    reg = _FakeRegistry()
    logs = []
    results = instantiate_scene(_FakeAPI(fail_names={'a'}), reg, specs, logs.append)
    assert [r.ok for r in results] == [False, True]
    assert reg.json_calls == [("RenderableComponent", {"meshPath": "b.obj"})]
    assert reg.rotations == [(0.0, 90.0, 0.0)]          # 0이 아닌 회전만 전달
    assert any("a 생성 실패" in l for l in logs)
    assert logs[-1] == "[SceneLoad] scene.json 엔티티 1/2개 생성"



def test_texture_is_expanded_to_every_entity_and_sent_as_texture_path():
    specs = expand_scene_objects([
        {'name': 'g', 'model': 'c.obj', 'texture': 't.png',
         'grid': {'count': [2, 1, 1], 'spacing': [1, 0, 0]}},
    ])
    assert [s.texture_path for s in specs] == ['t.png', 't.png']
    reg = _FakeRegistry()
    instantiate_scene(_FakeAPI(), reg, specs, lambda _m: None)
    assert reg.json_calls == [("RenderableComponent", {"meshPath": "c.obj", "texturePath": "t.png"})] * 2

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
