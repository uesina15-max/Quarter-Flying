"""
Inspector Panel
동적 컴포넌트 메타데이터(JSONSchema)를 이용해 런타임에 인스펙터를 렌더링.
"""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QScrollArea,
    QDoubleSpinBox, QSpinBox, QCheckBox, QGroupBox, QSizePolicy,
    QFrame, QPushButton, QComboBox, QLineEdit, QFileDialog, QMessageBox, QGridLayout
)
from PySide6.QtCore import Qt, Signal, Slot, QTimer
import json
import os
from pathlib import Path
from core.prefab_variants import write_instance

from project import current_project, ProjectError
from scene_authoring import ASSET_FIELDS, project_asset_path
from panels.follow_camera import FollowCameraWidget
from panels.keyboard_movement import KeyboardMovementWidget

from style.theme import COLORS

from engine_binding import binding as ge_python, HAS_ENGINE, to_entity

# Inspector가 PrefabInstanceComponent에 대해 일반 DynamicComponentWidget 대신
# 전용 헤더를 그리기 위한 컴포넌트 이름 상수 (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 4).
_PREFAB_INSTANCE_COMPONENT_NAME = "PrefabInstanceComponent"


def _prefab_display_name(prefab_path: str) -> str:
    """"assets/prefabs/Barrel.prefab.json" -> "Barrel" (두 단계 확장자 제거)."""
    base = os.path.basename(prefab_path)
    stem, _ = os.path.splitext(base)   # "Barrel.prefab.json" -> "Barrel.prefab"
    stem, _ = os.path.splitext(stem)   # "Barrel.prefab" -> "Barrel"
    return stem or base


class PrefabInstanceHeader(QWidget):
    """선택한 엔티티가 프리팹 인스턴스일 때 표시하는 "Prefab: <name> [Revert] [Apply]" 헤더
    (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 4). 일반 컴포넌트 편집 위젯들 위에 얹힌다."""

    revert_requested = Signal(int)  # entity_id
    write_requested = Signal(int, str, bool)

    def __init__(self, entity_id: int, prefab_path: str, parent=None):
        super().__init__(parent)
        self.entity_id = entity_id
        self._build_ui(prefab_path)

    def _build_ui(self, prefab_path: str):
        self.setStyleSheet(
            f"background-color: {COLORS['bg_header']}; border: 1px solid {COLORS['border']}; border-radius: 3px;"
        )
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)

        lbl = QLabel(f"📦  Prefab: {_prefab_display_name(prefab_path)}")
        lbl.setToolTip(prefab_path)
        lbl.setStyleSheet(f"color: {COLORS['text_primary']}; font-weight: bold; border: none;")
        layout.addWidget(lbl)
        layout.addStretch()

        btn_revert = QPushButton("Revert")
        btn_revert.setFixedHeight(22)
        btn_revert.setToolTip("이 인스턴스를 프리팹 원본 상태로 되돌립니다")
        btn_revert.clicked.connect(lambda: self.revert_requested.emit(self.entity_id))
        layout.addWidget(btn_revert)

        btn_apply = QPushButton("Apply")
        btn_apply.setFixedHeight(22)
        btn_apply.clicked.connect(lambda: self.write_requested.emit(self.entity_id, prefab_path, False))
        layout.addWidget(btn_apply)
        variant = QPushButton("Variant…")
        variant.clicked.connect(lambda: self.write_requested.emit(self.entity_id, prefab_path, True))
        layout.addWidget(variant)


class ComponentWidget(QGroupBox):
    def __init__(self, title: str, parent=None):
        super().__init__(parent)
        self.setTitle("")
        self._title = title
        
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 4)
        outer.setSpacing(0)

        header = QWidget()
        header.setFixedHeight(26)
        header.setStyleSheet(f"background-color: {COLORS['bg_header']}; border-left: 3px solid {COLORS['accent']}; border-bottom: 1px solid {COLORS['border']};")
        h_layout = QHBoxLayout(header)
        h_layout.setContentsMargins(8, 0, 8, 0)

        title_label = QLabel(self._title)
        h_layout.addWidget(title_label)
        h_layout.addStretch()

        outer.addWidget(header)

        self.content = QWidget()
        self.content_layout = QVBoxLayout(self.content)
        self.content_layout.setContentsMargins(10, 4, 10, 8)
        self.content_layout.setSpacing(6)
        outer.addWidget(self.content)

# 축 이름과 색(뷰포트 기즈모와 같은 색 - engine/core/Engine.cpp UpdateTransformGizmo).
AXIS_COLORS = (("X", "#f25050"), ("Y", "#66e65a"), ("Z", "#5a8cff"))
# TransformComponent의 세 줄 = 뷰포트 기즈모 도구(0 이동, 1 회전, 2 크기 - core/viewport_manipulation.py MODE_*).
TRANSFORM_TOOLS = {"position": (0, "✥ 이동", "W"), "rotation": (1, "⟳ 회전", "E"), "scale": (2, "⤢ 크기", "R")}


