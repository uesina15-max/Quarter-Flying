"""
editor/core/viewport_manipulation.py

Scene 뷰포트에서 클릭으로 고르고, 기즈모로 옮기고·돌리고·키우기(docs/VIEWPORT_MANIPULATION_PLAN.md). Qt를 모른다 -
viewport.py가 마우스 이벤트를 정규화 장치 좌표(-1..1, y 위가 +)로 바꿔 넘긴다.

경계: 피킹 규칙·광선·기즈모 손잡이 형상과 잡기 판정은 엔진(Engine.PickEntityAtScreen / GetViewRay / PickGizmo,
ecs/Picking.h, renderer/TransformGizmo.h), 부모 변환은 엔진(LocalPositionForWorld, GetWorldRotation)이 한다. 여기서는
끄는 동안의 새 값만 계산하고, 값은 Undo가 되는 EditorAPI(move_entity / rotate_entity / scale_entity)로만 쓴다.
"""

import json
import math
from typing import Callable, Optional, Tuple

Vec3 = Tuple[float, float, float]
Quat = Tuple[float, float, float, float]   # (x, y, z, w) - 엔진 Quaternion과 같은 순서

MODE_MOVE, MODE_ROTATE, MODE_SCALE = 0, 1, 2      # 엔진 GizmoMode
HANDLE_X, HANDLE_Y, HANDLE_Z, HANDLE_UNIFORM = 1, 2, 3, 4   # 엔진 GizmoHandle

# 평면과 거의 나란한 광선은 아주 먼 곳에서 만난다 - 그대로 쓰면 마우스를 조금만 움직여도 물체가 지평선 너머로
# 날아간다. 이보다 멀면 이번 이동은 건너뛴다.
MAX_DRAG_DISTANCE = 1000.0
MIN_SCALE_FACTOR = 0.01          # 크기를 0이나 음수로 끌어내리지 않는다(뒤집힘·사라짐 방지)
UNIFORM_SCALE_PIXELS = 150.0     # 가운데 상자: 오른쪽·위로 이만큼 끌면 두 배
ROTATE_DEGREES_PER_PIXEL = 0.5   # 고리가 화면과 거의 수직일 때(평면 교차가 불안정) 화면 이동량으로 돌린다


def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _scale(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _length(a): return math.sqrt(_dot(a, a))


def quat_multiply(a: Quat, b: Quat) -> Quat:
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def quat_conjugate(q: Quat) -> Quat:
    return (-q[0], -q[1], -q[2], q[3])


def quat_normalize(q: Quat) -> Quat:
    n = math.sqrt(sum(v * v for v in q))
    return tuple(v / n for v in q) if n > 1e-12 else (0.0, 0.0, 0.0, 1.0)


def quat_axis_angle(axis: Vec3, radians: float) -> Quat:
    s = math.sin(radians * 0.5)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(radians * 0.5))


def intersect_plane(origin: Vec3, direction: Vec3, point: Vec3, normal: Vec3) -> Optional[Vec3]:
    """광선과 평면(point를 지나고 normal에 수직)의 교점. 나란하거나, 뒤쪽이거나, 너무 멀면 None."""
    denom = _dot(direction, normal)
    if abs(denom) < 1e-6:
        return None
    t = _dot(_sub(point, origin), normal) / denom
    if t <= 0.0 or t > MAX_DRAG_DISTANCE:
        return None
    return _add(origin, _scale(direction, t))


def closest_axis_parameter(axis_origin: Vec3, axis_dir: Vec3, ray_origin: Vec3, ray_dir: Vec3) -> Optional[float]:
    """축 직선(axis_origin + s * axis_dir) 위에서 광선과 가장 가까운 점의 s. 두 방향은 단위 벡터.

    축이 시선과 거의 나란하면(화면에서 점처럼 보임) 끄는 방향이 정해지지 않으므로 None. 가장 가까운 곳이 카메라
    뒤여도 None.
    """
    w0 = _sub(axis_origin, ray_origin)
    b = _dot(axis_dir, ray_dir)
    denom = 1.0 - b * b
    if denom < 1e-4:
        return None
    d, e = _dot(axis_dir, w0), _dot(ray_dir, w0)
    t = (e - b * d) / denom      # 광선 쪽 매개변수
    if t <= 0.0:
        return None
    return (b * e - d) / denom


# ── 끌기 종류. value(광선 시작, 광선 방향, 픽셀)는 새 값 또는 None(이번 이동은 건너뜀)을 돌려준다. ──────────────

