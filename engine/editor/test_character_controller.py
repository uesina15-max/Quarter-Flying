"""
샘플 캐릭터 스크립트(engine/assets/scripts/character_controller.py)의 step() 유닛테스트.
docs/USABILITY_IMPROVEMENT_PLAN.md G1 — "점프 높이가 프레임률에 따라 달라지면 결함이다".

엔진 없이 돈다(step()은 순수 함수). 기존 engine/editor/test_*.py 관례를 따른다(pytest 미사용, 직접 실행).
    python test_character_controller.py
"""

import importlib.util
import math
import os
import sys

_SCRIPT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "assets", "scripts", "character_controller.py")
_spec = importlib.util.spec_from_file_location("character_controller", _SCRIPT)
cc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cc)


def _jump(fps: float):
    """바닥에서 점프해 다시 착지할 때까지의 (최고 높이, 체공 시간)."""
    dt = 1.0 / fps
    s = cc.State(0.0, 1.0, 0.0, ground_y=1.0)
    s = cc.step(s, cc.Inputs(jump_pressed=True), dt)
    peak, t = s.y, dt
    while s.y > s.ground_y and t < 5.0:
        s = cc.step(s, cc.Inputs(), dt)
        peak, t = max(peak, s.y), t + dt
    return peak - 1.0, t


def test_jump_height_and_airtime_do_not_depend_on_frame_rate():
    settings = cc.Settings()
    expected_peak = settings.jump_speed ** 2 / (2 * settings.gravity)
    expected_air = 2 * settings.jump_speed / settings.gravity
    for fps in (30, 60, 144, 240):
        peak, air = _jump(fps)
        assert abs(peak - expected_peak) / expected_peak < 0.02, f"{fps}fps peak {peak:.4f} vs {expected_peak:.4f}"
        assert abs(air - expected_air) <= 1.0 / fps + 1e-9, f"{fps}fps airtime {air:.4f} vs {expected_air:.4f}"


def test_horizontal_distance_does_not_depend_on_frame_rate():
    for fps in (30, 144):
        s = cc.State(0.0, 0.0, 0.0)
        for _ in range(int(fps)):        # 1초
            s = cc.step(s, cc.Inputs(move_y=1.0), 1.0 / fps)
        assert abs(-s.z - cc.Settings().move_speed) < 1e-6, f"{fps}fps moved {-s.z}"


def test_large_dt_is_clamped():
    s = cc.step(cc.State(0.0, 0.0, 0.0), cc.Inputs(move_y=1.0), 2.0)   # 창을 2초 끌었다 돌아옴
    assert abs(-s.z - cc.Settings().move_speed * cc.MAX_STEP_SECONDS) < 1e-6, s


def test_diagonal_is_not_faster():
    s = cc.step(cc.State(0.0, 0.0, 0.0), cc.Inputs(move_x=1.0, move_y=1.0), 0.1)
    assert abs(math.hypot(s.x, s.z) - cc.Settings().move_speed * 0.1) < 1e-6, s


def test_movement_follows_camera_forward():
    # 카메라가 +X를 보고 있으면 "앞"은 +X, "오른쪽"은 +Z
    s = cc.step(cc.State(0.0, 0.0, 0.0), cc.Inputs(move_y=1.0, forward_x=1.0, forward_z=0.0), 0.1)
    assert s.x > 0 and abs(s.z) < 1e-9, s
    s = cc.step(cc.State(0.0, 0.0, 0.0), cc.Inputs(move_x=1.0, forward_x=1.0, forward_z=0.0), 0.1)
    assert s.z > 0 and abs(s.x) < 1e-9, s


def test_no_double_jump_and_ground_clamp():
    s = cc.step(cc.State(0.0, 1.0, 0.0, ground_y=1.0), cc.Inputs(jump_pressed=True), 1 / 60)
    vy_after_jump = s.vy
    s = cc.step(s, cc.Inputs(jump_pressed=True), 1 / 60)     # 공중에서 다시 눌러도
    assert s.vy < vy_after_jump, "공중에서 다시 뛰면 안 된다"
    for _ in range(200):
        s = cc.step(s, cc.Inputs(), 1 / 60)
    assert s.y == 1.0 and s.vy == 0.0, s                      # 바닥 아래로 내려가지 않음


def test_sprint_multiplies_speed():
    s = cc.step(cc.State(0.0, 0.0, 0.0), cc.Inputs(move_y=1.0, sprint=True), 0.1)
    settings = cc.Settings()
    assert abs(-s.z - settings.move_speed * settings.sprint_multiplier * 0.1) < 1e-6, s


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
