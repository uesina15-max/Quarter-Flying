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

try:
    import ge_python
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False


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
        header.setStyleSheet("background-color: #0d1b2a; border-left: 3px solid #00d4ff; border-bottom: 1px solid #1e3a5f;")
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
    def __init__(self, comp_name: str, schema_jsonstr: str, entity_id: int, registry, parent=None):
        super().__init__(f"  {comp_name}", parent)
        self.comp_name = comp_name
        self.entity_id = entity_id
        self.registry = registry
        
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
                
            elif ftype == 3: # Vec3
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
                    v_layout.addWidget(sp)
                    widget.append(sp)
                h.addLayout(v_layout)
                
            elif ftype == 5: # EntityRef
                widget = QComboBox()
                # Populate Entity List
                widget.addItem("None", 0)
                if HAS_ENGINE and self.registry:
                    try:
                        entities = self.registry.GetAllEntities()
                        for e in entities:
                            fname = self.registry.GetEntityName(e.id)
                            widget.addItem(f"{fname} (ID: {e.id})", e.id)
                    except:
                        pass
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
            
        self._building = False
        if HAS_ENGINE and self.registry:
            self.load_data()

    def set_read_only(self, read_only: bool):
        for data in self._fields.values():
            w = data['widget']
            if isinstance(w, list):
                for sub in w: sub.setEnabled(not read_only)
            else:
                w.setEnabled(not read_only)

    def load_data(self):
        try:
            from ge_python import Entity
            json_str = self.registry.GetComponentJson(Entity(self.entity_id), self.comp_name)
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
                    elif ftype == 3:
                        w[0].setValue(val[0])
                        w[1].setValue(val[1])
                        w[2].setValue(val[2])
                    elif ftype in (5, 6):
                        idx = w.findData(val)
                        if idx >= 0: w.setCurrentIndex(idx)
            self._building = False
        except Exception as e:
            print(f"[Inspector] Load Data Failed: {e}")

    def _on_field_change(self, fname: str):
        if self._building or not self.registry: return
        
        fdata = self._fields.get(fname)
        if not fdata: return
        
        w = fdata['widget']
        ftype = fdata['type']
        
        val = None
        if ftype == 2: val = w.isChecked()
        elif ftype in (0, 1): val = w.value()
        elif ftype == 4: val = w.text()
        elif ftype == 3: val = [w[0].value(), w[1].value(), w[2].value()]
        elif ftype in (5, 6): val = w.currentData()
        
        try:
            from ge_python import Entity
            # Partial Single-Field Update calling with schema version protection!
            self.registry.SetComponentFieldJson(
                Entity(self.entity_id), 
                self.comp_name, 
                fname, 
                json.dumps(val), 
                self._schema_version
            )
        except Exception as e:
            print(f"[Inspector] Partial update failed for {self.comp_name}.{fname}: {e}")


class InspectorPanel(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.registry = None
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
                name = self.registry.GetEntityName(entity_id)
                self.lbl_entity_name.setText(name)
                
                from ge_python import Entity
                comps = self.registry.GetRegisteredComponents()
                
                # 등록된 컴포넌트 스키마 중 데이터가 {} 가 아닌 것들만 UI 생성
                for c in comps:
                    json_str = self.registry.GetComponentJson(Entity(entity_id), c)
                    if json_str and json_str != "{}":
                        schema = self.registry.GetComponentSchema(c)
                        w = DynamicComponentWidget(c, schema, entity_id, self.registry)
                        w.set_read_only(self._play_mode)
                        self.scroll_layout.insertWidget(self.scroll_layout.count() - 1, w)
            except Exception as e:
                print(f"Inspect Error: {e}")