class DragPlane:
    """몸통을 잡고 끌기(이동 모드): 잡은 점(grab)이 커서를 따라가도록 엔티티의 월드 위치를 계산한다.

    vertical=False: 잡은 점을 지나는 수평면 - X, Z만 바뀐다.
    vertical=True : 잡은 점을 지나고 카메라 쪽을 보는 수직면 - Y만 바뀐다(Shift).
    """

    kind = MODE_MOVE

    def __init__(self, grab: Vec3, start_world: Vec3, vertical: bool, view_direction: Vec3):
        self.grab = tuple(grab)
        self.start_world = tuple(start_world)
        self.vertical = vertical
        if vertical:
            # 카메라 시선의 수평 성분을 법선으로. 바로 위에서 내려다보면 수직면이 화면과 나란해져 높이를 정할 수
            # 없다 - 그때는 Z축 평면으로 대신한다(움직임은 생기지만 정확히 커서를 따르지는 않는다).
            h = math.hypot(view_direction[0], view_direction[2])
            self.normal = (view_direction[0] / h, 0.0, view_direction[2] / h) if h > 1e-3 else (0.0, 0.0, 1.0)
        else:
            self.normal = (0.0, 1.0, 0.0)

    def world_position(self, origin: Vec3, direction: Vec3) -> Optional[Vec3]:
        hit = intersect_plane(origin, direction, self.grab, self.normal)
        if hit is None:
            return None
        delta = _sub(hit, self.grab)
        if self.vertical:
            return (self.start_world[0], self.start_world[1] + delta[1], self.start_world[2])
        return (self.start_world[0] + delta[0], self.start_world[1], self.start_world[2] + delta[2])

    def value(self, origin, direction, px):
        return self.world_position(origin, direction)

    def rebased(self, current_world: Vec3, vertical: bool, view_direction: Vec3) -> "DragPlane":
        """드래그 중 Shift를 바꿨을 때: 지금 위치에서 새 평면으로 이어 간다(물체가 튀지 않게)."""
        moved = _sub(current_world, self.start_world)
        return DragPlane(_add(self.grab, moved), current_world, vertical, view_direction)


class AxisDrag:
    """이동 화살표를 잡고 끌기: 그 축 방향으로만 움직인다. 누른 곳의 축 위 지점을 기준으로 커서를 따라간다."""

    kind = MODE_MOVE

    def __init__(self, direction: Vec3, start_world: Vec3, press_ray):
        self.direction = tuple(direction)
        self.start_world = tuple(start_world)
        self.s0 = closest_axis_parameter(self.start_world, self.direction, *press_ray)

    def value(self, origin, direction, px) -> Optional[Vec3]:
        if self.s0 is None:
            return None
        s = closest_axis_parameter(self.start_world, self.direction, origin, direction)
        if s is None or abs(s - self.s0) > MAX_DRAG_DISTANCE:
            return None
        return _add(self.start_world, _scale(self.direction, s - self.s0))


class RotateDrag:
    """회전 고리를 잡고 끌기: 그 월드 축을 중심으로 돌린다. 결과는 부모 기준 로컬 회전(엔진 TransformComponent.rotation).

    각도는 고리 평면 위에서 누른 점 -> 지금 점이 돈 양이다. 한 바퀴를 넘겨도 이어지도록 이동마다 더해 간다.
    고리가 화면과 거의 수직이면(평면과 시선이 나란) 평면 교차가 튀므로, 누를 때 그 상태였으면 가로 이동량으로 돌린다.
    """

    kind = MODE_ROTATE

    def __init__(self, axis: Vec3, origin: Vec3, start_world: Quat, parent_world: Quat, press_ray, press_px):
        self.axis, self.origin = tuple(axis), tuple(origin)
        self.start_world, self.parent_world = tuple(start_world), tuple(parent_world)
        self.press_px = press_px
        self.angle = 0.0
        self._last = self._plane_vector(*press_ray) if abs(_dot(press_ray[1], self.axis)) > 0.15 else None
        self.screen_fallback = self._last is None

    def _plane_vector(self, origin, direction) -> Optional[Vec3]:
        hit = intersect_plane(origin, direction, self.origin, self.axis)
        if hit is None:
            return None
        v = _sub(hit, self.origin)
        return v if _length(v) > 1e-6 else None

    def value(self, origin, direction, px) -> Optional[Quat]:
        if self.screen_fallback:
            self.angle = math.radians((px[0] - self.press_px[0]) * ROTATE_DEGREES_PER_PIXEL)
        else:
            v = self._plane_vector(origin, direction)
            if v is None:
                return None
            self.angle += math.atan2(_dot(self.axis, _cross(self._last, v)), _dot(self._last, v))
            self._last = v
        # 월드에서 돌리고(r * W) 부모 기준으로 되돌린다: local = inverse(P) * r * W
        r = quat_axis_angle(self.axis, self.angle)
        return quat_normalize(quat_multiply(quat_conjugate(self.parent_world), quat_multiply(r, self.start_world)))


