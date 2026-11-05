"""
core/viewport_manipulation 테스트 (docs/VIEWPORT_MANIPULATION_PLAN.md).

앞쪽은 가짜 엔진으로 클릭/드래그 흐름과 평면 계산을 본다(엔진·Qt 불필요). 마지막 테스트는 실제 엔진 모듈로
피킹 → 드래그 → Undo 한 번을 확인한다(Release 모듈이 PYTHONPATH에 있고 GL 창을 만들 수 있을 때만, 아니면 SKIP).
기존 engine/editor/test_*.py 관례를 따른다(pytest 미사용, 직접 실행).
    python test_viewport_manipulation.py
"""

import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.viewport_manipulation import (DragPlane, ViewportManipulator, intersect_plane, MAX_DRAG_DISTANCE,
                                       closest_axis_parameter, quat_multiply, quat_axis_angle,
                                       MODE_MOVE, MODE_ROTATE, MODE_SCALE, HANDLE_UNIFORM)


def _close(a, b, eps=1e-4):
    return all(abs(x - y) < eps for x, y in zip(a, b))


# ── 가짜 엔진 ────────────────────────────────────────────────────────────────

class _Entity:
    def __init__(self, id_): self.id = id_
    def IsValid(self): return self.id != 0


class _Registry:
    def __init__(self): self.pos, self.rot, self.scale, self.parent = {}, {}, {}, {}
    def HasTransformComponent(self, e): return e.id in self.pos
    def GetComponentJson(self, e, name):
        return json.dumps({"scale": list(self.scale.get(e.id, (1.0, 1.0, 1.0)))})


class _Commands:
    def __init__(self): self.begun = self.ended = 0
    def begin_merge_session(self): self.begun += 1
    def end_merge_session(self): self.ended += 1


class _Binding:
    def __init__(self, registry):
        self.registry, self.commands = registry, _Commands()
        outer = self
        class CommandManager:
            @staticmethod
            def get_instance(): return outer.commands
        self.CommandManager = CommandManager
    def GetWorldPosition(self, registry, e): return registry.pos[e.id]
    def GetWorldRotation(self, registry, e): return registry.rot.get(e.id, (0.0, 0.0, 0.0, 1.0))
    def GetParent(self, registry, e): return registry.parent.get(e.id, _Entity(0))
    def LocalPositionForWorld(self, registry, e, x, y, z): return (x, y, z)
    @staticmethod
    def Vec3(x, y, z): return (x, y, z)
    @staticmethod
    def Quaternion(x, y, z, w): return (x, y, z, w)


class _Api:
    def __init__(self, registry, fail=False): self.registry, self.fail, self.moves = registry, fail, []
    def move_entity(self, e, v):
        if self.fail:
            raise RuntimeError("Editing is locked during Play/Pause")
        self.moves.append(v)
        self.registry.pos[e.id] = v
    def rotate_entity(self, e, q): self.moves.append(q); self.registry.rot[e.id] = q   # 부모 없는 경우만 쓰인다
    def scale_entity(self, e, s): self.moves.append(s); self.registry.scale[e.id] = s


def _oblique_ray(ndc_x, ndc_y):
    """(x, 10, 10) 근처에서 45도로 내려다보는 광선 - 수평면·수직면 모두와 만난다."""
    d = (0.0, -1.0 / math.sqrt(2.0), -1.0 / math.sqrt(2.0))
    return (ndc_x * 10.0, 10.0 + ndc_y * 10.0, 10.0), d


class _Engine:
    def __init__(self, hit=None, gizmo=None):
        self.hit, self.gizmo, self.highlights = hit, gizmo, []
    def PickEntityAtScreen(self, x, y): return self.hit
    def GetViewRay(self, x, y): return _oblique_ray(x, y)
    def PickGizmo(self, x, y): return self.gizmo
    def SetGizmoHighlight(self, axis): self.highlights.append(axis)
    def SetGizmoMode(self, mode): self.mode = mode


# 가짜 광선(_oblique_ray)의 화면 가운데가 큐브 윗면(y=0.5)과 만나는 점.
GRAB = (0.0, 0.5, 0.5)


_AXES = {1: (1.0, 0.0, 0.0), 2: (0.0, 1.0, 0.0), 3: (0.0, 0.0, 1.0), 4: (0.0, 0.0, 0.0)}


