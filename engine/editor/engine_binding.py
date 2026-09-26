"""
editor/engine_binding.py

에디터 전체에서 엔진 pybind11 확장 모듈(`quarterflying`)을 가져오는 유일한 지점.

배경: ROADMAP.md의 P1 Phase 6("이름 정합성 정리")에서 옛 이름 `ge_python`/`ge_engine`을
`Quarter Flying` 계열 이름으로 개명했다(engine/bindings/PythonModule.cpp의
`PYBIND11_MODULE(quarterflying, m)`, CMake 타겟명 `quarterflying`/`quarterflying_engine` 등).
이 래퍼가 그 전부터 이미 있었던 덕에, 개명 시점에 실제로 고쳐야 했던 import 지점은
바로 아래 이 파일 한 곳뿐이었다 - 에디터의 각 패널/모듈은 전부 이 모듈을 거치고,
호출부의 `ge_python.Xxx`(변수명일 뿐이라 그대로 유지) 표현은 전혀 손댈 필요가 없었다.

사용법: 다른 모든 에디터 코드는 `import quarterflying`을 직접 하지 말고 이 모듈을 거친다.

    from engine_binding import binding as ge_python, HAS_ENGINE
    ...
    if HAS_ENGINE:
        e = ge_python.Engine()

이렇게 하면 호출부의 `ge_python.Xxx` 표현은 기존 코드와 완전히 동일하게 유지되고
(별칭일 뿐이므로), 나중에 실제 확장 모듈 이름이 또 바뀌더라도 아래 try 블록의 import
문 한 줄만 고치면 된다.

엔티티 id(int)를 엔진 API에 넘길 때는 `ge_python.Entity(...)`로 직접 감싸지 말고
아래 `to_entity()`를 쓴다.

HAS_ENGINE이 False면 `binding`은 None이다 - 엔진 확장 모듈 없이도(더미/에디터 전용
모드) 에디터 UI 코드 자체는 정상적으로 임포트되는 기존 관례를 그대로 유지한다.
"""

try:
    import quarterflying as binding
    HAS_ENGINE = True
except ImportError:
    binding = None
    HAS_ENGINE = False


# EntityID는 C++ 쪽에서 uint32_t다(engine/ecs/Entity.h).
_ENTITY_ID_MAX = 0xFFFFFFFF


def to_entity(entity_id):
    """raw int 엔티티 id(또는 이미 만들어진 Entity)를 `binding.Entity`로 정규화한다.

    에디터에서 엔티티 id를 엔진 API에 넘기는 모든 경로는 이 함수를 거친다.

    이 함수가 생긴 이유: 패널들이 각자 `ge_python.Entity(entity_id)`로 직접 감싸던 시절,
    감싸기를 한 군데 빠뜨린 것만으로 버그가 두 번 났다. 둘 다 컴파일도 되고 크래시도
    안 나서 발견하기 어려웠다(CLAUDE.md "코드 품질 관례" 1번 사례):
      - Inspector가 raw int를 그대로 넘겨서 pybind11이 "incompatible function arguments"를
        던졌는데, 바깥 try가 이를 삼켜서 Inspector가 실제 엔티티에 대해 한 번도 그려진
        적이 없었다.
      - Scene Hierarchy가 Entity 객체를 Signal(int)로 emit해서 id가 조용히 0이 됐다.
    변환을 한 곳으로 모으고 잘못된 입력은 여기서 바로 원인이 적힌 에러로 막는다
    (CLAUDE.md 관례 3번). 그러면 엔진 API 안쪽에서 무엇이 틀렸는지 모르는 pybind11 에러로
    터지지 않는다.

    Raises:
        RuntimeError: 엔진 모듈 없이(HAS_ENGINE=False) 호출된 경우. 호출부가 HAS_ENGINE
            검사를 빠뜨렸다는 뜻이다. 예전처럼 None.Entity에서 나는 AttributeError보다
            원인이 분명하다.
        TypeError: int도 Entity도 아닌 값. bool도 여기서 거절한다. bool은 int의
            하위 클래스라 True가 조용히 엔티티 1이 되기 때문이다.
        ValueError: uint32 범위를 벗어난 id. 흔한 원인은 Inspector의 "선택 없음" 값인 -1이
            엔진 호출까지 새어 나온 경우다.
    """
    if not HAS_ENGINE:
        raise RuntimeError(
            "to_entity(): 엔진 모듈(quarterflying)이 로드되지 않았습니다 - "
            "호출부에서 HAS_ENGINE을 먼저 확인해야 합니다"
        )
    if isinstance(entity_id, binding.Entity):
        return entity_id
    if isinstance(entity_id, bool) or not isinstance(entity_id, int):
        raise TypeError(
            f"to_entity(): 엔티티 id는 int 또는 Entity여야 합니다 "
            f"(받은 값: {entity_id!r}, 타입: {type(entity_id).__name__})"
        )
    if not 0 <= entity_id <= _ENTITY_ID_MAX:
        raise ValueError(
            f"to_entity(): 엔티티 id {entity_id}가 uint32 범위를 벗어났습니다 "
            f"(-1이면 '선택 없음' 상태의 id가 엔진 호출까지 새어 나온 것)"
        )
    return binding.Entity(entity_id)