class ScaleDrag:
    """크기 손잡이: 그 로컬 축 성분만 키운다(손잡이 길이만큼 끌면 두 배). 가운데 상자: 세 성분을 함께."""

    kind = MODE_SCALE

    def __init__(self, handle: int, axis: Vec3, origin: Vec3, length: float, start_scale: Vec3, press_ray, press_px):
        self.handle, self.axis, self.origin = handle, tuple(axis), tuple(origin)
        self.length = max(length, 1e-6)
        self.start_scale = tuple(start_scale)
        self.press_px = press_px
        self.s0 = None if handle == HANDLE_UNIFORM else closest_axis_parameter(self.origin, self.axis, *press_ray)

    def value(self, origin, direction, px) -> Optional[Vec3]:
        if self.handle == HANDLE_UNIFORM:
            factor = 1.0 + ((px[0] - self.press_px[0]) - (px[1] - self.press_px[1])) / UNIFORM_SCALE_PIXELS
            return tuple(v * max(factor, MIN_SCALE_FACTOR) for v in self.start_scale)
        if self.s0 is None:
            return None
        s = closest_axis_parameter(self.origin, self.axis, origin, direction)
        if s is None:
            return None
        factor = max(1.0 + (s - self.s0) / self.length, MIN_SCALE_FACTOR)
        scale = list(self.start_scale)
        scale[self.handle - 1] *= factor
        return tuple(scale)