def _setup(hit_point=GRAB, fail=False, picked=True, gizmo_axis=0, gizmo_mode=MODE_MOVE):
    registry = _Registry()
    cube = _Entity(7)
    registry.pos[7] = (0.0, 0.0, 0.0)
    binding = _Binding(registry)
    api = _Api(registry, fail)
    selected, moved, errors = [], [], []
    hit = {"entity": cube, "distance": 5.0, "point": hit_point} if picked else None
    gizmo = ({"handle": gizmo_axis, "mode": gizmo_mode, "entity": cube, "origin": (0.0, 0.0, 0.0),
              "axis": _AXES[gizmo_axis], "length": 1.0} if gizmo_axis else None)
    engine = _Engine(hit, gizmo)
    m = ViewportManipulator(engine, registry, api, binding, selected.append, moved.append, errors.append)
    m.fake_engine = engine
    return m, registry, binding, api, selected, moved, errors


# ── 평면 계산 ────────────────────────────────────────────────────────────────

def test_intersect_plane_rejects_parallel_behind_and_far():
    assert _close(intersect_plane((0, 10, 0), (0, -1, 0), (0, 2, 0), (0, 1, 0)), (0, 2, 0))
    assert intersect_plane((0, 10, 0), (1, 0, 0), (0, 2, 0), (0, 1, 0)) is None      # 나란함
    assert intersect_plane((0, 10, 0), (0, 1, 0), (0, 2, 0), (0, 1, 0)) is None      # 뒤쪽
    d = (1.0, -1e-4, 0.0)
    n = math.sqrt(sum(v * v for v in d))
    assert intersect_plane((0, 10, 0), tuple(v / n for v in d), (0, 2, 0), (0, 1, 0)) is None   # MAX_DRAG_DISTANCE 너머
    assert MAX_DRAG_DISTANCE > 100


def test_horizontal_plane_keeps_height_and_follows_grab_offset():
    # 큐브 윗면(y=0.5)을 잡았다 - 원점(y=0)은 그대로 y=0에 남고 X·Z만 잡은 점의 이동만큼 움직인다.
    plane = DragPlane(GRAB, (0.0, 0.0, 0.0), False, _oblique_ray(0, 0)[1])
    o, d = _oblique_ray(0.2, 0.0)
    w = plane.world_position(o, d)
    assert _close(w, (2.0, 0.0, 0.0)), w


def test_vertical_plane_changes_only_height():
    plane = DragPlane(GRAB, (0.0, 0.0, 0.0), True, _oblique_ray(0, 0)[1])
    o, d = _oblique_ray(0.3, 0.1)   # 오른쪽으로도 움직였지만 X는 무시된다
    w = plane.world_position(o, d)
    # 수직면 z=0.5에서 광선 높이 1.5 - 잡은 점(0.5)보다 1.0 위 -> 원점도 1.0 올라간다
    assert _close(w, (0.0, 1.0, 0.0)), w


# ── 클릭/드래그 흐름 ─────────────────────────────────────────────────────────

def test_click_without_moving_only_selects():
    m, registry, binding, api, selected, moved, _ = _setup()
    m.press(0, 0, (100, 100))
    m.move(0.001, 0, (102, 101), False)   # 문턱값(4px) 미만
    m.release()
    assert selected == [7]
    assert api.moves == [] and moved == []
    assert binding.commands.begun == 0 and binding.commands.ended == 0


def test_empty_click_deselects():
    m, _, binding, api, selected, moved, _ = _setup(picked=False)
    m.press(0, 0, (100, 100))
    m.move(0.5, 0, (300, 100), False)
    m.release()
    assert selected == [-1] and api.moves == [] and binding.commands.begun == 0


def test_drag_is_one_merge_session_and_reports_move():
    m, registry, binding, api, selected, moved, _ = _setup()
    m.press(0, 0, (100, 100))
    for i in range(1, 6):
        m.move(0.04 * i, 0, (100 + 10 * i, 100), False)
    m.release()
    assert binding.commands.begun == 1 and binding.commands.ended == 1
    assert len(api.moves) == 5
    assert _close(registry.pos[7], (2.0, 0.0, 0.0)), registry.pos[7]
    assert moved == [7]


def test_switching_shift_mid_drag_does_not_jump():
    m, registry, *_ = _setup()
    m.press(0, 0, (100, 100))
    m.move(0.2, 0, (140, 100), False)
    before = registry.pos[7]
    m.move(0.2, 0, (140, 100), True)       # 같은 커서 위치에서 Shift만 눌렀다
    assert _close(registry.pos[7], before), (registry.pos[7], before)
    m.move(0.2, 0.1, (140, 80), True)      # 이제 위로만
    after = registry.pos[7]
    assert _close((after[0], after[2]), (before[0], before[2])) and after[1] > before[1], (before, after)
    m.release()


