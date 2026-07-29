"""
Scene Hierarchy Panel
씬 계층 뷰 — 엔티티 트리를 표시하고 선택/생성/삭제를 지원
"""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QTreeWidget,
    QTreeWidgetItem, QPushButton, QMenu, QAbstractItemView,
    QSizePolicy
)
from PySide6.QtCore import Qt, Signal, QTimer
from PySide6.QtGui import QIcon, QColor, QBrush, QFont, QAction

# ge_python 바인딩이 없는 경우에도 더미 모드로 동작
try:
    import ge_python
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False


class EntityItem(QTreeWidgetItem):
    """씬 트리의 엔티티 항목"""

    def __init__(self, entity_id: int, name: str, parent=None):
        if parent:
            super().__init__(parent)
        else:
            super().__init__()
        self.entity_id = entity_id
        self.entity_name = name
        self.setText(0, name)
        self._apply_style()

    def _apply_style(self):
        font = QFont("Segoe UI", 11)
        self.setFont(0, font)


class SceneHierarchyPanel(QWidget):
    """
    씬 계층 뷰 패널

    Signals:
        entity_selected(int): 엔티티 ID를 포함한 선택 시그널
        entity_deselected(): 선택 해제 시그널
    """

    entity_selected = Signal(int)    # entity_id
    entity_deselected = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.registry = None          # ge_python ECSRegistry 참조 (2단계 연동)
        self._selected_entity_id = -1
        self._dummy_counter = 0       # 더미 엔티티 카운터

        self._build_ui()
        self._connect_signals()

        # 1단계: 더미 씬 데이터로 초기화
        self._populate_dummy_scene()

        # 2단계: 엔진 연동 시 주기적 갱신 타이머
        self._refresh_timer = QTimer(self)
        self._refresh_timer.timeout.connect(self._on_refresh_tick)
        # 연동이 활성화되면 start() 호출

    # ----------------------------------------------------------------
    # UI 구성
    # ----------------------------------------------------------------

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # 패널 헤더
        header = QLabel("  SCENE HIERARCHY")
        header.setObjectName("panel_header")
        header.setFixedHeight(28)
        layout.addWidget(header)

        # 툴바 (엔티티 추가/삭제)
        toolbar = self._build_toolbar()
        layout.addWidget(toolbar)

        # 트리 위젯
        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.setAnimated(True)
        self.tree.setExpandsOnDoubleClick(True)
        self.tree.setSelectionMode(QAbstractItemView.SingleSelection)
        self.tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self.tree.setDragDropMode(QAbstractItemView.NoDragDrop)
        self.tree.setAlternatingRowColors(True)
        layout.addWidget(self.tree)

    def _build_toolbar(self) -> QWidget:
        bar = QWidget()
        bar.setFixedHeight(32)
        bar.setStyleSheet("background-color: #0d1b2a; border-bottom: 1px solid #1e3a5f;")
        h = QHBoxLayout(bar)
        h.setContentsMargins(6, 2, 6, 2)
        h.setSpacing(4)

        self.btn_create = QPushButton("＋ Entity")
        self.btn_create.setObjectName("btn_create")
        self.btn_create.setFixedHeight(22)
        self.btn_create.setToolTip("새 엔티티 생성")

        self.btn_delete = QPushButton("✕")
        self.btn_delete.setObjectName("btn_delete")
        self.btn_delete.setFixedHeight(22)
        self.btn_delete.setFixedWidth(28)
        self.btn_delete.setToolTip("선택한 엔티티 삭제")

        h.addWidget(self.btn_create)
        h.addStretch()
        h.addWidget(self.btn_delete)

        return bar

    # ----------------------------------------------------------------
    # 시그널 연결
    # ----------------------------------------------------------------

    def _connect_signals(self):
        self.tree.currentItemChanged.connect(self._on_item_changed)
        self.tree.customContextMenuRequested.connect(self._on_context_menu)
        self.btn_create.clicked.connect(self.create_entity)
        self.btn_delete.clicked.connect(self.delete_selected_entity)

    # ----------------------------------------------------------------
    # 더미 씬 데이터 (1단계)
    # ----------------------------------------------------------------

    def _populate_dummy_scene(self):
        """초기 더미 씬 데이터 삽입"""
        self.tree.clear()

        root = EntityItem(1, "🌍  World Root")
        self.tree.addTopLevelItem(root)

        # Camera
        cam = EntityItem(2, "🎥  Main Camera")
        root.addChild(cam)

        # Lights
        dir_light = EntityItem(3, "💡  Directional Light")
        root.addChild(dir_light)

        # Static Meshes group
        meshes = EntityItem(4, "📦  Static Meshes")
        root.addChild(meshes)
        meshes.addChild(EntityItem(5, "    Floor"))
        meshes.addChild(EntityItem(6, "    WallNorth"))
        meshes.addChild(EntityItem(7, "    WallSouth"))

        # Player
        player = EntityItem(8, "🧍  Player")
        root.addChild(player)
        player.addChild(EntityItem(9, "    PlayerModel"))
        player.addChild(EntityItem(10, "    Collider"))

        root.setExpanded(True)
        meshes.setExpanded(True)

        self._dummy_counter = 11

    # ----------------------------------------------------------------
    # 2단계: 엔진 ECS 연동
    # ----------------------------------------------------------------

    def connect_registry(self, registry):
        """
        ge_python ECSRegistry 연결 (2단계).
        연결 후 주기적 갱신을 시작합니다.
        """
        self.registry = registry
        self._refresh_timer.start(500)  # 500ms마다 씬 갱신
        self.refresh_from_engine()

    def refresh_from_engine(self):
        """
        엔진 ECS에서 실제 엔티티 목록을 읽어와 트리를 갱신합니다.
        (ge_python.ECSRegistry.GetAllEntities() 필요)
        """
        if not self.registry or not HAS_ENGINE:
            return
        try:
            entities = self.registry.GetAllEntities()
            self.tree.clear()
            for eid in entities:
                name = self.registry.GetEntityName(eid) if hasattr(self.registry, 'GetEntityName') else f"Entity_{eid}"
                self.tree.addTopLevelItem(EntityItem(eid, name))
        except Exception as e:
            print(f"[SceneHierarchy] 엔진 동기화 실패: {e}")

    def _on_refresh_tick(self):
        """타이머 콜백 — 엔진과 씬 동기화"""
        self.refresh_from_engine()

    # ----------------------------------------------------------------
    # 데모 씬 통합 (Phase 2)
    # ----------------------------------------------------------------

    def load_from_scene_data(self, scene_data):
        """
        scene.json 데이터로부터 씬 계층 구성
        """
        self.tree.clear()
        
        # 루트 아이템
        root = EntityItem(0, "🌍  Scene Root")
        self.tree.addTopLevelItem(root)
        
        # 오브젝트 로드
        if 'objects' in scene_data:
            self._load_objects_from_data(scene_data['objects'], root)
        
        # 라이트 로드
        if 'lights' in scene_data:
            self._load_lights_from_data(scene_data['lights'], root)
        
        root.setExpanded(True)
        print(f"[SceneHierarchy] Loaded scene with {root.childCount()} items")

    def _load_objects_from_data(self, objects_data, parent_item):
        """오브젝트 데이터로부터 트리 아이템 생성"""
        entity_id = 1
        
        for obj_data in objects_data:
            name = obj_data.get('name', 'unnamed')
            model = obj_data.get('model', '')
            
            # 아이템 생성
            item = EntityItem(entity_id, f"📦  {name}")
            parent_item.addChild(item)
            entity_id += 1
            
            # 인스턴스가 있는 경우
            if 'instances' in obj_data:
                for i, instance in enumerate(obj_data['instances']):
                    inst_name = f"{name}_Instance_{i}"
                    inst_item = EntityItem(entity_id, f"    🔲  {inst_name}")
                    item.addChild(inst_item)
                    entity_id += 1
            
            # 그리드가 있는 경우
            if 'grid' in obj_data:
                grid_data = obj_data['grid']
                count = grid_data.get('count', [1, 1, 1])
                grid_name = f"{name}_Grid_{count[0]}x{count[1]}x{count[2]}"
                grid_item = EntityItem(entity_id, f"    📊  {grid_name}")
                item.addChild(grid_item)
                entity_id += 1

    def _load_lights_from_data(self, lights_data, parent_item):
        """라이트 데이터로부터 트리 아이템 생성"""
        entity_id = 100
        
        for i, light_data in enumerate(lights_data):
            light_name = f"💡  Light_{i}"
            item = EntityItem(entity_id, light_name)
            parent_item.addChild(item)
            entity_id += 1

    # ----------------------------------------------------------------
    # 엔티티 생성/삭제
    # ----------------------------------------------------------------

    def create_entity(self):
        """새 엔티티 생성 (엔진 연동 시 ECS에도 반영)"""
        entity_id = self._dummy_counter
        self._dummy_counter += 1

        if self.registry and HAS_ENGINE:
            try:
                entity = self.registry.CreateEntity()
                entity_id = entity.id
                name = f"Entity_{entity_id}"
            except Exception as e:
                print(f"[SceneHierarchy] 엔티티 생성 실패: {e}")
                name = f"Entity_{entity_id}"
        else:
            name = f"Entity_{entity_id}"

        item = EntityItem(entity_id, f"🔲  {name}")
        self.tree.addTopLevelItem(item)
        self.tree.setCurrentItem(item)

    def delete_selected_entity(self):
        """선택한 엔티티 삭제"""
        item = self.tree.currentItem()
        if not item or not isinstance(item, EntityItem):
            return

        entity_id = item.entity_id

        if self.registry and HAS_ENGINE:
            try:
                self.registry.DestroyEntity(ge_python.Entity(entity_id))
            except Exception as e:
                print(f"[SceneHierarchy] 엔티티 삭제 실패: {e}")

        # 트리에서 제거
        parent = item.parent()
        if parent:
            parent.removeChild(item)
        else:
            idx = self.tree.indexOfTopLevelItem(item)
            self.tree.takeTopLevelItem(idx)

        self.entity_deselected.emit()
        self._selected_entity_id = -1

    # ----------------------------------------------------------------
    # 이벤트 핸들러
    # ----------------------------------------------------------------

    def _on_item_changed(self, current, previous):
        if current and isinstance(current, EntityItem):
            self._selected_entity_id = current.entity_id
            self.entity_selected.emit(current.entity_id)
        else:
            self._selected_entity_id = -1
            self.entity_deselected.emit()

    def _on_context_menu(self, pos):
        item = self.tree.itemAt(pos)
        menu = QMenu(self)

        action_create = QAction("➕  Create Entity", self)
        action_create.triggered.connect(self.create_entity)
        menu.addAction(action_create)

        if item and isinstance(item, EntityItem):
            menu.addSeparator()
            action_dup = QAction("📋  Duplicate", self)
            action_dup.triggered.connect(lambda: self._duplicate_entity(item))
            menu.addAction(action_dup)

            action_del = QAction("🗑️  Delete", self)
            action_del.triggered.connect(self.delete_selected_entity)
            menu.addAction(action_del)

        menu.exec(self.tree.viewport().mapToGlobal(pos))

    def _duplicate_entity(self, item: EntityItem):
        """엔티티 복제"""
        new_id = self._dummy_counter
        self._dummy_counter += 1
        new_name = f"{item.entity_name}_copy"
        new_item = EntityItem(new_id, new_name)

        parent = item.parent()
        if parent:
            parent.addChild(new_item)
        else:
            self.tree.addTopLevelItem(new_item)

        self.tree.setCurrentItem(new_item)

    # ----------------------------------------------------------------
    # Public API
    # ----------------------------------------------------------------

    def get_selected_entity_id(self) -> int:
        return self._selected_entity_id
