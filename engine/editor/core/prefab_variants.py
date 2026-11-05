import json
import os
import tempfile
from pathlib import Path


def nodes(document):
    if "entities" not in document:
        return {(): document["components"]}
    paths = []
    result = {}
    for index, entity in enumerate(document["entities"]):
        path = paths[entity["parent"]] + (entity["name"],) if index else ()
        if path in result:
            raise ValueError(f"형제 이름이 중복됩니다: {path}")
        paths.append(path)
        result[path] = entity["components"]
    return result


def differences(base, changed):
    patch = {key: None for key in base.keys() - changed.keys()}
    for key, value in changed.items():
        if key in base and base[key] == value:
            continue
        patch[key] = differences(base[key], value) if isinstance(value, dict) and isinstance(base.get(key), dict) else value
    return patch


def write_instance(binding, registry, entity, root, source, destination=None):
    root, source = Path(root).resolve(), Path(source).resolve()
    destination = Path(destination).resolve() if destination else source
    for path in (source, destination):
        path.relative_to(root)
    original = source.read_bytes()
    authored = json.loads(original.decode("utf-8-sig"))
    snapshot = json.loads(binding.SnapshotPrefab(registry, entity))
    resolved = json.loads(binding.ReadPrefab(source, str(root)))
    edited, existing = nodes(snapshot), nodes(resolved)
    if edited.keys() != existing.keys():
        raise ValueError("원본과 인스턴스의 계층이 다릅니다. 새 프리팹으로 저장하세요.")
    writes_variant = destination != source or "base" in authored
    # 증상: 아무것도 고치지 않은 인스턴스로 베리언트를 만들어도 샘플 프리팹마다 필드가 기록됐고(Fire는 8개), 그 필드는
    # 이후 원본을 고쳐도 상속되지 않았다. 에러 없음. 원인: 인스턴스 스냅샷을 파일 원문과 비교했다. 파일의 0.2는 float32를
    # 거치면 0.20000000298...이 되고, 파일에 없는 필드(기본값)는 스냅샷에만 있어서 전부 "다름"이 된다.
    # 베리언트는 양쪽 다 스폰->캡처를 거친 값끼리 비교한다.
    placed = nodes(json.loads(binding.SnapshotPrefabFile(source, str(root)))) if writes_variant else existing
    transform = edited[()].get("TransformComponent", {})
    if "position" in placed[()].get("TransformComponent", {}):
        transform["position"] = placed[()]["TransformComponent"]["position"]
    if writes_variant:
        base_path = source if destination != source else (source.parent / authored["base"]).resolve()
        inherited = nodes(json.loads(binding.SnapshotPrefabFile(base_path, str(root))))
        if inherited.keys() != edited.keys():
            raise ValueError("베이스 계층이 변경됐습니다.")
        updates = []
        for path in edited:
            patch = differences(inherited[path], edited[path])
            if patch:
                updates.append({"path": list(path), "components": patch})
        document = {"variantVersion": 1, "name": destination.name.removesuffix(".prefab.json") if destination != source else resolved["name"],
                    "base": Path(os.path.relpath(base_path, destination.parent)).as_posix(), "overrides": updates}
    else:
        for path in existing:
            existing[path].clear()
            existing[path].update(edited[path])
        document = resolved
    with tempfile.NamedTemporaryFile(dir=destination.parent, suffix=".prefab.json", delete=False) as stream:
        temporary = Path(stream.name)
    try:
        temporary.write_text(json.dumps(document, ensure_ascii=False, indent=2), encoding="utf8")
        binding.ReadPrefab(temporary, str(root))
        if source.read_bytes() != original:
            raise ValueError("편집하는 동안 원본이 변경됐습니다.")
        if destination != source:
            os.link(temporary, destination)
        else:
            os.replace(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