def test_rejected_move_ends_drag_and_closes_session():
    m, registry, binding, api, _, moved, errors = _setup(fail=True)
    m.press(0, 0, (100, 100))
    m.move(0.1, 0, (130, 100), False)
    m.move(0.2, 0, (160, 100), False)      # 이미 끝났으니 다시 시도하지 않는다
    assert binding.commands.begun == 1 and binding.commands.ended == 1
    assert len(errors) == 1 and "locked" in errors[0]
    assert not m.dragging
    m.release()
    assert binding.commands.ended == 1


# ── 기즈모 축 드래그 ─────────────────────────────────────────────────────────

def test_closest_axis_parameter():
    # 축 X(원점), 광선이 x=3에서 아래로 -> s = 3
    assert abs(closest_axis_parameter((0, 0, 0), (1, 0, 0), (3, 10, 0), (0, -1, 0)) - 3.0) < 1e-6
    # 시선과 나란한 축은 끄는 방향이 없다
    assert closest_axis_parameter((0, 0, 0), (0, -1, 0), (0, 10, 0), (0, -1, 0)) is None
    # 가장 가까운 곳이 카메라 뒤
    assert closest_axis_parameter((0, 0, 0), (1, 0, 0), (3, 10, 0), (0, 1, 0)) is None


def test_gizmo_axis_drag_moves_only_along_that_axis_without_reselecting():
    m, registry, binding, api, selected, moved, _ = _setup(gizmo_axis=1)   # X 화살표
    m.press(0, 0, (100, 100))
    assert selected == []                       # 화살표는 고른 엔티티에 붙어 있다 - 선택을 다시 하지 않는다
    assert m.fake_engine.highlights == [1]      # 누른 축 강조
    m.move(0.2, 0.15, (140, 70), True)          # 위로도 움직이고 Shift도 눌렀지만 X로만
    p = registry.pos[7]
    assert abs(p[0] - 2.0) < 1e-4 and abs(p[1]) < 1e-9 and abs(p[2]) < 1e-9, p
    m.release()
    assert moved == [7] and binding.commands.begun == 1 and binding.commands.ended == 1
    assert m.fake_engine.highlights[-1] == 0    # 놓으면 강조를 내리고 다음 hover가 다시 정한다


def test_vertical_axis_drag_follows_cursor_height():
    m, registry, *_ = _setup(gizmo_axis=2)      # Y 화살표
    m.press(0, 0, (100, 100))
    m.move(0.3, 0.1, (160, 80), False)
    p = registry.pos[7]
    assert abs(p[0]) < 1e-9 and abs(p[2]) < 1e-9 and p[1] > 0.5, p
    m.release()


def test_hover_highlights_axis_and_clears():
    m, *_ = _setup(gizmo_axis=3)
    m.hover(0.1, 0.1)
    m.hover(0.1, 0.1)                           # 같은 축이면 다시 보내지 않는다
    m.fake_engine.gizmo = None
    m.hover(0.9, 0.9)
    assert m.fake_engine.highlights == [3, 0]


# ── 회전·크기 ────────────────────────────────────────────────────────────────

def _rotate_vec(q, v):
    p = quat_multiply(quat_multiply(q, (v[0], v[1], v[2], 0.0)), (-q[0], -q[1], -q[2], q[3]))
    return p[:3]


def test_rotate_ring_drag_turns_around_world_axis():
    # Y 고리. 가짜 광선은 45도로 내려다보므로 수평면(y=0)과 잘 만난다(화면 대체 경로를 타지 않는다).
    m, registry, binding, api, *_ = _setup(gizmo_axis=2, gizmo_mode=MODE_ROTATE)
    # 누른 점: 광선(0, 10, 10)->y=0 평면은 (0, 0, 0)이라 원점이다 - 원점이 아닌 곳에서 누르려고 x를 비킨다
    m.press(0.3, 0.0, (100, 100))     # 고리 위 (3, 0, 0) 근처
    m.move(0.0, -0.3, (100, 130), False)   # (0, 0, 3) 쪽: +X에서 +Z로 = Y축 기준 -90도
    q = registry.rot[7]
    x_axis = _rotate_vec(q, (1.0, 0.0, 0.0))
    assert _close(x_axis, (0.0, 0.0, 1.0), 1e-3), (q, x_axis)
    m.release()
    assert binding.commands.begun == 1 and binding.commands.ended == 1


