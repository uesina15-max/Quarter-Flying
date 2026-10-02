"""
editor/scene_instantiation.py

scene.json의 objects[]를 실제 ECS 엔티티(Transform + Renderable.meshPath)로 만든다.
(docs/REVIEW_BASED_IMPROVEMENT_PLAN.md P0-2 "메시 에셋 브리지")

이 모듈이 생기기 전: DemoSceneIntegration은 scene.json을 파싱만 했고, 결과는 Scene Hierarchy
트리에 더미 항목으로만 들어갔다. 엔진이 연결되면 500ms 갱신(refresh_from_engine)이 트리를 ECS
기준으로 다시 그리면서 그 항목들이 사라졌다. 즉 scene.json의 모델은 한 번도 그려진 적이 없다.

역할 분리:
  - expand_scene_objects(): 순수 데이터 변환(grid/instances 전개). 엔진과 Qt가 필요 없다.
  - instantiate_scene():    엔진 호출. 엔티티마다 독립 실행하고, 실패는 "[SceneLoad]" 로그로 남긴다
                            (demo_scene_seed.py와 같은 방식).
메시 파일 로드 자체는 C++ RenderSystem이 meshPath를 처음 볼 때 한다(엔진 쪽 GL 컨텍스트에서).
로드 실패는 엔진 로그 "RenderSystem - failed to load mesh"로 나타난다.
"""

import json
from typing import Callable, List, NamedTuple, Sequence


class SceneEntitySpec(NamedTuple):
    name: str
    mesh_path: str                  # 에셋 루트(engine/) 기준, 예: "assets/models/cube.obj"
    position: Sequence[float]
    rotation: Sequence[float]       # 오일러 각(도), scene.json 형식 그대로
    scale: Sequence[float]
    texture_path: str = ''          # 에셋 루트 기준, 비어 있으면 텍스처 없음(scene.json의 "texture")


class InstantiateResult(NamedTuple):
    name: str
    ok: bool
    detail: str


def _vec3(value, default):
    if value is None:
        return list(default)
    if not isinstance(value, (list, tuple)) or len(value) != 3:
        raise ValueError(f"3-element vector expected, got {value!r}")
    return [float(v) for v in value]


def expand_scene_objects(scene_objects) -> List[SceneEntitySpec]:
    """DemoSceneIntegration.get_scene_objects() 결과를 엔티티 단위 명세로 펼친다.

    - grid:      {"count":[nx,ny,nz], "spacing":[sx,sy,sz], "origin":[ox,oy,oz]} -> nx*ny*nz개
    - instances: [{"position","rotation","scale"}, ...] -> 항목마다 1개
    - 둘 다 없으면 transform(position/rotation/scale) 기준 1개
    model이 비어 있는 오브젝트는 그릴 수 없으므로 ValueError로 거절한다(조용히 큐브로 그리지 않음).
    "texture"(선택)는 전개된 모든 엔티티에 같이 붙는다.
    """
    specs: List[SceneEntitySpec] = []
    for obj in scene_objects:
        name = obj.get('name') or 'unnamed'
        model = obj.get('model') or ''
        if not model:
            raise ValueError(f"scene object '{name}' has no 'model' path")
        texture = obj.get('texture') or ''

        grid = obj.get('grid')
        instances = obj.get('instances') or []
        if grid:
            count = [int(c) for c in grid.get('count', [1, 1, 1])]
            if len(count) != 3 or any(c < 1 for c in count):
                raise ValueError(f"scene object '{name}': grid.count must be 3 positive ints, got {count}")
            spacing = _vec3(grid.get('spacing'), (0.0, 0.0, 0.0))
            origin = _vec3(grid.get('origin'), (0.0, 0.0, 0.0))
            for ix in range(count[0]):
                for iy in range(count[1]):
                    for iz in range(count[2]):
                        pos = [origin[0] + ix * spacing[0],
                               origin[1] + iy * spacing[1],
                               origin[2] + iz * spacing[2]]
                        specs.append(SceneEntitySpec(f"{name}[{ix},{iy},{iz}]", model,
                                                     pos, [0.0, 0.0, 0.0], [1.0, 1.0, 1.0], texture))
        elif instances:
            for i, inst in enumerate(instances):
                specs.append(SceneEntitySpec(
                    f"{name}[{i}]" if len(instances) > 1 else name, model,
                    _vec3(inst.get('position'), (0.0, 0.0, 0.0)),
                    _vec3(inst.get('rotation'), (0.0, 0.0, 0.0)),
                    _vec3(inst.get('scale'), (1.0, 1.0, 1.0)), texture))
        else:
            t = obj.get('transform') or {}
            specs.append(SceneEntitySpec(
                name, model,
                _vec3(t.get('position'), (0.0, 0.0, 0.0)),
                _vec3(t.get('rotation'), (0.0, 0.0, 0.0)),
                _vec3(t.get('scale'), (1.0, 1.0, 1.0)), texture))
    return specs


def instantiate_scene(editor_api, registry, specs: Sequence[SceneEntitySpec],
                      log: Callable[[str], None]) -> List[InstantiateResult]:
    """명세마다 엔티티 하나(Transform + Renderable{meshPath})를 만든다. 하나가 실패해도 계속한다."""
    if editor_api is None or registry is None:
        raise ValueError(
            "instantiate_scene(): editor_api와 registry가 모두 필요합니다 "
            f"(editor_api={editor_api!r}, registry={registry!r})"
        )

    results: List[InstantiateResult] = []
    for spec in specs:
        try:
            entity = editor_api.create_entity(spec.name)
            editor_api.add_component(entity, "TransformComponent")
            registry.SetTransformPosition(entity, *spec.position)
            registry.SetTransformScale(entity, *spec.scale)
            if any(abs(v) > 1e-6 for v in spec.rotation):
                registry.SetTransformRotation(entity, *spec.rotation)
            editor_api.add_component(entity, "RenderableComponent")
            renderable = {"meshPath": spec.mesh_path}
            if spec.texture_path:
                renderable["texturePath"] = spec.texture_path
            registry.SetComponentJson(entity, "RenderableComponent", json.dumps(renderable))
            results.append(InstantiateResult(spec.name, True, ""))
        except Exception as e:
            detail = f"{type(e).__name__}: {e}"
            results.append(InstantiateResult(spec.name, False, detail))
            log(f"[SceneLoad] {spec.name} 생성 실패 - {detail}")

    ok = sum(1 for r in results if r.ok)
    log(f"[SceneLoad] scene.json 엔티티 {ok}/{len(results)}개 생성")
    return results
