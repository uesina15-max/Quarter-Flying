"""
editor/core/editor_camera.py

Scene 뷰포트용 궤도(orbit) 카메라 컨트롤러. 계산만 하고 엔진이나 Qt는 모른다.
viewport.py가 마우스 입력을 이 객체에 넘기고, 결과(eye, target)를 Engine.SetEditorCameraView로 보낸다.

조작 규약(일반적인 DCC/엔진 에디터와 같음):
  - 우클릭 드래그 (또는 Alt + 좌클릭 드래그): 타깃을 중심으로 궤도 회전
  - 휠클릭 드래그 (또는 Shift + 우클릭 드래그): 화면 평면으로 이동(pan)
  - 휠: 줌(타깃까지 거리)
  - F: 원점 기준 기본 시점으로 리셋
"""

import math
from typing import Tuple

Vec3 = Tuple[float, float, float]

# 엔진 기본 카메라(Engine.cpp 6-3)와 같은 시작 시점: eye (0,5,15) -> target (0,0,0)
_DEFAULT_TARGET: Vec3 = (0.0, 0.0, 0.0)
_DEFAULT_EYE: Vec3 = (0.0, 5.0, 15.0)

_MIN_DISTANCE = 0.5
_MAX_DISTANCE = 2000.0
_PITCH_LIMIT = 89.0          # 정확히 +-90도면 up 벡터와 평행해져 lookAt이 무너진다


class OrbitCamera:
    ORBIT_DEG_PER_PIXEL = 0.3
    ZOOM_FACTOR_PER_STEP = 0.85   # 휠 한 칸(120 단위)당 거리 배율

    def __init__(self):
        self.reset()

    # ── 상태 ─────────────────────────────────────────────────────────────────
    def reset(self):
        self.target = list(_DEFAULT_TARGET)
        dx, dy, dz = (_DEFAULT_EYE[i] - _DEFAULT_TARGET[i] for i in range(3))
        self.distance = math.sqrt(dx * dx + dy * dy + dz * dz)
        self.yaw = math.degrees(math.atan2(dx, dz))                         # +Z에서 본 방위각
        self.pitch = math.degrees(math.asin(dy / self.distance))            # 위로 올려다본 각

    def eye(self) -> Vec3:
        yaw, pitch = math.radians(self.yaw), math.radians(self.pitch)
        offset = (math.cos(pitch) * math.sin(yaw),
                  math.sin(pitch),
                  math.cos(pitch) * math.cos(yaw))
        return tuple(self.target[i] + offset[i] * self.distance for i in range(3))

    def view(self) -> Tuple[Vec3, Vec3]:
        return self.eye(), tuple(self.target)

    # ── 조작 ─────────────────────────────────────────────────────────────────
    def orbit(self, dx_pixels: float, dy_pixels: float):
        self.yaw -= dx_pixels * self.ORBIT_DEG_PER_PIXEL
        self.pitch = max(-_PITCH_LIMIT, min(_PITCH_LIMIT, self.pitch + dy_pixels * self.ORBIT_DEG_PER_PIXEL))

    def pan(self, dx_pixels: float, dy_pixels: float, viewport_height_pixels: float):
        # 타깃 거리에서 화면 1픽셀이 월드에서 차지하는 길이(수직 FOV 60도, Engine.cpp와 같은 값)
        world_per_pixel = 2.0 * self.distance * math.tan(math.radians(30.0)) / max(viewport_height_pixels, 1.0)
        yaw, pitch = math.radians(self.yaw), math.radians(self.pitch)
        right = (math.cos(yaw), 0.0, -math.sin(yaw))
        up = (-math.sin(pitch) * math.sin(yaw), math.cos(pitch), -math.sin(pitch) * math.cos(yaw))
        for i in range(3):
            self.target[i] += (-dx_pixels * right[i] + dy_pixels * up[i]) * world_per_pixel

    def look_from(self, eye: Vec3, forward: Vec3):
        """eye 위치에서 forward 방향을 보는 시점으로 맞춘다(거리는 유지). "선택 카메라 시점으로 보기"용."""
        n = math.sqrt(sum(v * v for v in forward))
        if n < 1e-9:
            raise ValueError("look_from: forward가 0 벡터입니다")
        f = [v / n for v in forward]
        self.target = [eye[i] + f[i] * self.distance for i in range(3)]
        # eye = target + offset * distance 이고 offset = -forward
        ox, oy, oz = -f[0], -f[1], -f[2]
        self.yaw = math.degrees(math.atan2(ox, oz))
        self.pitch = max(-_PITCH_LIMIT, min(_PITCH_LIMIT, math.degrees(math.asin(max(-1.0, min(1.0, oy))))))

    def zoom(self, wheel_steps: float):
        """wheel_steps > 0 = 휠을 앞으로(가까이)."""
        self.distance = max(_MIN_DISTANCE, min(_MAX_DISTANCE, self.distance * (self.ZOOM_FACTOR_PER_STEP ** wheel_steps)))