# 0: Int, 1: Float, 2: Bool, 3: Vec3, 4: String, 5: EntityRef, 6: Enum
class DynamicComponentWidget(ComponentWidget):
    # TransformComponent에서 줄 이름(이동·회전·크기)을 눌렀다 - 그 기즈모 도구로 바꾼다(InspectorPanel이 받는다).
    tool_selected = Signal(int)

    def __init__(self, comp_name: str, schema_jsonstr: str, entity_id: int, registry, editor_api=None, parent=None):
        super().__init__(f"  {comp_name}", parent)
        self.comp_name = comp_name
        self.entity_id = entity_id
        self.registry = registry
        self.editor_api = editor_api
        self._merge_active = False
        self._read_only = False
        self._asset_buttons = {}
        
        schema_data = json.loads(schema_jsonstr) if schema_jsonstr else {}
        self._schema_version = schema_data.get("version", 1)
        self._schema_fields = schema_data.get("fields", [])
        
        self._fields = {}
        self._building = True

        # bind _on_change with the field name so we can do partial update
        # Python scoping workaround for lambda in loop
        def make_change_handler(fname):
            return lambda *args: self._on_field_change(fname)

        # TransformComponent: 위치·회전·크기를 표 하나로(축 이름은 맨 위 한 줄), 줄 이름은 기즈모 도구 버튼.
        # 예전에는 줄마다 X·Y·Z 글자가 반복됐고, 도구는 툴바에만 있어 값을 보는 곳과 고르는 곳이 떨어져 있었다.
        self.tool_buttons = {}
        self._grid = None
        if comp_name == "TransformComponent":
            grid_host = QWidget()
            self._grid = QGridLayout(grid_host)
            self._grid.setContentsMargins(0, 0, 0, 0)
            self._grid.setHorizontalSpacing(4)
            self._grid.setVerticalSpacing(6)
            for column, (axis, color) in enumerate(AXIS_COLORS, start=1):
                header = QLabel(axis)
                header.setAlignment(Qt.AlignCenter)
                header.setStyleSheet(f"color: {color}; font-weight: bold;")
                self._grid.addWidget(header, 0, column)
                self._grid.setColumnStretch(column, 1)
            self.content_layout.addWidget(grid_host)

        for field in self._schema_fields:
            if self._grid is not None and field['name'] in TRANSFORM_TOOLS and field['type'] in (3, 7):
                self._add_transform_row(field, make_change_handler(field['name']))
                continue
            row = QWidget()
            h = QHBoxLayout(row)
            h.setContentsMargins(0, 0, 0, 0)
            
            lbl = QLabel(field['displayName'])
            lbl.setFixedWidth(80)
            h.addWidget(lbl)
            
            ftype = field['type']
            widget = None

            if ftype == 2: # Bool
                widget = QCheckBox()
                widget.toggled.connect(make_change_handler(field['name']))
                h.addWidget(widget)
                h.addStretch()
                
            elif ftype == 0: # Int
                widget = QSpinBox()
                widget.setRange(-999999, 999999)
                widget.setButtonSymbols(QSpinBox.NoButtons)
                widget.valueChanged.connect(make_change_handler(field['name']))
                if self.editor_api is not None:
                    widget.installEventFilter(self)
                h.addWidget(widget)
                
            elif ftype == 1: # Float
                widget = QDoubleSpinBox()
                widget.setRange(-999999.0, 999999.0)
                widget.setDecimals(3)
                widget.setButtonSymbols(QDoubleSpinBox.NoButtons)
                widget.valueChanged.connect(make_change_handler(field['name']))
                if self.editor_api is not None:
                    widget.installEventFilter(self)
                h.addWidget(widget)
                
            elif ftype == 4: # String
                widget = QLineEdit()
                widget.editingFinished.connect(make_change_handler(field['name']))
                h.addWidget(widget)
                if comp_name == "RenderableComponent" and field['name'] in ASSET_FIELDS:
                    button = QPushButton("…")
                    button.setFixedWidth(28)
                    button.setToolTip("프로젝트 안의 파일 선택")
                    button.setEnabled(editor_api is not None)
                    button.clicked.connect(lambda _=False, name=field['name']: self._browse_asset(name))
                    self._asset_buttons[field['name']] = button
                    h.addWidget(button)
                
            elif ftype in (3, 7): # Vec3 / EulerRotation(오일러 각 도 단위 3개 값 - Reflection.h FieldType)
                v_layout = QHBoxLayout()
                v_layout.setSpacing(2)
                widget = []
                for axis, color in AXIS_COLORS:
                    axis_label = QLabel(axis)
                    axis_label.setFixedWidth(10)
                    axis_label.setStyleSheet(f"color: {color}; font-weight: bold;")
                    v_layout.addWidget(axis_label)
                    sp = self._make_axis_spinbox(make_change_handler(field['name']))
                    v_layout.addWidget(sp, 1)
                    widget.append(sp)
                h.addLayout(v_layout, 1)
                
            elif ftype == 5: # EntityRef
                # 콤보 항목의 데이터는 runtime id가 아니라 UUID(문자열)다. 엔진은 EntityRef를 UUID로
                # 직렬화하고(GetComponentJson), 필드 패치도 UUID로 해석한다(Reflection.h patchField의
                # GetEntityByUUID). 문자열인 이유: UUID는 uint64라 QVariant의 부호 있는 64비트 정수에
                # 담으면 큰 값이 깨질 수 있다.
                # 증상(예전): 목록에 "None"만 나왔고, 값을 표시하지도 바꾸지도 못했다. 원인은 세 가지였다.
                #   1) GetEntityName(e.id)에 raw int를 넘겨 pybind11이 "incompatible function arguments"를
                #      던졌고, 바로 아래 bare except가 그걸 삼켰다.
                #   2) 표시: 저장값(UUID)과 콤보 데이터(runtime id)가 달라 findData가 항상 실패했다.
                #   3) 변경: runtime id를 보내면 엔진이 그것을 UUID로 해석해 엉뚱한 엔티티(또는 무효)가 됐다.
                widget = QComboBox()
                widget.addItem("None", "0")
                if HAS_ENGINE and self.registry:
                    try:
                        for e in self.registry.GetAllEntities():
                            fname = self.registry.GetEntityName(e)
                            uuid = self.registry.GetEntityUUID(e)
                            widget.addItem(f"{fname} (ID: {e.id})", str(uuid))
                    except Exception as ex:
                        print(f"[Inspector] EntityRef 목록 구성 실패({field['name']}): {ex}")
                widget.currentIndexChanged.connect(make_change_handler(field['name']))
                h.addWidget(widget)
                
            elif ftype == 6: # Enum
                widget = QComboBox()
                options = field.get('options', [])
                for i, opt in enumerate(options):
                    widget.addItem(opt, i)
                widget.currentIndexChanged.connect(make_change_handler(field['name']))
                h.addWidget(widget)

            self._fields[field['name']] = {'widget': widget, 'type': ftype}
            self.content_layout.addWidget(row)

        # VFX Lite Phase 4 (docs/VFX_LITE_PLAN.md §5.3/§5.10) — 파티클 이펙트에만
        # "예상 파티클 수" 요약 줄을 덧붙인다. **잘못된 설정을 에디터 단계에서 막는 게
        # 가장 싼 최적화**이기 때문이다: Rate x Lifetime이 Max Particle을 넘으면 파티클이
        # 눈에 띄게 잘려나가고, 반대로 Max Particle만 크게 잡아두면 쓰지도 않을 메모리를
        # 미리 예약한다. 둘 다 실행해보기 전에는 알아채기 어렵다.
        self._estimate_label = None
        if comp_name == "ParticleEffectComponent":
            self._estimate_label = QLabel("")
            self._estimate_label.setWordWrap(True)
            self.content_layout.addWidget(self._estimate_label)

        self._building = False
        if HAS_ENGINE and self.registry:
            self.load_data()

    # 엔진의 ParticleSystem::kPreviewMaxParticle과 같은 값이어야 한다. 여기서 경고만
    # 띄우고 실제 상한은 엔진이 건다 - 에디터가 안 띄워도 Preview는 여전히 안전하다.
    PREVIEW_MAX_PARTICLE = 2048

    def _update_particle_estimate(self):
        """Spawn Rate / Lifetime / Burst / Max Particle로 예상 파티클 수를 계산해 보여준다."""
        if self._estimate_label is None:
            return
        try:
            rate     = self._fields['spawnRate']['widget'].value()
            lifetime = self._fields['lifetime']['widget'].value()
            burst    = self._fields['burst']['widget'].value()
            max_p    = self._fields['maxParticle']['widget'].value()
        except (KeyError, AttributeError):
            return

        # §5.3의 어림 계산. Burst는 한 번 터지고 수명대로 사라지므로 정상상태 **평균**에는
        # 기여하지 않는다 - 평균이 아니라 피크에만 더한다(외부 검토 보고서가 둘을 합쳐
        # "평균"이라고 부른 것은 docs/VFX_LITE_PLAN.md §7.4에서 정정됐다).
        average = rate * lifetime
        peak    = average + burst

        warnings = []
        if max_p <= 0:
            warnings.append("Max Particle이 0 이하라 파티클이 생성되지 않습니다")
        elif peak > max_p:
            warnings.append(
                f"Peak({peak:.0f})이 Max Particle({max_p})을 넘습니다 — 초과분은 생성되지 않고 잘립니다"
            )
        elif max_p > 16 and max_p > peak * 4:
            warnings.append(
                f"Max Particle({max_p})이 Peak({peak:.0f}) 대비 과도합니다 — 쓰지 않을 메모리를 예약합니다"
            )

        if max_p > self.PREVIEW_MAX_PARTICLE:
            warnings.append(
                f"Preview 상한 {self.PREVIEW_MAX_PARTICLE}개가 적용됩니다 (엔진이 강제)"
            )

        text = f"Expected  Average: {average:.0f}   Peak: {peak:.0f}   /   Max: {max_p}"
        if warnings:
            color = COLORS['text_warning']
            text += "\n⚠ " + "\n⚠ ".join(warnings)
        else:
            color = COLORS['text_dim']

        self._estimate_label.setText(text)
        self._estimate_label.setStyleSheet(f"color: {color}; font-size: 11px;")

    def _uses_editor_api(self, fname: str) -> bool:
        return (
            self.editor_api is not None
            and self.comp_name == "TransformComponent"
            and fname == "position"
        )

    def _make_axis_spinbox(self, on_change):
        sp = QDoubleSpinBox()
        sp.setRange(-99999.0, 99999.0)
        sp.setDecimals(2)
        sp.setButtonSymbols(QDoubleSpinBox.NoButtons)
        # 세 칸이 남는 폭을 나눠 쓰게 한다. 예전에는 칸의 최소 폭이 범위 문자열(-99999.00) 길이로 잡혀서
        # 이름(80px) + 세 칸이 Inspector 폭(약 300px)을 넘었고, Z 칸이 패널 오른쪽 밖으로 밀려 보이지 않았다.
        sp.setMinimumWidth(36)
        sp.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Fixed)
        sp.valueChanged.connect(on_change)
        if self.editor_api is not None:
            # 입력칸에 포커스가 있는 동안의 키 입력을 Undo 한 번으로 묶는다(병합 세션, eventFilter).
            sp.installEventFilter(self)
        return sp

    def _add_transform_row(self, field, on_change):
        mode, text, key = TRANSFORM_TOOLS[field['name']]
        row = self._grid.rowCount()
        button = QPushButton(text)
        button.setCheckable(True)
        button.setToolTip(f"{field['displayName']} - 뷰포트 기즈모를 이 도구로 (뷰포트에서 {key})")
        button.setStyleSheet(
            f"QPushButton {{ text-align: left; padding: 3px 8px; }}"
            f"QPushButton:checked {{ background-color: {COLORS['accent']}; color: white; }}")
        button.clicked.connect(lambda _=False, m=mode: self.tool_selected.emit(m))
        self.tool_buttons[mode] = button
        self._grid.addWidget(button, row, 0)
        spins = []
        for column in range(3):
            sp = self._make_axis_spinbox(on_change)
            self._grid.addWidget(sp, row, column + 1)
            spins.append(sp)
        self._fields[field['name']] = {'widget': spins, 'type': field['type']}

    def set_tool(self, mode: int):
        """지금 기즈모 도구의 줄 이름을 눌린 상태로(버튼을 눌러 꺼진 경우에도 다시 켠다)."""
        for m, button in self.tool_buttons.items():
            button.setChecked(m == mode)

    def eventFilter(self, obj, event):
        if self.editor_api is None:
            return super().eventFilter(obj, event)

        from PySide6.QtCore import QEvent
        if event.type() == QEvent.FocusIn:
            self._begin_merge_session()
        elif event.type() == QEvent.FocusOut:
            self._end_merge_session()
        return super().eventFilter(obj, event)

    def _begin_merge_session(self):
        if self._merge_active or not HAS_ENGINE:
            return
        try:
            from engine_binding import binding as ge_python
            ge_python.CommandManager.get_instance().begin_merge_session()
            self._merge_active = True
        except Exception as e:
            print(f"[Inspector] Merge session start failed: {e}")

    def _end_merge_session(self):
        if not self._merge_active or not HAS_ENGINE:
            return
        try:
            from engine_binding import binding as ge_python
            ge_python.CommandManager.get_instance().end_merge_session()
        except Exception as e:
            print(f"[Inspector] Merge session end failed: {e}")
        finally:
            self._merge_active = False

    def _browse_asset(self, fname: str):
        if self._read_only or self.editor_api is None or not self.registry:
            return
        project = current_project()
        title, file_filter, _, folder = ASSET_FIELDS[fname]
        start = project.assets_dir / folder
        current = self._fields[fname]['widget'].text()
        if current:
            candidate = Path(current)
            if not candidate.is_absolute():
                candidate = project.root / candidate
            try:
                project.relative(candidate)
                if candidate.is_file():
                    start = candidate
            except (ValueError, OSError, ProjectError):
                pass
        if not start.exists():
            start = project.root
        selected, _ = QFileDialog.getOpenFileName(self, title, str(start), file_filter)
        if not selected or self._read_only or current_project() is not project:
            return
        try:
            relative = project_asset_path(project, fname, selected)
            entity = to_entity(self.entity_id)
            values = json.loads(self.registry.GetComponentJson(entity, self.comp_name) or "{}")
            if values.get(fname) != relative:
                self.editor_api.set_component_field(
                    entity, self.comp_name, fname, json.dumps(relative), self._schema_version)
            # Update the text only after the engine accepts the command.
            self._fields[fname]['widget'].setText(relative)
        except Exception as e:
            QMessageBox.warning(self, title, str(e))

    def set_read_only(self, read_only: bool):
        self._read_only = read_only
        for button in self._asset_buttons.values():
            button.setEnabled(not read_only and self.editor_api is not None)
        for data in self._fields.values():
            w = data['widget']
            if isinstance(w, list):
                for sub in w: sub.setEnabled(not read_only)
            else:
                w.setEnabled(not read_only)

    def load_data(self):
        try:
            # 모듈 최상단에서 이미 import된 ge_python(엔진 바인딩 별칭)을 그대로 쓴다 -
            # 여기서 다시 "from ge_python import Entity"로 직접 import하면
            # engine_binding.py 래퍼를 우회하게 되어, 실제 확장 모듈 이름이 바뀔 때마다
            # (docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md Phase 6) 이런 지역 import들도
            # 전부 찾아 고쳐야 하는 문제가 생긴다.
            json_str = self.registry.GetComponentJson(to_entity(self.entity_id), self.comp_name)
            data = json.loads(json_str) if json_str != "{}" else {}
            
            self._building = True
            for fname, val in data.items():
                if fname in self._fields:
                    ftype = self._fields[fname]['type']
                    w = self._fields[fname]['widget']
                    
                    if ftype == 2:
                        w.setChecked(val)
                    elif ftype in (0, 1):
                        w.setValue(val)
                    elif ftype == 4:
                        w.setText(str(val))
                    elif ftype in (3, 7):
                        w[0].setValue(val[0])
                        w[1].setValue(val[1])
                        w[2].setValue(val[2])
                    elif ftype == 5:
                        idx = w.findData(str(int(val or 0)))   # EntityRef: UUID 문자열로 비교
                        if idx >= 0: w.setCurrentIndex(idx)
                    elif ftype == 6:
                        idx = w.findData(val)
                        if idx >= 0: w.setCurrentIndex(idx)
            self._building = False
            self._update_particle_estimate()
        except Exception as e:
            print(f"[Inspector] Load Data Failed: {e}")

    def _on_field_change(self, fname: str):
        if self._building or not self.registry: return

        # 값이 엔진에 반영되든 아니든 예측치는 항상 최신으로 유지한다 - 아래 부분 갱신이
        # 실패해도(예: 스키마 버전 불일치) 사용자가 지금 입력한 값 기준의 경고는 봐야 한다.
        self._update_particle_estimate()

        fdata = self._fields.get(fname)
        if not fdata: return
        
        w = fdata['widget']
        ftype = fdata['type']
        
        val = None
        if ftype == 2: val = w.isChecked()
        elif ftype in (0, 1): val = w.value()
        elif ftype == 4: val = w.text()
        elif ftype in (3, 7): val = [w[0].value(), w[1].value(), w[2].value()]
        elif ftype == 5: val = int(w.currentData() or "0")   # EntityRef: UUID(정수)로 보낸다
        elif ftype == 6: val = w.currentData()
        
        try:
            if self._uses_editor_api(fname):
                self.editor_api.move_entity(
                    to_entity(self.entity_id),
                    ge_python.Vec3(val[0], val[1], val[2]),
                )
                return

            if self.editor_api is not None and self.comp_name == "HierarchyComponent" and fname == "parent":
                # 필드를 직접 쓰면(SetComponentFieldJson) 순환 검사도 Undo도 없다 - 자기 자손을 부모로
                # 고르면 계층이 순환하고 엔진은 경고만 남긴 채 그 체인을 루트로 끊는다. 드래그와 같은
                # EditorAPI.set_parent 경로로 보내서 순환은 거절되고 Ctrl+Z가 되게 한다.
                parent = self.registry.GetEntityByUUID(val) if val else None
                self.editor_api.set_parent(to_entity(self.entity_id), parent)
                return

            if self.editor_api is not None:
                # 값이 그대로면 명령을 만들지 않는다. 문자열 칸의 editingFinished는 값을 안 바꾸고 포커스만 옮겨도
                # 오므로, 그대로 두면 아무것도 안 바꿨는데 "저장 안 된 변경"이 켜진다.
                try:
                    current = json.loads(self.registry.GetComponentJson(to_entity(self.entity_id), self.comp_name) or "{}")
                    if current.get(fname) == val:
                        return
                except (ValueError, TypeError):
                    pass   # 비교를 못 하면 그냥 쓴다(명령이 원인을 보고한다)
                # 증상(2026-10-16 점검, docs/EDITOR_BUGFIX_PLAN_2026-10-22.md F1): position·parent 외 필드
                # (rotation, scale, 색, 메시 경로 …)를 바꿔도 Undo가 안 됐고, 창 제목 '*'·종료 확인에도 잡히지 않아
                # 저장하지 않고 닫으면 경고 없이 사라졌다. 크래시도 로그도 없었다.
                # 원인: 이 필드들은 아래 SetComponentFieldJson으로 레지스트리에 직접 써서 CommandManager를 거치지
                # 않았다. "저장 안 된 변경"은 CommandManager 변경 카운터로만 판단한다(project_actions.py).
                # 이제 EditorAPI 명령(SetComponentFieldCommand)으로 보낸다 - 스키마 버전 보호는 명령이 한다.
                self.editor_api.set_component_field(
                    to_entity(self.entity_id), self.comp_name, fname, json.dumps(val), self._schema_version)
                return

            # 엔진 EditorAPI가 연결되지 않은 경우(단독 패널 테스트 등)만 레지스트리에 직접 쓴다 - Undo·변경 표시 없음.
            self.registry.SetComponentFieldJson(
                to_entity(self.entity_id),
                self.comp_name, 
                fname, 
                json.dumps(val), 
                self._schema_version
            )
        except Exception as e:
            print(f"[Inspector] Partial update failed for {self.comp_name}.{fname}: {e}")


