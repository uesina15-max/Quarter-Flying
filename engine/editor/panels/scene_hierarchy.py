"""
Scene Hierarchy Panel
씬 계층 뷰 — 엔티티 트리를 표시하고 선택/생성/삭제를 지원
"""

import os

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QTreeWidget,
    QTreeWidgetItem, QPushButton, QMenu, QAbstractItemView,
    QSizePolicy, QFileDialog
)
from PySide6.QtCore import Qt, Signal, QTimer
from PySide6.QtGui import QIcon, QColor, QBrush, QFont, QAction

from style.theme import COLORS

# 엔진 모듈이 없는 경우에도 더미 모드로 동작
from engine_binding import HAS_ENGINE, to_entity

# engine/editor/panels/scene_hierarchy.py 기준 engine/assets/prefabs/
# (panels/prefab_browser.py도 같은 경로를 독립적으로 계산한다 - 패널 사이에
# 공유 "paths" 모듈이 아직 없어서, motion_mixer.py가 이미 하는 것과 같은 방식으로
# 각자 로컬 상수를 둔다.)
_PREFAB_ASSETS_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "assets", "prefabs",
)


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
    prefab_capture_status = Signal(str)  # 상태 바 표시용 (성공/실패 메시지 텍스트)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.registry = None          # ge_python ECSRegistry 참조 (2단계 연동)
        self.editor_api = None        # ge_python EditorAPI 참조 (Create Prefab 등에 필요)
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
        bar.setStyleSheet(f"background-color: {COLORS['bg_header']}; border-bottom: 1px solid {COLORS['border']};")
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

    def connect_editor(self, editor_api):
        """엔진 모듈의 EditorAPI 연결 (Create Prefab 등 명령형 API가 필요한 동작용)."""
        self.editor_api = editor_api

    def refresh_from_engine(self):
        """
        엔진 ECS에서 실제 엔티티 목록을 읽어와 트리를 갱신합니다.
        (ge_python.ECSRegistry.GetAllEntities() 필요)
        """
        if not self.registry or not HAS_ENGINE:
            return
        try:
            # 버그 1: GetAllEntities()는 ge_python.Entity 객체를 돌려주는데, 그걸 그대로
            # EntityItem(entity_id: int, ...)에 넣으면 entity_id가 Entity 객체가
            # 되어버린다. 이 값이 나중에 entity_selected(Signal(int))로 emit되는
            # 순간 PySide6가 Entity -> int 변환을 못 해서 조용히 0으로 깨진다 -
            # Scene Hierarchy에서 뭘 클릭해도 Inspector는 항상 entity 0을 보여주고
            # 있었다(docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 4 실측 검증 중 발견).
            # .id로 미리 풀어서 EntityItem.entity_id를 항상 raw int로 유지한다
            # (자체 타입 힌트도 원래 int였다 - create_entity()의 툴바 경로는 이미
            # entity.id를 이렇게 풀어서 쓰고 있었다).
            #
            # 버그 2: 이 메서드는 500ms 타이머로 반복 호출되는데(connect_registry()),
            # self.tree.clear()가 선택 상태를 통째로 지우고 아래 루프는 선택을 복원하지
            # 않았다 - 즉 무엇을 선택하든 0.5초 안에 항상 선택 해제되어 Inspector가
            # "No Selection"으로 돌아갔다(같은 실측 검증 중 발견). 갱신 전 선택된
            # entity_id를 기억해뒀다가, 다시 존재하면 재선택한다.
            previously_selected = self._selected_entity_id

            entities = self.registry.GetAllEntities()
            self.tree.clear()
            item_to_reselect = None
            for eid in entities:
                name = self.registry.GetEntityName(eid) if hasattr(self.registry, 'GetEntityName') else f"Entity_{eid.id}"
                item = EntityItem(eid.id, name)
                self.tree.addTopLevelItem(item)
                if eid.id == previously_selected:
                    item_to_reselect = item

            if item_to_reselect is not None:
                self.tree.setCurrentItem(item_to_reselect)
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
                self.registry.DestroyEntity(to_entity(entity_id))
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

            # 실제 ECS에 연결되어 있어야만 의미가 있다 - 연결 전 더미 트리 항목은
            # 캡처할 실제 컴포넌트가 없다.
            if self.registry and self.editor_api and HAS_ENGINE:
                action_prefab = QAction("📦  Create Prefab...", self)
                action_prefab.triggered.connect(lambda: self._create_prefab_from_item(item))
                menu.addAction(action_prefab)

            action_del = QAction("🗑️  Delete", self)
            action_del.triggered.connect(self.delete_selected_entity)
            menu.addAction(action_del)

        menu.exec(self.tree.viewport().mapToGlobal(pos))

    def _create_prefab_from_item(self, item: "EntityItem"):
        """선택한 엔티티를 *.prefab.json으로 저장한다
        (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 3, EditorAPI::CapturePrefab)."""
        if not self.editor_api:
            return

        os.makedirs(_PREFAB_ASSETS_DIR, exist_ok=True)

        # 트리 표시용 이모지/공백을 걷어내고 기본 저장 이름을 제안한다.
        raw_name = item.entity_name.strip()
        default_name = "".join(ch for ch in raw_name if ch.isalnum() or ch in ("_", "-")) or "Prefab"
        default_path = os.path.join(_PREFAB_ASSETS_DIR, f"{default_name}.prefab.json")

        path, _ = QFileDialog.getSaveFileName(
            self, "Create Prefab", default_path, "Prefab Files (*.prefab.json)"
        )
        if not path:
            return
        if not path.endswith(".prefab.json"):
            path += ".prefab.json"

        try:
            entity = to_entity(item.entity_id)
            self.editor_api.capture_prefab(entity, path)
            self.prefab_capture_status.emit(f"프리팹 생성 완료: {os.path.basename(path)}")
        except Exception as e:
            print(f"[SceneHierarchy] 프리팹 생성 실패: {e}")
            self.prefab_capture_status.emit(f"프리팹 생성 실패: {e}")

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
