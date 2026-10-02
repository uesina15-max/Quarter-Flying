"""
core/editor_camera.OrbitCamera 유닛테스트 (Qt, 엔진 불필요).

기존 engine/editor/test_*.py 관례를 따른다(pytest 미사용, 직접 실행).
    python test_editor_camera.py
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.editor_camera import OrbitCamera, look_rotation_quaternion, forward_from_quaternion, quaternion_from_euler_degrees


def _close(a, b, eps=1e-4):
    return all(abs(x - y) < eps for x, y in zip(a, b))


def _dist(a, b):
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def test_default_view_matches_engine_default_camera():
    eye, target = OrbitCamera().view()
    assert _close(eye, (0.0, 5.0, 15.0)), eye
    assert _close(target, (0.0, 0.0, 0.0)), target


def test_orbit_keeps_distance_and_moves_eye():
    cam = OrbitCamera()
    d0 = cam.distance
    eye0 = cam.eye()
    cam.orbit(300, 0)                       # 90도 수평 회전
    eye1 = cam.eye()
    assert abs(_dist(eye1, cam.target) - d0) < 1e-4
    assert not _close(eye0, eye1)
    assert abs(eye1[1] - eye0[1]) < 1e-4     # 수평 회전은 높이를 바꾸지 않는다


def test_pitch_is_clamped_below_vertical():
    cam = OrbitCamera()
    cam.orbit(0, 100000)
    assert cam.pitch < 90.0
    cam.orbit(0, -100000)
    assert cam.pitch > -90.0


def test_zoom_changes_distance_within_limits():
    cam = OrbitCamera()
    d0 = cam.distance
    cam.zoom(1)
    assert cam.distance < d0
    cam.zoom(-1000)
    assert cam.distance <= 2000.0
    cam.zoom(1000)
    assert cam.distance >= 0.5


def test_pan_moves_target_and_eye_together():
    cam = OrbitCamera()
    eye0, target0 = cam.view()
    cam.pan(100, 0, viewport_height_pixels=500)
    eye1, target1 = cam.view()
    moved = [target1[i] - target0[i] for i in range(3)]
    assert _dist(target0, target1) > 0.0
    assert _close([eye1[i] - eye0[i] for i in range(3)], moved)   # 시선 방향은 그대로
    # 기본 시점(+Z에서 바라봄)에서 오른쪽 드래그 = 타깃이 -X로(장면이 커서를 따라 오른쪽으로 보임)
    assert moved[0] < 0 and abs(moved[1]) < 1e-6


def test_reset_restores_default():
    cam = OrbitCamera()
    cam.orbit(123, 45)
    cam.zoom(3)
    cam.pan(10, 20, 400)
    cam.reset()
    assert _close(cam.eye(), (0.0, 5.0, 15.0))


def test_look_rotation_points_forward_at_target():
    for eye, target in [((0, 5, 15), (0, 0, 0)), ((20, 12, 0), (0, 0, 0)), ((-3, 1, -8), (4, 2, 6)), ((1, 0, 0), (1, 0, -5))]:
        q = look_rotation_quaternion(eye, target)
        assert abs(sum(v * v for v in q) - 1.0) < 1e-6          # 단위 쿼터니언
        f = forward_from_quaternion(q)
        d = [target[i] - eye[i] for i in range(3)]
        n = math.sqrt(sum(v * v for v in d))
        assert _close(f, [v / n for v in d], 1e-5), (eye, target, f)


def test_look_rotation_matches_engine_euler_convention():
    # 엔진에서 오일러 (-30, 90, 0)인 카메라(Cam B 검증)의 전방은 (-cos30, -sin30, 0)이다
    f_expected = (-math.cos(math.radians(30)), -0.5, 0.0)
    q = look_rotation_quaternion((0, 0, 0), f_expected)
    assert _close(forward_from_quaternion(q), f_expected, 1e-5)


def test_euler_quaternion_matches_look_rotation():
    # 엔진 오일러 (-30, 90, 0) = Cam B 검증에서 전방 (-cos30, -0.5, 0)
    f = forward_from_quaternion(quaternion_from_euler_degrees((-30.0, 90.0, 0.0)))
    assert _close(f, (-math.cos(math.radians(30)), -0.5, 0.0), 1e-5), f


def test_look_from_reproduces_eye_and_direction():
    cam = OrbitCamera()
    eye, fwd = (4.0, 7.0, -2.0), (0.3, -0.4, -0.866)
    cam.look_from(eye, fwd)
    e2, t2 = cam.view()
    assert _close(e2, eye, 1e-4), e2
    d = [t2[i] - e2[i] for i in range(3)]
    n = math.sqrt(sum(v * v for v in d)); m = math.sqrt(sum(v * v for v in fwd))
    assert _close([v / n for v in d], [v / m for v in fwd], 1e-4)


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
