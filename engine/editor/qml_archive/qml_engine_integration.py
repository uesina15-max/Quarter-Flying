"""
editor/qml_engine_integration.py

C++ 엔진과 QML 사이의 Python 브리지 클래스
"""

import sys
import os
from typing import List, Dict, Any
from PySide6.QtCore import QObject, Signal, Slot, Property, QAbstractListModel, Qt
from PySide6.QtGui import QVector3D

try:
    import ge_python
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False
    print("[QML Engine Integration] ge_python not found - running in UI-only mode")


class SceneItem:
    """씬 계층 아이템 데이터 클래스"""
    def __init__(self, name: str, item_type: str, depth: int = 0):
        self.name = name
        self.type = item_type
        self.depth = depth


class SceneModel(QAbstractListModel):
    """씬 계층 모델 (QML ListView용)"""
    
    # QML 역할 정의
    NameRole = Qt.UserRole + 1
    TypeRole = Qt.UserRole + 2
    DepthRole = Qt.UserRole + 3
    
    def __init__(self):
        super().__init__()
        self._items: List[SceneItem] = []
        
    def roleNames(self):
        return [self.NameRole, self.TypeRole, self.DepthRole]
    
    def rowCount(self, parent=None):
        return len(self._items)
    
    def data(self, index, role=Qt.DisplayRole):
        if not index.isValid() or index.row() >= len(self._items):
            return None
            
        item = self._items[index.row()]
        
        if role == self.NameRole:
            return item.name
        elif role == self.TypeRole:
            return item.type
        elif role == self.DepthRole:
            return item.depth
        return None
    
    def add_item(self, name: str, item_type: str, depth: int = 0):
        """아이템 추가"""
        self.beginInsertRows(self.index(len(self._items)), self.index(len(self._items)))
        self._items.append(SceneItem(name, item_type, depth))
        self.endInsertRows()
    
    def clear_items(self):
        """모든 아이템 제거"""
        self.beginResetModel()
        self._items.clear()
        self.endResetModel()
    
    def load_from_json(self, scene_data: Dict[str, Any]):
        """JSON 데이터로부터 씬 로드"""
        self.clear_items()
        
        # 루트 아이템
        self.add_item("World Root", "root", 0)
        
        # 오브젝트 로드
        if 'objects' in scene_data:
            self._load_objects(scene_data['objects'], 1)
        
        # 라이트 로드
        if 'lights' in scene_data:
            self._load_lights(scene_data['lights'], 1)
    
    def _load_objects(self, objects_data: List[Dict], depth: int):
        """오브젝트 데이터 로드"""
        for obj_data in objects_data:
            name = obj_data.get('name', 'unnamed')
            obj_type = 'mesh'
            
            # 타입 결정
            if 'grid' in obj_data:
                obj_type = 'folder'
            elif 'instances' in obj_data:
                obj_type = 'entity'
            
            self.add_item(name, obj_type, depth)
            
            # 하위 아이템 (인스턴스)
            if 'instances' in obj_data:
                for i, instance in enumerate(obj_data['instances']):
                    inst_name = f"{name}_Instance_{i}"
                    self.add_item(inst_name, 'mesh', depth + 1)
    
    def _load_lights(self, lights_data: List[Dict], depth: int):
        """라이트 데이터 로드"""
        for i, light_data in enumerate(lights_data):
            light_name = f"Light_{i}"
            self.add_item(light_name, 'light', depth)