class ViewportManipulator:
    """클릭 = 선택, 끌기 = 바꾸기. 드래그 한 번은 병합 세션 하나라 Undo 한 번에 되돌아간다.

    mode(이동·회전·크기)에 맞는 기즈모를 엔진이 고른 엔티티 하나에 그린다. 손잡이를 누르면 그 축으로, 이동 모드에서는
    오브젝트 몸통을 눌러도 수평면(Shift = 위아래)으로 옮긴다. 마우스를 올린 손잡이는 노랗게 강조한다(hover).

    engine      : Engine(PickGizmo, SetGizmoHighlight, SetGizmoMode, PickEntityAtScreen, GetViewRay)
    binding     : 엔진 모듈(GetWorldPosition, GetWorldRotation, GetParent, LocalPositionForWorld, Vec3, Quaternion,
                  CommandManager)
    on_select   : 엔티티 id(빈 곳이면 -1)를 받아 에디터 선택을 바꾼다
    on_moved    : 드래그가 끝난 엔티티 id(Inspector 갱신 등)
    on_error    : 사용자에게 보일 실패 메시지
    """

    DRAG_THRESHOLD_PX = 4.0   # 이보다 덜 움직이고 놓으면 클릭(선택만)

    def __init__(self, engine, registry, editor_api, binding,
                 on_select: Callable[[int], None], on_moved: Callable[[int], None] = lambda _id: None,
                 on_error: Callable[[str], None] = lambda _msg: None):
        self.engine, self.registry, self.editor_api, self.binding = engine, registry, editor_api, binding
        self.on_select, self.on_moved, self.on_error = on_select, on_moved, on_error
        self._mode = MODE_MOVE
        self._entity = None          # 누른 곳에서 잡은 엔티티(드래그 후보)
        self._gizmo = None           # 누른 기즈모 손잡이(엔진 PickGizmo 결과) - None이면 몸통을 잡음
        self._grab: Optional[Vec3] = None
        self._press_px: Optional[Tuple[float, float]] = None
        self._press_ndc: Optional[Tuple[float, float]] = None
        self._drag = None            # DragPlane / AxisDrag / RotateDrag / ScaleDrag
        self._current_world: Optional[Vec3] = None   # 몸통 드래그의 지금 위치(Shift 전환용)
        self._merging = False
        self._highlight = 0

    @property
    def mode(self) -> int:
        return self._mode

    def set_mode(self, mode: int) -> None:
        if mode not in (MODE_MOVE, MODE_ROTATE, MODE_SCALE):
            raise ValueError(f"알 수 없는 기즈모 모드: {mode}")
        self.release()
        self._mode = mode
        self.engine.SetGizmoMode(mode)

    @property
    def dragging(self) -> bool:
        return self._drag is not None

    def hover(self, ndc_x: float, ndc_y: float) -> None:
        """버튼을 누르지 않고 움직일 때: 커서 아래 기즈모 손잡이를 강조한다."""
        if self._entity is not None:
            return   # 누르고 있는 동안은 잡은 손잡이 강조를 유지한다
        hit = self.engine.PickGizmo(ndc_x, ndc_y)
        self._set_highlight(hit["handle"] if hit is not None else 0)

    def press(self, ndc_x: float, ndc_y: float, px: Tuple[float, float]) -> None:
        self.release()   # 놓기를 놓친 경우(창 밖에서 놓음 등) 이전 드래그를 정리한다
        self._press_px, self._press_ndc = (px[0], px[1]), (ndc_x, ndc_y)
        gizmo = self.engine.PickGizmo(ndc_x, ndc_y)
        if gizmo is not None:
            # 손잡이는 이미 고른 엔티티에 붙어 있다 - 선택은 그대로 두고 끌 준비만 한다.
            self._gizmo, self._entity = gizmo, gizmo["entity"]
            self._set_highlight(gizmo["handle"])
            return
        hit = self.engine.PickEntityAtScreen(ndc_x, ndc_y)
        if hit is None:
            self.on_select(-1)
            return
        entity = hit["entity"]
        self.on_select(entity.id)
        # 몸통을 끌어 옮기는 것은 이동 모드에서만. 회전·크기 모드에서 몸통 클릭은 선택만 한다.
        if self._mode == MODE_MOVE and self.registry.HasTransformComponent(entity):
            self._entity, self._grab = entity, tuple(hit["point"])

    def move(self, ndc_x: float, ndc_y: float, px: Tuple[float, float], vertical: bool) -> None:
        if self._entity is None:
            return
        ray = self.engine.GetViewRay(ndc_x, ndc_y)
        if ray is None:
            return
        origin, direction = ray
        if self._drag is None:
            if math.hypot(px[0] - self._press_px[0], px[1] - self._press_px[1]) < self.DRAG_THRESHOLD_PX:
                return
            if not self._begin_drag(vertical, direction):
                return
        elif isinstance(self._drag, DragPlane) and vertical != self._drag.vertical:
            self._drag = self._drag.rebased(self._current_world, vertical, direction)
        value = self._drag.value(origin, direction, px)
        if value is None:
            return
        try:
            self._apply(value)
        except Exception as e:
            # 명령이 거절되면(Play 잠금, 엔티티 삭제 등) 드래그를 끝낸다 - 같은 실패를 이동 이벤트마다 찍지 않는다.
            print(f"[Viewport] 변환 실패: {e}")
            self.on_error(f"변환 실패: {e}")
            self.release()
            return
        if isinstance(self._drag, DragPlane):
            self._current_world = value

    def release(self) -> None:
        changed = self._entity if self._drag is not None else None
        if self._merging:
            self.binding.CommandManager.get_instance().end_merge_session()
            self._merging = False
        if self._gizmo is not None:
            self._set_highlight(0)   # 다음 hover가 커서 아래 손잡이를 다시 강조한다
        self._entity = self._gizmo = self._grab = self._press_px = self._press_ndc = None
        self._drag = self._current_world = None
        if changed is not None:
            self.on_moved(changed.id)

    # ── 내부 ────────────────────────────────────────────────────────────────

    def _set_highlight(self, handle: int) -> None:
        if handle != self._highlight:
            self.engine.SetGizmoHighlight(handle)
            self._highlight = handle

    def _begin_drag(self, vertical: bool, view_direction: Vec3) -> bool:
        if not self.registry.HasTransformComponent(self._entity):   # 누른 뒤 지워졌다
            self.release()
            return False
        b, registry, entity = self.binding, self.registry, self._entity
        if self._gizmo is None:
            start = tuple(b.GetWorldPosition(registry, entity))
            self._drag = DragPlane(self._grab, start, vertical, view_direction)
            self._current_world = start
        else:
            press_ray = self.engine.GetViewRay(*self._press_ndc)
            if press_ray is None:
                return False
            g = self._gizmo
            if g["mode"] == MODE_MOVE:
                self._drag = AxisDrag(g["axis"], tuple(b.GetWorldPosition(registry, entity)), press_ray)
            elif g["mode"] == MODE_ROTATE:
                parent = b.GetParent(registry, entity)
                parent_world = tuple(b.GetWorldRotation(registry, parent)) if parent.IsValid() else (0.0, 0.0, 0.0, 1.0)
                self._drag = RotateDrag(g["axis"], g["origin"], tuple(b.GetWorldRotation(registry, entity)),
                                        parent_world, press_ray, self._press_px)
            else:
                start_scale = json.loads(registry.GetComponentJson(entity, "TransformComponent"))["scale"]
                self._drag = ScaleDrag(g["handle"], g["axis"], g["origin"], g["length"], tuple(start_scale),
                                       press_ray, self._press_px)
        # 이 드래그의 명령들을 하나로 합친다. 세션은 이번 드래그에서 쌓은 명령에만 합친다(CommandManager::Execute).
        b.CommandManager.get_instance().begin_merge_session()
        self._merging = True
        return True

    def _apply(self, value) -> None:
        b, api, entity = self.binding, self.editor_api, self._entity
        kind = self._drag.kind
        if kind == MODE_MOVE:
            api.move_entity(entity, b.Vec3(*b.LocalPositionForWorld(self.registry, entity, *value)))
        elif kind == MODE_ROTATE:
            api.rotate_entity(entity, b.Quaternion(*value))
        else:
            api.scale_entity(entity, b.Vec3(*value))