def test_rotate_keeps_child_world_rotation_consistent_with_parent():
    # 부모가 Y축으로 90도 돌아 있고 자식 로컬은 항등(월드 = P). 월드 X축으로 30도 돌리면 결과 월드 = R * P,
    # 저장할 로컬 = inverse(P) * R * P 이다.
    m, registry, *_ = _setup(gizmo_axis=1, gizmo_mode=MODE_ROTATE)
    parent = _Entity(3)
    p = quat_axis_angle((0.0, 1.0, 0.0), math.radians(90))
    registry.parent[7], registry.rot[3], registry.rot[7] = parent, p, p
    m.press(0.0, 0.3, (100, 100))
    assert m._begin_drag(False, (0.0, -1.0, 0.0))
    m._drag.screen_fallback = True                         # 각도를 화면 이동량으로 정해 계산을 고정한다
    local = m._drag.value(None, None, (100 + 60, 100))     # 60px * 0.5도 = 30도
    world = quat_multiply(p, local)
    expected = quat_multiply(quat_axis_angle((1.0, 0.0, 0.0), math.radians(30)), p)
    assert _close(world, expected, 1e-5) or _close(world, tuple(-v for v in expected), 1e-5), (world, expected)
    m.release()


def test_scale_handle_scales_one_component_and_center_scales_all():
    m, registry, binding, api, *_ = _setup(gizmo_axis=1, gizmo_mode=MODE_SCALE)
    registry.scale[7] = (2.0, 1.0, 1.0)
    m.press(0.0, 0.0, (100, 100))
    m.move(0.1, 0.0, (130, 100), False)    # X축 위로 1(손잡이 길이)만큼 -> 두 배
    assert _close(registry.scale[7], (4.0, 1.0, 1.0)), registry.scale[7]
    m.move(-5.0, 0.0, (0, 100), False)     # 반대로 아주 멀리 - 0 아래로 가지 않는다
    assert registry.scale[7][0] > 0 and registry.scale[7][1:] == (1.0, 1.0), registry.scale[7]
    m.release()

    m, registry, *_ = _setup(gizmo_axis=HANDLE_UNIFORM, gizmo_mode=MODE_SCALE)
    m.press(0.0, 0.0, (100, 100))
    m.move(0.0, 0.0, (175, 25), False)     # 오른쪽 75 + 위 75 = 150px -> 두 배
    assert _close(registry.scale[7], (2.0, 2.0, 2.0)), registry.scale[7]
    m.release()


def test_body_drag_only_moves_in_move_mode():
    m, registry, binding, api, selected, *_ = _setup()
    m.set_mode(MODE_ROTATE)
    assert m.fake_engine.mode == MODE_ROTATE
    m.press(0.0, 0.0, (100, 100))
    m.move(0.3, 0.0, (160, 100), False)
    m.release()
    assert selected == [7] and api.moves == []      # 고르기만 한다
    m.set_mode(MODE_MOVE)
    m.press(0.0, 0.0, (100, 100))
    m.move(0.3, 0.0, (160, 100), False)
    m.release()
    assert len(api.moves) == 1


# ── 실제 엔진 ────────────────────────────────────────────────────────────────

