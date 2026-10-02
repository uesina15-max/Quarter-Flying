"""
Inspector Panel
동적 컴포넌트 메타데이터(JSONSchema)를 이용해 런타임에 인스펙터를 렌더링.
"""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QScrollArea,
    QDoubleSpinBox, QSpinBox, QCheckBox, QGroupBox, QSizePolicy,
    QFrame, QPushButton, QComboBox, QLineEdit
)
from PySide6.QtCore import Qt, Signal, Slot, QTimer
import json
import os

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

        # §2.5: v1은 인스턴스 -> 원본 역전파(Apply)를 지원하지 않는다 - 버튼은 두되
        # 비활성화 + 안내 툴팁으로 "여기 있고, 아직 안 됨"을 명확히 한다(motion_editor.py의
        # do_import_fbx() 자리표시자와 같은 관례).
        btn_apply = QPushButton("Apply")
        btn_apply.setFixedHeight(22)
        btn_apply.setEnabled(False)
        btn_apply.setToolTip("다음 단계에서 지원 예정 (인스턴스 → 원본 역전파)")
        layout.addWidget(btn_apply)


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

# 0: Int, 1: Float, 2: Bool, 3: Vec3, 4: String, 5: EntityRef, 6: Enum
class DynamicComponentWidget(ComponentWidget):
    def __init__(self, comp_name: str, schema_jsonstr: str, entity_id: int, registry, editor_api=None, parent=None):
        super().__init__(f"  {comp_name}", parent)
        self.comp_name = comp_name
        self.entity_id = entity_id
        self.registry = registry
        self.editor_api = editor_api
        self._merge_active = False
        
        schema_data = json.loads(schema_jsonstr) if schema_jsonstr else {}
        self._schema_version = schema_data.get("version", 1)
        self._schema_fields = schema_data.get("fields", [])
        
        self._fields = {}
        self._building = True

        for field in self._schema_fields:
            row = QWidget()
            h = QHBoxLayout(row)
            h.setContentsMargins(0, 0, 0, 0)
            
            lbl = QLabel(field['displayName'])
            lbl.setFixedWidth(80)
            h.addWidget(lbl)
            
            ftype = field['type']
            widget = None
            
            # bind _on_change with the field name so we can do partial update
            # Python scoping workaround for lambda in loop
            def make_change_handler(fname):
                return lambda *args: self._on_field_change(fname)
            
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
                h.addWidget(widget)
                
            elif ftype == 1: # Float
                widget = QDoubleSpinBox()
                widget.setRange(-999999.0, 999999.0)
                widget.setDecimals(3)
                widget.setButtonSymbols(QDoubleSpinBox.NoButtons)
                widget.valueChanged.connect(make_change_handler(field['name']))
                h.addWidget(widget)
                
            elif ftype == 4: # String
                widget = QLineEdit()
                widget.editingFinished.connect(make_change_handler(field['name']))
                h.addWidget(widget)
                
            elif ftype in (3, 7): # Vec3 / EulerRotation(오일러 각 도 단위 3개 값 - Reflection.h FieldType)
                v_layout = QHBoxLayout()
                v_layout.setSpacing(2)
                widget = []
                for axis in ["X", "Y", "Z"]:
                    v_layout.addWidget(QLabel(axis))
                    sp = QDoubleSpinBox()
                    sp.setRange(-99999.0, 99999.0)
                    sp.setDecimals(2)
                    sp.setButtonSymbols(QDoubleSpinBox.NoButtons)
                    sp.valueChanged.connect(make_change_handler(field['name']))
                    if self._uses_editor_api(field['name']):
                        sp.installEventFilter(self)
                    v_layout.addWidget(sp)
                    widget.append(sp)
                h.addLayout(v_layout)
                
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

    def eventFilter(self, obj, event):
        if not self._uses_editor_api("position"):
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

    def set_read_only(self, read_only: bool):
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

            # Partial Single-Field Update calling with schema version protection!
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
    prefab_revert_status = Signal(str)  # 상태 바 표시용 (성공/실패 메시지 텍스트)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.registry = None
        self.editor_api = None
        self._current_entity_id = -1
        self._play_mode = False
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
        layout.addWidget(self.name_bar)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        self.scroll_content = QWidget()
        self.scroll_layout = QVBoxLayout(self.scroll_content)
        self.scroll_layout.setContentsMargins(0, 0, 0, 0)
        self.scroll_layout.addStretch()
        scroll.setWidget(self.scroll_content)
        layout.addWidget(scroll)

    def set_play_mode(self, is_playing: bool):
        self._play_mode = is_playing
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

    @Slot()
    def on_entity_deselected(self):
        self._current_entity_id = -1
        while self.scroll_layout.count() > 1:
            item = self.scroll_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self.lbl_entity_name.setText("No Selection")

    def _refresh_inspector(self, entity_id: int):
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

                # PrefabInstanceComponent가 있으면 일반 위젯 대신 전용 헤더를 맨 위에 얹는다
                # (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 4) - 아래 루프에서는
                # 이 컴포넌트를 건너뛴다(같은 데이터를 두 번 보여주지 않도록).
                prefab_json_str = self.registry.GetComponentJson(entity, _PREFAB_INSTANCE_COMPONENT_NAME)
                if prefab_json_str and prefab_json_str != "{}":
                    prefab_data = json.loads(prefab_json_str)
                    header = PrefabInstanceHeader(entity_id, prefab_data.get("prefabPath", ""))
                    header.revert_requested.connect(self._on_revert_requested)
                    self.scroll_layout.insertWidget(self.scroll_layout.count() - 1, header)

                # 등록된 컴포넌트 스키마 중 데이터가 {} 가 아닌 것들만 UI 생성
                for c in comps:
                    if c == _PREFAB_INSTANCE_COMPONENT_NAME:
                        continue
                    json_str = self.registry.GetComponentJson(entity, c)
                    if json_str and json_str != "{}":
                        schema = self.registry.GetComponentSchema(c)
                        w = DynamicComponentWidget(c, schema, entity_id, self.registry, self.editor_api)
                        w.set_read_only(self._play_mode)
                        self.scroll_layout.insertWidget(self.scroll_layout.count() - 1, w)
            except Exception as e:
                print(f"Inspect Error: {e}")

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
