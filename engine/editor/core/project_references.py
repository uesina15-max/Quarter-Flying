import json
import os
import tempfile
from dataclasses import dataclass
from pathlib import Path
from gamebuild.game_builder import COMPONENT_PATH_FIELDS


@dataclass(frozen=True)
class Reference:
    owner: Path
    keys: tuple
    value: str
    base: Path

    @property
    def target(self):
        return (self.base / self.value).resolve()


def references(project, owner, document):
    def ref(keys, value, base):
        if isinstance(value, str) and value and not value.startswith("builtin:"):
            yield Reference(owner, keys, value, base)

    def components(data, keys):
        for name, fields in COMPONENT_PATH_FIELDS.items():
            component = data.get(name)
            if isinstance(component, dict):
                for field, base in fields:
                    yield from ref(keys + (name, field), component.get(field), project.root if base == "project" else project.assets_dir)
        instance = data.get("PrefabInstanceComponent")
        if isinstance(instance, dict):
            yield from ref(keys + ("PrefabInstanceComponent", "prefabPath"), instance.get("prefabPath"), project.root)

    if owner.name.endswith(".material.json"):
        yield from ref(("texture",), document.get("texture"), project.root)
    elif owner.name.endswith(".action.json"):
        for index, event in enumerate(document.get("events", [])):
            if event.get("type") == "Sound":
                yield from ref(("events", index, "params", "clip"), event.get("params", {}).get("clip"), owner.parent)
    else:
        yield from ref(("base",), document.get("base"), owner.parent)
        yield from components(document.get("components", {}), ("components",))
        for group in ("entities", "overrides"):
            for index, entity in enumerate(document.get(group, [])):
                yield from components(entity.get("components", {}), (group, index, "components"))


def project_index(project):
    files = set()
    for directory in (project.scenes_dir, project.assets_dir):
        if directory.is_dir():
            files.update(p for p in directory.rglob("*.json") if p.name.endswith(
                (".scene.json", ".prefab.json", ".material.json", ".action.json")))
    rows, issues = [], []
    for owner in sorted(files):
        try:
            owner.resolve().relative_to(project.root.resolve())
            rows.extend(references(project, owner, json.loads(owner.read_text(encoding="utf-8-sig"))))
        except (ValueError, OSError, TypeError, AttributeError) as error:
            issues.append(f"{owner.name}: {error}")
    return rows, issues


def save_json(path, document, original):
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False, mode="w", encoding="utf8") as stream:
        temporary = Path(stream.name)
        try:
            json.dump(document, stream, ensure_ascii=False, indent=2, allow_nan=False)
            stream.write("\n")
            stream.close()
            if original is None:
                os.link(temporary, path)
            else:
                if path.read_bytes() != original:
                    raise ValueError("파일이 변경됐습니다. 다시 열어 주세요.")
                os.replace(temporary, path)
        finally:
            stream.close()
            temporary.unlink(missing_ok=True)


def replace_reference(project, row, target):
    target = target.resolve()
    for path in (row.owner.resolve(), target):
        path.relative_to(project.root.resolve())
    if not target.is_file():
        raise ValueError("대상 파일이 없습니다.")
    old_type = "".join(Path(row.value).suffixes[-2:]) if row.value.lower().endswith(".json") else Path(row.value).suffix
    if not target.name.lower().endswith(old_type.lower()):
        raise ValueError("같은 형식의 파일을 선택하세요.")
    original = row.owner.read_bytes()
    doc = json.loads(original.decode("utf-8-sig"))
    parent = doc
    for key in row.keys[:-1]:
        parent = parent[key]
    if parent[row.keys[-1]] != row.value:
        raise ValueError("참조가 변경됐습니다. 새로 고침하세요.")
    parent[row.keys[-1]] = Path(os.path.relpath(target, row.base)).as_posix()
    save_json(row.owner, doc, original)