class InspectorPanel(QWidget):
    follow_camera_play_requested = Signal()
    gizmo_tool_selected = Signal(int)   # Transform 줄 이름(이동·회전·크기)을 눌렀다 - main이 기즈모 도구를 바꾼다
    authoring_status = Signal(str)
    prefab_revert_status = Signal(str)  # 상태 바 표시용 (성공/실패 메시지 텍스트)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.registry = None
        self.editor_api = None
        self._current_entity_id = -1
        self._play_mode = False
        self.follow_camera = None
        self.keyboard_movement = None
        self.transform_widget = None   # 위쪽 고정 영역의 TransformComponent(스크롤에 묻히지 않게)
        self._gizmo_tool = 0
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        header = QLabel("  INSPECTOR")
        header.setFixedHeight(28)
        layout.addWidget(header)

        self.name_bar = QWidget()
        self.name_bar.setFixedHeight(36)
        h = QHBoxLayout(self.name_bar)
        h.setContentsMargins(10, 4, 10, 4)
        self.lbl_entity_name = QLabel("No Selection")
        h.addWidget(self.lbl_entity_name)
        h.addStretch()
        # 컴포넌트 추가(E-6). 목록은 엔진에 등록된 컴포넌트(GetRegisteredComponents)에서 만든다 - 여기에 이름을
        # 하드코딩하면 새 컴포넌트(예: DirectionalLightComponent)를 추가할 때마다 이 메뉴를 고쳐야 한다.
        self.btn_add_component = QPushButton("＋ Component")
        self.btn_add_component.setEnabled(False)
        self.btn_add_component.clicked.connect(self._show_add_component_menu)
        h.addWidget(self.btn_add_component)
        layout.addWidget(self.name_bar)

        # Keep the task controls inside the visible panel width. Generic Vec3
        # component rows can make the scroll contents wider than the Inspector.
        self.authoring_layout = QVBoxLayout()
        self.authoring_layout.setContentsMargins(0, 0, 0, 0)
        layout.addLayout(self.authoring_layout)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        self.scroll_content = QWidget()
        self.scroll_layout = QVBoxLayout(self.scroll_content)
        self.scroll_layout.setContentsMargins(0, 0, 0, 0)
        self.scroll_layout.addStretch()
        scroll.setWidget(self.scroll_content)
        layout.addWidget(scroll)

    # 메뉴에 내지 않는 컴포넌트: 다른 기능이 관리하는 메타데이터(프리팹 인스턴스 표시는 Instantiate/Create Prefab이 붙인다).
    # KeyboardMovementComponent는 "키보드로 움직이기" 위젯(panels/keyboard_movement.py)만 붙인다. 증상: 이 메뉴로
    # 붙이면 기본값 enabled=True라 기존 조작 대상과 함께 두 객체가 동시에 움직였고, 카메라에 붙이면 전용 위젯도
    # 일반 목록도 그리지 않아 Inspector 어디에도 보이지 않았다(지울 방법도 없음). 원인: 메뉴가 위젯의 대상 해제·
    # 자격 검사(core/keyboard_movement.eligibility)를 거치지 않았다.
    _NOT_USER_ADDABLE = {_PREFAB_INSTANCE_COMPONENT_NAME, 'KeyboardMovementComponent'}

    def addable_components(self, entity_id: int):
        """이 엔티티에 아직 없는, 사용자가 붙일 수 있는 등록 컴포넌트 이름(정렬)."""
        if not (self.registry and HAS_ENGINE) or entity_id < 0:
            return []
        entity = to_entity(entity_id)
        names = []
        for c in self.registry.GetRegisteredComponents():
            if c in self._NOT_USER_ADDABLE:
                continue
            existing = self.registry.GetComponentJson(entity, c)
            if not existing or existing == "{}":
                names.append(c)
        return sorted(names)

    def add_component(self, entity_id: int, comp_name: str):
        """EditorAPI 명령으로 붙인다(Undo 가능)."""
        if self.editor_api is None:
            raise RuntimeError("EditorAPI가 연결되지 않아 컴포넌트를 추가할 수 없습니다")
        self.editor_api.add_component(to_entity(entity_id), comp_name)
        self._refresh_inspector(entity_id)

    def _show_add_component_menu(self):
        from PySide6.QtWidgets import QMenu
        entity_id = self._current_entity_id
        menu = QMenu(self)
        names = self.addable_components(entity_id)
        if not names:
            menu.addAction("(추가할 수 있는 컴포넌트 없음)").setEnabled(False)
        for name in names:
            menu.addAction(name, lambda n=name: self._on_add_component(entity_id, n))
        menu.exec(self.btn_add_component.mapToGlobal(self.btn_add_component.rect().bottomLeft()))

    def _on_add_component(self, entity_id: int, comp_name: str):
        try:
            self.add_component(entity_id, comp_name)
        except Exception as e:
            print(f"[Inspector] 컴포넌트 추가 실패 ({comp_name}): {e}")
            self.prefab_revert_status.emit(f"컴포넌트 추가 실패: {e}")

    def _update_add_button(self):
        self.btn_add_component.setEnabled(
            bool(self.registry and self.editor_api and HAS_ENGINE) and self._current_entity_id >= 0
            and not self._play_mode)

    def set_gizmo_tool(self, mode: int):
        """지금 기즈모 도구(0 이동, 1 회전, 2 크기)를 Transform 줄 이름에 표시한다. 다시 그려도 유지된다."""
        self._gizmo_tool = mode
        if self.transform_widget is not None:
            self.transform_widget.set_tool(mode)

    def set_play_mode(self, is_playing: bool):
        self._play_mode = is_playing
        if self.transform_widget is not None:
            self.transform_widget.set_read_only(is_playing)
        if self.keyboard_movement is not None:
            self.keyboard_movement.set_read_only(is_playing)
        if self.follow_camera is not None:
            self.follow_camera.set_read_only(is_playing)
        self._update_add_button()
        # 위젯 Read-Only 반영
        for i in range(self.scroll_layout.count()):
            item = self.scroll_layout.itemAt(i)
            if item and item.widget() and isinstance(item.widget(), DynamicComponentWidget):
                item.widget().set_read_only(is_playing)

    def connect_registry(self, registry):
        self.registry = registry

    def connect_editor(self, editor_api):
        self.editor_api = editor_api

    @Slot(int)
    def on_entity_selected(self, entity_id: int):
        self._current_entity_id = entity_id
        self._refresh_inspector(entity_id)
        self._update_add_button()

    @Slot()
    def on_entity_deselected(self):
        self._clear_authoring_widgets()
        self._current_entity_id = -1
        self._update_add_button()
        while self.scroll_layout.count() > 1:
            item = self.scroll_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self.lbl_entity_name.setText("No Selection")

    def pending_authoring_message(self):
        if self.keyboard_movement is not None and self.keyboard_movement._speed_dirty:
            return '이동 속도를 먼저 적용한 뒤 Play로 시험하세요.'
        if self.follow_camera is not None and self.follow_camera._offset_edited:
            return '카메라 거리·높이를 먼저 적용한 뒤 Play로 시험하세요.'
        return ''

    def _clear_authoring_widgets(self):
        """위쪽 고정 영역(Transform, 키보드로 움직이기, 따라가기 카메라)을 비운다."""
        if self.transform_widget is not None:
            self.authoring_layout.removeWidget(self.transform_widget)
            self.transform_widget.hide()
            self.transform_widget.deleteLater()
            self.transform_widget = None
        if self.keyboard_movement is not None:
            self.authoring_layout.removeWidget(self.keyboard_movement)
            self.keyboard_movement.hide()
            self.keyboard_movement.deleteLater()
            self.keyboard_movement = None
        if self.follow_camera is not None:
            self.authoring_layout.removeWidget(self.follow_camera)
            self.follow_camera.hide()
            self.follow_camera.deleteLater()
            self.follow_camera = None

    def _refresh_inspector(self, entity_id: int):
        self._clear_authoring_widgets()
        while self.scroll_layout.count() > 1:
            item = self.scroll_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        if self.registry and HAS_ENGINE:
            try:
                entity = to_entity(entity_id)
                # 버그: entity_id(raw int)를 감싸지 않고 그대로 넘기면 pybind11이
                # "incompatible function arguments"로 예외를 던져서, 아래 프리팹 헤더를
                # 포함한 이 try 블록 전체가 항상 여기서 조용히 중단됐다(실제 엔진에
                # 연결된 상태에서 인스펙터가 한 번도 제대로 그려진 적이 없었다는 뜻 -
                # docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 4 실측 검증 중 발견).
                name = self.registry.GetEntityName(entity)
                self.lbl_entity_name.setText(name)

                comps = self.registry.GetRegisteredComponents()

                # Transform은 위쪽 고정 영역 맨 위에 둔다. 예전에는 아래 작업 위젯(키보드·카메라) 뒤 스크롤 영역에 있어
                # 위치·회전·크기가 패널 아래로 밀려 잘려 보였다. 줄 이름이 기즈모 도구 버튼이다(DynamicComponentWidget).
                if self.registry.HasTransformComponent(entity):
                    transform = DynamicComponentWidget(
                        "TransformComponent", self.registry.GetComponentSchema("TransformComponent"), entity_id,
                        self.registry, self.editor_api)
                    transform.set_read_only(self._play_mode)
                    transform.set_tool(self._gizmo_tool)
                    transform.tool_selected.connect(self.gizmo_tool_selected.emit)
                    self.authoring_layout.addWidget(transform)
                    self.transform_widget = transform

                if self.editor_api and self.registry.HasTransformComponent(entity) and not self.registry.HasCameraComponent(entity):
                    connected_registry = self.registry
                    movement = KeyboardMovementWidget(
                        self.registry, self.editor_api, self.registry.GetEntityUUID(entity),
                        lambda: (not self._play_mode and self.registry is connected_registry
                                 and self._current_entity_id == entity_id))
                    movement.status.connect(self.authoring_status.emit)
                    movement.play_requested.connect(self.follow_camera_play_requested.emit)
                    movement.set_read_only(self._play_mode)
                    self.authoring_layout.addWidget(movement)
                    self.keyboard_movement = movement
                    follow = FollowCameraWidget(
                        self.registry, self.editor_api, self.registry.GetEntityUUID(entity),
                        lambda: (not self._play_mode and self.registry is connected_registry
                                 and self._current_entity_id == entity_id))
                    follow.status.connect(self.authoring_status.emit)
                    follow.status.connect(lambda _message: movement.reload())
                    follow.play_requested.connect(self.follow_camera_play_requested.emit)
                    follow.set_read_only(self._play_mode)
                    self.authoring_layout.addWidget(follow)
                    self.follow_camera = follow

                # PrefabInstanceComponent가 있으면 일반 위젯 대신 전용 헤더를 맨 위에 얹는다
                # (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 4) - 아래 루프에서는
                # 이 컴포넌트를 건너뛴다(같은 데이터를 두 번 보여주지 않도록).
                prefab_json_str = self.registry.GetComponentJson(entity, _PREFAB_INSTANCE_COMPONENT_NAME)
                if prefab_json_str and prefab_json_str != "{}":
                    prefab_data = json.loads(prefab_json_str)
                    header = PrefabInstanceHeader(entity_id, prefab_data.get("prefabPath", ""))
                    header.revert_requested.connect(self._on_revert_requested)
                    header.write_requested.connect(self._write_prefab)
                    header.setEnabled(not self._play_mode)
                    self.scroll_layout.insertWidget(self.scroll_layout.count() - 1, header)

                # 등록된 컴포넌트 스키마 중 데이터가 {} 가 아닌 것들만 UI 생성
                for c in comps:
                    if c == _PREFAB_INSTANCE_COMPONENT_NAME:
                        continue
                    if c == "TransformComponent" and self.transform_widget is not None:
                        continue   # 위쪽 고정 영역에 이미 있다
                    # 전용 위젯이 있을 때만 일반 위젯을 생략한다. 카메라처럼 전용 위젯을 그리지 않는 엔티티에 옛 씬
                    # 파일 등으로 붙어 있으면 일반 위젯으로라도 보여야 한다(실행 시스템은 카메라를 움직이지 않는다).
                    if c == 'KeyboardMovementComponent' and self.keyboard_movement is not None:
                        continue
                    json_str = self.registry.GetComponentJson(entity, c)
                    if json_str and json_str != "{}":
                        schema = self.registry.GetComponentSchema(c)
                        w = DynamicComponentWidget(c, schema, entity_id, self.registry, self.editor_api)
                        w.set_read_only(self._play_mode)
                        self.scroll_layout.insertWidget(self.scroll_layout.count() - 1, w)
            except Exception as e:
                print(f"Inspect Error: {e}")

    def _write_prefab(self, entity_id, source_path, variant):
        if self._play_mode or not source_path:
            return
        try:
            root = current_project().root
            source = (root / source_path).resolve()
            destination = None
            if variant:
                path, _ = QFileDialog.getSaveFileName(self, "새 베리언트", str(source.parent / "Variant.prefab.json"), "Prefab (*.prefab.json)")
                if not path:
                    return
                destination = Path(path).resolve()
                if destination.exists() or not destination.name.endswith(".prefab.json"):
                    raise ValueError("새 .prefab.json 파일명을 선택하세요.")
            elif QMessageBox.question(self, "Apply", f"{source.name}에 선택한 인스턴스의 변경을 저장할까요?") != QMessageBox.StandardButton.Yes:
                return
            write_instance(ge_python, self.registry, to_entity(entity_id), root, source, destination)
            self.prefab_revert_status.emit(f"프리팹 저장: {destination or source}")
        except Exception as error:
            # Exception 전체를 받는다. 엔진 오류(quarterflying.EngineError)는 RuntimeError의 자식이 아니라서, 예전처럼
            # (OSError, RuntimeError, ValueError)만 받으면 EntityRef가 있는 인스턴스의 Apply("has an EntityRef field")가
            # 대화상자 없이 콘솔 트레이스백으로만 끝나 버튼이 고장 난 것처럼 보였다.
            QMessageBox.warning(self, "프리팹 저장 실패", str(error))

    def _on_revert_requested(self, entity_id: int):
        """Revert 버튼 클릭 -> EditorAPI::RevertPrefabInstance 호출 후 인스펙터 갱신
        (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 4)."""
        if not self.editor_api:
            return
        try:
            self.editor_api.revert_prefab_instance(to_entity(entity_id))
            self.prefab_revert_status.emit("프리팹 되돌리기 완료")
        except Exception as e:
            print(f"[Inspector] 프리팹 되돌리기 실패: {e}")
            self.prefab_revert_status.emit(f"프리팹 되돌리기 실패: {e}")
        finally:
            # 성공/실패 여부와 무관하게 최신 값으로 다시 그린다 - 성공했으면 되돌려진
            # 값을, 실패했으면(예: 원본 파일 삭제) §2.9에 따라 바뀌지 않은 기존 값을 보여준다.
            if entity_id == self._current_entity_id:
                self._refresh_inspector(entity_id)
