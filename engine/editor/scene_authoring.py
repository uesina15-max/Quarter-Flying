"""Local scene authoring helpers; no intermediate files or new save formats."""
from pathlib import Path

from ge_transaction import transaction

# Only RenderableComponent's project-relative fields use these selectors.
ASSET_FIELDS = {
    'meshPath': ('모델 선택', 'OBJ (*.obj)', ('.obj',), 'models'),
    'texturePath': ('텍스처 선택', 'Images (*.png *.jpg *.jpeg *.bmp *.tga *.psd *.gif *.hdr *.pic *.pnm)',
                    ('.png', '.jpg', '.jpeg', '.bmp', '.tga', '.psd', '.gif', '.hdr', '.pic', '.pnm'), 'textures'),
    'materialPath': ('머티리얼 선택', 'Materials (*.material.json)', ('.material.json',), 'materials'),
}

PRESET_COMPONENTS = {
    'Cube': 'RenderableComponent',
    'Camera': 'CameraComponent',
    'Directional Light': 'DirectionalLightComponent',
    'Particle': 'ParticleEffectComponent',
}


def project_asset_path(project, field, selected):
    """Validate a newly selected file only. Never rewrite existing field values."""
    path = Path(selected)
    if not path.is_absolute():
        path = project.root / path
    relative = project.relative(path)  # resolves symlinks and rejects outside paths
    if not path.is_file():
        raise ValueError('선택한 파일이 없거나 파일이 아닙니다.')
    if not path.name.lower().endswith(ASSET_FIELDS[field][2]):
        raise ValueError('이 항목에서 지원하지 않는 파일 형식입니다.')
    return relative


def available_entity_name(registry, base):
    names = {registry.GetEntityName(e) for e in registry.GetAllEntities()}
    if base not in names:
        return base
    suffix = 1
    while f'{base}_{suffix}' in names:
        suffix += 1
    return f'{base}_{suffix}'


def create_scene_preset(editor_api, registry, preset):
    """Use engine defaults and one undoable transaction for the entire entity."""
    component = PRESET_COMPONENTS[preset]
    name = available_entity_name(registry, preset)
    with transaction(editor_api, f'Create {preset}'):
        entity = editor_api.create_entity(name)
        editor_api.add_component(entity, 'TransformComponent')
        editor_api.add_component(entity, component)
    return entity