# ── 시선 -> 카메라 회전 (Align to View) ──────────────────────────────────────
# 엔진 규약: TransformComponent.rotation은 쿼터니언이고, 카메라 전방은 rotation * (0, 0, -1)이다
# (CameraSystem.cpp). 오일러 각은 glm::quat(vec3(pitch, yaw, roll)) 규약이며, roll=0이면
# q = qYaw * qPitch이고 전방은 (-cos p * sin y, sin p, -cos p * cos y)이다.

def look_rotation_quaternion(eye: Vec3, target: Vec3) -> Tuple[float, float, float, float]:
    """eye에서 target을 보는 카메라의 회전 쿼터니언 (x, y, z, w). roll은 0이다."""
    d = [target[i] - eye[i] for i in range(3)]
    length = math.sqrt(sum(v * v for v in d))
    if length < 1e-9:
        raise ValueError("look_rotation_quaternion: eye와 target이 같은 점입니다")
    dx, dy, dz = (v / length for v in d)
    pitch = math.asin(max(-1.0, min(1.0, dy)))
    yaw = math.atan2(-dx, -dz)
    cp, sp = math.cos(pitch * 0.5), math.sin(pitch * 0.5)
    cy, sy = math.cos(yaw * 0.5), math.sin(yaw * 0.5)
    # qYaw(w=cy, y=sy) * qPitch(w=cp, x=sp)
    return (cy * sp, sy * cp, -sy * sp, cy * cp)


def forward_from_quaternion(q: Tuple[float, float, float, float]) -> Vec3:
    """쿼터니언 (x, y, z, w)으로 (0, 0, -1)을 회전한 방향 (엔진의 카메라 전방 계산과 같다)."""
    x, y, z, w = q
    vx, vy, vz = 0.0, 0.0, -1.0
    # v' = v + 2w(u x v) + 2 u x (u x v),  u = (x, y, z)
    cx, cy, cz = y * vz - z * vy, z * vx - x * vz, x * vy - y * vx
    ccx, ccy, ccz = y * cz - z * cy, z * cx - x * cz, x * cy - y * cx
    return (vx + 2 * (w * cx + ccx), vy + 2 * (w * cy + ccy), vz + 2 * (w * cz + ccz))


def quaternion_from_euler_degrees(euler: Vec3) -> Tuple[float, float, float, float]:
    """엔진의 QuaternionFromEulerDegrees(= glm::quat(radians(euler)))와 같은 공식. 반환 (x, y, z, w).

    TransformComponent.rotation은 직렬화될 때 오일러 각(도)이 된다(Reflection.h FieldType::EulerRotation).
    """
    hx, hy, hz = (math.radians(v) * 0.5 for v in euler)
    cx, cy, cz = math.cos(hx), math.cos(hy), math.cos(hz)
    sx, sy, sz = math.sin(hx), math.sin(hy), math.sin(hz)
    w = cx * cy * cz + sx * sy * sz
    x = sx * cy * cz - cx * sy * sz
    y = cx * sy * cz + sx * cy * sz
    z = cx * cy * sz - sx * sy * cz
    return (x, y, z, w)
