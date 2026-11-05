"""Connect a camera using existing ECS fields and undoable local commands."""
import json
import math
from dataclasses import dataclass

from engine_binding import binding as engine
from ge_transaction import transaction
from scene_authoring import available_entity_name


def component(registry, entity, name):
    return json.loads(registry.GetComponentJson(entity, name) or '{}')


def resolve_target(registry, uuid):
    entity = registry.GetEntityByUUID(uuid)
    if not entity.IsValid() or not registry.HasTransformComponent(entity):
        raise ValueError('대상이 없거나 위치를 사용할 수 없습니다. 대상을 다시 선택하세요.')
    return entity


@dataclass(frozen=True)
class CameraChoice:
    uuid: int
    name: str
    reason: str
    connected: bool
    active: bool


def camera_choices(registry, target_uuid):
    target = resolve_target(registry, target_uuid)
    active = engine.SelectActiveCamera(registry).entity
    choices = []
    for entity in registry.GetAllEntities():
        if not registry.HasCameraComponent(entity):
            continue
        uuid = registry.GetEntityUUID(entity)
        reason = ''
        if entity.id == target.id:
            reason = '자기 자신은 따라갈 수 없습니다'
        elif not registry.HasTransformComponent(entity):
            reason = '위치가 없는 카메라입니다'
        elif engine.GetParent(registry, entity).IsValid():
            reason = '부모에 연결된 카메라입니다'
        elif component(registry, entity, 'CameraOrbitControlComponent'):
            reason = '마우스 회전 카메라로 사용 중입니다'
        elif component(registry, entity, 'ScriptComponent'):
            reason = '스크립트가 연결된 카메라입니다'
        follow = component(registry, entity, 'CameraFollowComponent')
        look = component(registry, entity, 'CameraLookAtComponent')
        connected = follow.get('target') == target_uuid and look.get('target') == target_uuid
        choices.append(CameraChoice(uuid, registry.GetEntityName(entity) or 'Camera',
                                    reason, connected, entity.id == active.id))
    return choices


def suggested_camera(choices):
    valid = [c for c in choices if not c.reason]
    linked = [c for c in valid if c.connected]
    active_linked = next((c for c in linked if c.active), None)
    if active_linked:
        return active_linked.uuid
    if len(linked) == 1:
        return linked[0].uuid
    if len(linked) > 1:
        return None
    active = next((c for c in valid if c.active), None)
    if active:
        return active.uuid
    return valid[0].uuid if len(valid) == 1 else (0 if not valid else None)


def camera_offset(registry, camera_uuid):
    if camera_uuid:
        camera = registry.GetEntityByUUID(camera_uuid)
        follow = component(registry, camera, 'CameraFollowComponent')
        if follow:
            return list(follow['offset'])
    return [0.0, 3.0, 8.0]


def resized_offset(original, distance, height):
    if not all(math.isfinite(v) for v in (*original, distance, height)) or distance < 0.1:
        raise ValueError('거리와 높이에 유효한 값을 입력하세요. 거리는 0.1 이상이어야 합니다.')
    horizontal = math.hypot(original[0], original[2])
    direction = (original[0] / horizontal, original[2] / horizontal) if horizontal else (0.0, 1.0)
    return [direction[0] * distance, height, direction[1] * distance]


def connect_follow_camera(registry, editor_api, target_uuid, camera_uuid, offset=None):
    """0 creates a camera. Existing rigs keep unedited offsets and damping."""
    resolve_target(registry, target_uuid)
    if camera_uuid is None:
        raise ValueError('사용할 카메라를 선택하세요.')
    if offset is not None and (len(offset) != 3 or not all(math.isfinite(v) for v in offset)):
        raise ValueError('카메라 위치에 유효한 값을 입력하세요.')
    if camera_uuid:
        choice = next((c for c in camera_choices(registry, target_uuid) if c.uuid == camera_uuid), None)
        if choice is None or choice.reason:
            raise ValueError(choice.reason if choice else '선택한 카메라가 없습니다. 다시 선택하세요.')
        camera = registry.GetEntityByUUID(camera_uuid)
    else:
        camera = None

    def set_field(entity, name, field, value):
        data = component(registry, entity, name)
        if data.get(field) != value:
            version = json.loads(registry.GetComponentSchema(name))['version']
            editor_api.set_component_field(entity, name, field, json.dumps(value), version)

    with transaction(editor_api, 'Connect Follow Camera'):
        if camera is None:
            camera = editor_api.create_entity(available_entity_name(registry, 'Follow Camera'))
            editor_api.add_component(camera, 'TransformComponent')
            editor_api.add_component(camera, 'CameraComponent')
        for name in ('CameraFollowComponent', 'CameraLookAtComponent'):
            if not component(registry, camera, name):
                editor_api.add_component(camera, name)
            set_field(camera, name, 'target', target_uuid)
        if offset is not None:
            set_field(camera, 'CameraFollowComponent', 'offset', offset)
        # Explicitly make this the game camera; priorities stay unchanged and Undo
        # restores all former main flags together with the new connection.
        for entity in registry.GetAllEntities():
            if registry.HasCameraComponent(entity):
                set_field(entity, 'CameraComponent', 'isMainCamera', entity.id == camera.id)
    return registry.GetEntityUUID(camera)
