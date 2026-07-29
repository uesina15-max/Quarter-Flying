"""
editor/demo_scene_integration.py

C++ DemoScene 기능을 Python으로 이전
"""

from PySide6.QtCore import QObject, Signal
import json
import os

try:
    import ge_python
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False


class DemoSceneIntegration(QObject):
    """
    C++ DemoScene의 Python 래퍼
    """
    
    scene_loaded = Signal(object)  # 씬 로드 완료 시그널
    scene_error = Signal(str)     # 에러 시그널
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.scene_objects = []
        self.lights = []
        self.camera = None
        
    def load_scene(self, scene_path):
        """
        scene.json 로드
        """
        try:
            if not os.path.exists(scene_path):
                raise FileNotFoundError(f"Scene file not found: {scene_path}")
            
            with open(scene_path, 'r') as f:
                scene_data = json.load(f)
            
            # 오브젝트 로드
            if 'objects' in scene_data:
                self._load_objects(scene_data['objects'])
            
            # 라이트 로드
            if 'lights' in scene_data:
                self._load_lights(scene_data['lights'])
            
            self.scene_loaded.emit(scene_data)
            print(f"[DemoScene] Scene loaded: {scene_path}")
            
        except Exception as e:
            error_msg = f"Failed to load scene: {e}"
            self.scene_error.emit(error_msg)
            print(f"[DemoScene] {error_msg}")
    
    def _load_objects(self, objects_data):
        """오브젝트 데이터 로드"""
        self.scene_objects.clear()
        
        for obj_data in objects_data:
            scene_object = {
                'name': obj_data.get('name', 'unnamed'),
                'model': obj_data.get('model', ''),
                'material': obj_data.get('material', {}),
                'transform': obj_data.get('transform', {}),
                'instances': obj_data.get('instances', []),
                'grid': obj_data.get('grid', None)
            }
            self.scene_objects.append(scene_object)
    
    def _load_lights(self, lights_data):
        """라이트 데이터 로드"""
        self.lights.clear()
        
        for light_data in lights_data:
            light = {
                'position': light_data.get('position', [0, 0, 0]),
                'color': light_data.get('color', [1, 1, 1]),
                'intensity': light_data.get('intensity', 1.0)
            }
            self.lights.append(light)
    
    def get_scene_objects(self):
        """씬 오브젝트 반환"""
        return self.scene_objects
    
    def get_lights(self):
        """라이트 반환"""
        return self.lights