def test_real_engine_pick_drag_and_single_undo():
    try:
        from engine_binding import binding as qf, HAS_ENGINE
        if not HAS_ENGINE:
            raise ImportError("엔진 모듈 없음")
        from PySide6.QtWidgets import QApplication, QWidget
    except ImportError as e:
        print(f"SKIP  test_real_engine_pick_drag_and_single_undo: {e}")
        return
    app = QApplication.instance() or QApplication([])
    host = QWidget()
    host.resize(320, 240)
    engine = qf.Engine()
    config = qf.EngineConfig()
    config.windowWidth, config.windowHeight = 320, 240
    try:
        engine.InitializeFromWindowHandle(int(host.winId()), config)
    except RuntimeError as e:
        print(f"SKIP  test_real_engine_pick_drag_and_single_undo: GL 창을 만들 수 없음({e})")
        return
    try:
        world = engine.CreateWorld("Viewport manipulation")
        engine.SetActiveWorld(world)
        registry = world.GetRegistry()
        api = qf.EditorAPI.get_instance()
        api.set_registry(registry)
        commands = qf.CommandManager.get_instance()
        cube = api.create_entity("Cube")
        api.add_component(cube, "TransformComponent")
        api.add_component(cube, "RenderableComponent")
        engine.SetViewCamera(qf.ViewCamera.Editor)
        engine.SetEditorCameraView(0, 5, 15, 0, 0, 0)
        engine.TickFrame()   # RenderSystem이 메시를 한 번 불러 경계가 생긴다

        hit = engine.PickEntityAtScreen(0.0, 0.0)
        assert hit is not None and hit["entity"].id == cube.id, hit
        assert engine.PickEntityAtScreen(0.95, 0.95) is None   # 빈 하늘
        ray = engine.GetViewRay(0.0, 0.0)
        assert _close(ray[1], (0.0, -5 / math.sqrt(250), -15 / math.sqrt(250)), 1e-3), ray

        commands.clear()
        selected = []
        m = ViewportManipulator(engine, registry, api, qf, selected.append)
        m.press(0.0, 0.0, (160, 120))
        for i in range(1, 6):
            m.move(0.05 * i, 0.0, (160 + 8 * i, 120), False)
        m.release()
        assert selected == [cube.id]
        pos = json.loads(registry.GetComponentJson(cube, "TransformComponent"))["position"]
        assert pos[0] > 0.5 and abs(pos[1]) < 1e-4, pos   # 오른쪽으로, 높이 그대로
        commands.undo()
        pos = json.loads(registry.GetComponentJson(cube, "TransformComponent"))["position"]
        assert _close(pos, (0, 0, 0)), pos                  # 드래그 한 번 = Undo 한 번
        assert not commands.can_undo()

        # 기즈모: 하나를 고르면 생긴다. 카메라가 정면 위에서 보므로 X 화살표는 화면 가운데에서 오른쪽으로 뻗는다.
        assert engine.PickGizmo(0.1, 0.0) is None           # 고르기 전에는 없다
        engine.SetSelectedEntities([cube])
        engine.TickFrame()
        g = engine.PickGizmo(0.1, 0.0)
        assert g is not None and g["handle"] == 1 and g["entity"].id == cube.id, g
        assert engine.PickGizmo(0.9, 0.9) is None
        m.press(0.1, 0.0, (176, 120))
        for i in range(1, 6):
            m.move(0.1 + 0.04 * i, 0.03 * i, (176 + 6 * i, 120 - 4 * i), False)   # 위로도 움직이지만 X로만
        m.release()
        pos = json.loads(registry.GetComponentJson(cube, "TransformComponent"))["position"]
        assert pos[0] > 0.3 and abs(pos[1]) < 1e-4 and abs(pos[2]) < 1e-4, pos
        commands.undo()
        pos = json.loads(registry.GetComponentJson(cube, "TransformComponent"))["position"]
        assert _close(pos, (0, 0, 0)), pos
        def transform():
            return json.loads(registry.GetComponentJson(cube, "TransformComponent"))

        # 크기: 가운데 상자를 오른쪽·위로 150px 끌면 두 배, Undo 한 번에 원래대로
        m.set_mode(MODE_SCALE)
        engine.TickFrame()
        g = engine.PickGizmo(0.0, 0.0)
        assert g is not None and g["handle"] == HANDLE_UNIFORM and g["mode"] == MODE_SCALE, g
        m.press(0.0, 0.0, (160, 120))
        for i in range(1, 6):
            m.move(0.0, 0.0, (160 + 15 * i, 120 - 15 * i), False)
        m.release()
        assert _close(transform()["scale"], (2, 2, 2), 1e-3), transform()
        commands.undo()
        assert _close(transform()["scale"], (1, 1, 1)), transform()

        # 회전: 화면 아래쪽 세로선에서 고리를 찾아 잡고 옆으로 끈다. 값이 바뀌고 Undo 한 번에 원래대로.
        m.set_mode(MODE_ROTATE)
        engine.TickFrame()
        ring = next(((y, g) for y in [v / 100 for v in range(-2, -60, -1)]
                     for g in [engine.PickGizmo(0.0, y)] if g is not None), None)
        assert ring is not None, "회전 고리를 화면에서 찾지 못함"
        y, g = ring
        assert g["mode"] == MODE_ROTATE
        m.press(0.0, y, (160, 120))
        for i in range(1, 6):
            m.move(0.06 * i, y, (160 + 10 * i, 120), False)
        m.release()
        assert not _close(transform()["rotation"], (0, 0, 0), 1.0), transform()
        commands.undo()
        assert _close(transform()["rotation"], (0, 0, 0), 1e-3), transform()
        m.set_mode(MODE_MOVE)

        # Play 중에는 기즈모가 없다(Edit 상태에서만)
        world.Play()
        engine.TickFrame()
        assert engine.PickGizmo(0.1, 0.0) is None
        world.Stop()
    finally:
        qf.CommandManager.get_instance().clear()
        qf.EditorAPI.get_instance().set_registry(None)
        engine.Shutdown()
        host.close()


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
