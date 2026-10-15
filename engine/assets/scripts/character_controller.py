"""
샘플 캐릭터 조작 스크립트 (docs/USABILITY_IMPROVEMENT_PLAN.md G1)

엔진 기능이 아니라 게임 개발자가 고쳐 쓰는 **샘플**이다. 이동 방식·속도·점프 높이는 전부 이 파일에 있고,
엔진은 기본 단위만 준다: 입력 액션(quarterflying.input)과 자기 엔티티의 Transform(quarterflying.script.registry()).

쓰는 액션(프로젝트의 assets/input/actions.json): MoveX, MoveY(축), Jump, Sprint(버튼).

구조: 계산은 엔진 없이 부를 수 있는 순수 함수 step()에 있다. CharacterController.on_update는 입력을 읽어 step()을
부르고 결과를 Transform에 쓰기만 한다. 그래서 캐릭터가 이상하게 움직일 때 "입력 값이 틀렸나(액션 맵) /
계산이 틀렸나(step) / 적용이 틀렸나(Transform)"를 경계의 값으로 가를 수 있다. step()은
editor/test_character_controller.py가 프레임률을 바꿔 가며 검사한다.

한계(샘플이라서): 충돌·물리가 없다. 바닥은 "시작했을 때의 높이"이고 그보다 아래로 내려가지 않는다.
"""

import math
from dataclasses import dataclass

# 한 프레임에 적분할 최대 시간(초). 창을 끌다 돌아오면 dt가 커지는데, 그만큼을 한 번에 적분하면 캐릭터가
# 순간이동한다(runtime_bootstrap.MAX_STEP_SECONDS, 에디터 main._on_fps_tick과 같은 값).
MAX_STEP_SECONDS = 0.1


@dataclass(frozen=True)
class Settings:
    move_speed: float = 4.0          # 초당 이동 거리
    sprint_multiplier: float = 1.8   # Sprint를 누르고 있을 때 배율
    jump_speed: float = 6.0          # 점프 순간의 위쪽 속도. 최고 높이 = jump_speed^2 / (2 * gravity)
    gravity: float = 18.0            # 아래쪽 가속도


@dataclass(frozen=True)
class State:
    x: float
    y: float
    z: float
    vy: float = 0.0                  # 위쪽 속도
    ground_y: float = 0.0            # 바닥 높이(시작 높이)

    @property
    def grounded(self) -> bool:
        return self.y <= self.ground_y + 1e-6 and self.vy <= 0.0


@dataclass(frozen=True)
class Inputs:
    move_x: float = 0.0              # 오른쪽 +, -1..1
    move_y: float = 0.0              # 앞쪽 +, -1..1
    jump_pressed: bool = False       # 이번 프레임에 눌렀나(누르고 있는 동안 계속 뛰지 않게)
    sprint: bool = False
    forward_x: float = 0.0           # 카메라 앞 방향(XZ 평면). 기본은 -Z
    forward_z: float = -1.0


def step(state: State, inputs: Inputs, dt: float, settings: Settings = Settings()) -> State:
    """state를 dt초 진행한 새 State. 프레임률과 무관하게 같은 움직임이 나와야 한다."""
    dt = max(0.0, min(float(dt), MAX_STEP_SECONDS))

    # 카메라 기준 이동: 앞 = 카메라가 보는 방향을 바닥에 눕힌 것, 오른쪽 = 앞을 시계 방향으로 90도.
    fx, fz = inputs.forward_x, inputs.forward_z
    length = math.hypot(fx, fz)
    fx, fz = (fx / length, fz / length) if length > 1e-6 else (0.0, -1.0)
    rx, rz = -fz, fx
    mx = inputs.move_x * rx + inputs.move_y * fx
    mz = inputs.move_x * rz + inputs.move_y * fz
    amount = math.hypot(mx, mz)
    if amount > 1.0:                 # 대각선이 더 빠르지 않게
        mx, mz = mx / amount, mz / amount
    speed = settings.move_speed * (settings.sprint_multiplier if inputs.sprint else 1.0)

    vy = state.vy
    if inputs.jump_pressed and state.grounded:
        vy = settings.jump_speed
    # 등가속도 운동을 정확히 적분한다(y += v*dt - g*dt^2/2). 계획서 초안의 "v -= g*dt; y += v*dt"(반암시적
    # 오일러)는 최고 높이가 dt에 비례해 틀어진다. 기본값으로 재 보면 정확한 최고 높이 1.0에 대해 30fps 0.900,
    # 144fps 0.979 - 프레임률마다 점프 높이가 8% 다르다(test_character_controller.py가 잡는다).
    y = state.y + vy * dt - 0.5 * settings.gravity * dt * dt
    vy -= settings.gravity * dt
    if y <= state.ground_y:
        y, vy = state.ground_y, 0.0

    return State(state.x + mx * speed * dt, y, state.z + mz * speed * dt, vy, state.ground_y)


def camera_forward_xz(registry) -> tuple:
    """활성 카메라가 보는 방향을 XZ 평면에 눕힌 것. 카메라가 없으면 -Z."""
    import quarterflying
    selection = quarterflying.SelectActiveCamera(registry)
    if not selection.entity.IsValid() or not registry.HasTransformComponent(selection.entity):
        return 0.0, -1.0
    q = registry.GetTransformComponent(selection.entity).rotation
    # 쿼터니언으로 (0, 0, -1)을 돌린 벡터의 x, z 성분(엔진 규칙: -Z가 전방).
    fx = -2.0 * (q.x * q.z + q.w * q.y)
    fz = -(1.0 - 2.0 * (q.x * q.x + q.y * q.y))
    return fx, fz


class CharacterController:
    """ScriptComponent(module_name="character_controller", class_name="CharacterController")."""

    settings = Settings()

    def __init__(self, entity):
        self.entity = entity
        self.state = None

    def on_create(self):
        import quarterflying
        registry = quarterflying.script.registry()
        p = registry.GetTransformComponent(self.entity).position
        self.state = State(p.x, p.y, p.z, 0.0, ground_y=p.y)

    def on_update(self, dt):
        import quarterflying
        if self.state is None:
            self.on_create()
        registry = quarterflying.script.registry()
        inp = quarterflying.input
        fx, fz = camera_forward_xz(registry)
        inputs = Inputs(move_x=inp.get_axis("MoveX"), move_y=inp.get_axis("MoveY"),
                        jump_pressed=inp.was_pressed("Jump"), sprint=inp.is_down("Sprint"),
                        forward_x=fx, forward_z=fz)
        self.state = step(self.state, inputs, dt, self.settings)
        registry.SetTransformPosition(self.entity, self.state.x, self.state.y, self.state.z)