class EngineBridge(QObject):
    """
    QML과 C++ 엔진 사이의 메인 브리지 클래스
    """
    
    # 시그널 정의
    sceneLoaded = Signal(str)
    engineStatusChanged = Signal(str)
    objectSelected = Signal(str, str)
    playModeChanged = Signal(bool)
    
    def __init__(self):
        super().__init__()
        self._engine = None
        self._world = None
        self._initialized = False
        self._is_playing = False
        
        # 씬 모델
        self._scene_model = SceneModel()
        
    @Property(QObject, constant=True)
    def sceneModel(self):
        """씬 모델 (QML용)"""
        return self._scene_model
    
    @Slot(bool)
    def initializeEngine(self, enable_engine: bool):
        """엔진 초기화"""
        if enable_engine and HAS_ENGINE:
            try:
                self._engine = ge_python.Engine()
                config = ge_python.EngineConfig()
                config.windowTitle = "Quarter Flying QML Editor"
                config.windowWidth = 1280
                config.windowHeight = 720
                
                # 엔진 초기화 (윈도우 핸들은 QML 위젯에서 받아야 함)
                # 현재는 UI 전용 모드로 작동
                self._initialized = True
                self.engineStatusChanged.emit("Engine initialized")
                print("[EngineBridge] Engine initialized successfully")
                
            except Exception as e:
                self._initialized = False
                self.engineStatusChanged.emit(f"Engine initialization failed: {e}")
                print(f"[EngineBridge] Engine initialization error: {e}")
        else:
            self._initialized = False
            self.engineStatusChanged.emit("Engine disabled (UI-only mode)")
            print("[EngineBridge] Running in UI-only mode")
    
    @Slot(str)
    def loadScene(self, scene_path: str):
        """씬 로드"""
        try:
            import json
            print(f"[EngineBridge] Loading scene: {scene_path}")
            
            if os.path.exists(scene_path):
                with open(scene_path, 'r') as f:
                    scene_data = json.load(f)
                
                # 씬 모델 업데이트
                self._scene_model.load_from_json(scene_data)
                
                self.sceneLoaded.emit(scene_path)
                print(f"[EngineBridge] Scene loaded: {scene_path}")
            else:
                print(f"[EngineBridge] Scene file not found: {scene_path}")
                
        except Exception as e:
            print(f"[EngineBridge] Scene load error: {e}")
    
    @Slot()
    def play(self):
        """플레이 모드 시작"""
        if self._engine and self._initialized:
            try:
                if not self._is_playing:
                    self._is_playing = True
                    print("[EngineBridge] Starting play mode")
                    self.playModeChanged.emit(True)
                    
                    # 엔진 플레이 로직
                    if self._world:
                        self._world.Play()
            except Exception as e:
                print(f"[EngineBridge] Play error: {e}")
    
    @Slot()
    def pause(self):
        """일시정지"""
        if self._engine and self._initialized:
            try:
                if self._is_playing:
                    self._is_playing = False
                    print("[EngineBridge] Pausing")
                    self.playModeChanged.emit(False)
                    
                    # 엔진 일시정지 로직
                    if self._world:
                        self._world.Pause()
            except Exception as e:
                print(f"[EngineBridge] Pause error: {e}")
    
    @Slot()
    def stop(self):
        """플레이 모드 정지"""
        if self._engine and self._initialized:
            try:
                if self._is_playing:
                    self._is_playing = False
                    print("[EngineBridge] Stopping play mode")
                    self.playModeChanged.emit(False)
                    
                    # 엔진 정지 로직
                    if self._world:
                        self._world.Stop()
            except Exception as e:
                print(f"[EngineBridge] Stop error: {e}")
    
    @Slot(str, str)
    def selectObject(self, object_name: str, object_type: str):
        """오브젝트 선택"""
        print(f"[EngineBridge] Object selected: {object_name} ({object_type})")
        self.objectSelected.emit(object_name, object_type)
    
    @Slot(str, float)
    def updateProperty(self, property_name: str, value: float):
        """속성 업데이트"""
        print(f"[EngineBridge] Property update: {property_name} = {value}")
        # 실제 엔진 속성 업데이트 로직
    
    # 속성
    def getEngineStatus(self):
        return "Ready" if self._initialized else "Not initialized"
    
    engineStatus = Property(str, getEngineStatus, notify=engineStatusChanged)
    
    def getIsPlaying(self):
        return self._is_playing
    
    isPlaying = Property(bool, getIsPlaying, notify=playModeChanged)


class InspectorModel(QObject):
    """인스펙터 데이터 모델"""
    
    objectNameChanged = Signal()
    objectTypeChanged = Signal()
    
    def __init__(self):
        super().__init__()
        self._object_name = "No Selection"
        self._object_type = ""
        
        # 속성 데이터
        self._transform = {
            'position': QVector3D(0, 0, 0),
            'rotation': QVector3D(0, 0, 0),
            'scale': QVector3D(1, 1, 1)
        }
        
        self._material = {
            'color': '#FF5722',
            'roughness': 0.5,
            'metallic': 0.0
        }
    
    @Property(str, notify=objectNameChanged)
    def objectName(self):
        return self._object_name
    
    @objectName.setter
    def objectName(self, value):
        if self._object_name != value:
            self._object_name = value
            self.objectNameChanged.emit()
    
    @Property(str, notify=objectTypeChanged)
    def objectType(self):
        return self._object_type
    
    @objectType.setter
    def objectType(self, value):
        if self._object_type != value:
            self._object_type = value
            self.objectTypeChanged.emit()
    
    @Slot(str, str)
    def selectObject(self, name: str, obj_type: str):
        """오브젝트 선택"""
        self.objectName = name
        self.objectType = obj_type
        print(f"[InspectorModel] Selected: {name} ({obj_type})")
