"""Local ECS commands for the single keyboard-controlled prototype object."""
import json
import math
from engine_binding import binding as engine
from core.follow_camera import component, resolve_target
from ge_transaction import transaction

COMPONENT='KeyboardMovementComponent'

def eligibility(registry, entity):
    if not entity.IsValid() or not registry.HasTransformComponent(entity):
        return '위치가 있는 객체를 선택하세요.'
    if registry.HasCameraComponent(entity):
        return '게임 객체를 선택하세요. 카메라는 조작 대상으로 사용할 수 없습니다.'
    if engine.GetParent(registry,entity).IsValid():
        return '부모에 연결된 객체는 기본 이동을 사용할 수 없습니다.'
    if component(registry,entity,'ScriptComponent'):
        return '스크립트가 연결된 객체입니다. 기존 동작과 겹치므로 기본 이동을 연결하지 않습니다.'
    return ''

def set_field(registry,api,entity,field,value):
    if component(registry,entity,COMPONENT).get(field)!=value:
        version=json.loads(registry.GetComponentSchema(COMPONENT))['version']
        api.set_component_field(entity,COMPONENT,field,json.dumps(value),version)

def connect_keyboard(registry,api,uuid):
    target=resolve_target(registry,uuid)
    reason=eligibility(registry,target)
    if reason: raise ValueError(reason)
    with transaction(api,'Select Keyboard Object'):
        for entity in registry.GetAllEntities():
            if entity.id!=target.id and component(registry,entity,COMPONENT).get('enabled',False):
                set_field(registry,api,entity,'enabled',False)
        if not component(registry,target,COMPONENT):
            api.add_component(target,COMPONENT)
        set_field(registry,api,target,'enabled',True)

def disconnect_keyboard(registry,api,uuid):
    entity=resolve_target(registry,uuid)
    if component(registry,entity,COMPONENT).get('enabled',False):
        with transaction(api,'Disable Keyboard Object'):
            set_field(registry,api,entity,'enabled',False)


def set_speed(registry,api,uuid,speed):
    speed=float(speed)
    if not math.isfinite(speed) or not 0 <= speed <= 1000:
        raise ValueError('이동 속도는 0~1000 사이의 숫자여야 합니다.')
    entity=resolve_target(registry,uuid)
    reason=eligibility(registry,entity)
    if reason: raise ValueError(reason)
    if not component(registry,entity,COMPONENT):
        raise ValueError('먼저 키보드 조작을 연결하세요.')
    with transaction(api,'Change Movement Speed'):
        set_field(registry,api,entity,'speed',speed)
