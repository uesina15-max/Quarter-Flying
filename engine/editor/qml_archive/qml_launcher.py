"""
editor/qml_launcher.py

Qt Quick/QML 기반 현대화된 에디터 런처
"""

import sys
import os

# 에디터 디렉토리를 경로에 추가
_EDITOR_DIR = os.path.dirname(os.path.abspath(__file__))
if _EDITOR_DIR not in sys.path:
    sys.path.insert(0, _EDITOR_DIR)

from PySide6.QtWidgets import QApplication
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtCore import QObject, Signal, Slot, Property
from PySide6.QtGui import QGuiApplication

from qml_engine_integration import EngineBridge, InspectorModel

try:
    import ge_python
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False
    print("[QML Launcher] ge_python not found - running in UI-only mode")


class QMLEditorLauncher:
    """
    Qt Quick/QML 에디터 런처
    """
    
    def __init__(self):
        self.app = QGuiApplication(sys.argv)
        self.engine = QQmlApplicationEngine()
        
        # 브리지 객체 생성
        self.engine_bridge = EngineBridge()
        self.inspector_model = InspectorModel()
        
        self._setupQML()
        self._run()
    
    def _setupQML(self):
        """QML 설정"""
        # QML 리소스 경로 설정
        qml_path = os.path.join(_EDITOR_DIR, "qml")
        self.engine.addImportPath(qml_path)
        
        # 컨텍스트 속성 설정
        self.engine.rootContext().setContextProperty("engineBridge", self.engine_bridge)
        self.engine.rootContext().setContextProperty("inspectorModel", self.inspector_model)
        
        # QML 파일 로드
        main_qml = os.path.join(qml_path, "Main.qml")
        self.engine.load(main_qml)
        
        # 로드 오류 처리
        if not self.engine.rootObjects():
            print(f"[QML Launcher] Failed to load QML file: {main_qml}")
            sys.exit(-1)
        
        print("[QML Launcher] QML loaded successfully")
        
        # 기본 씬 로드 (테스트용)
        test_scene_path = os.path.join(_EDITOR_DIR, "..", "assets", "scene.json")
        if os.path.exists(test_scene_path):
            self.engine_bridge.loadScene(test_scene_path)
    
    def _run(self):
        """애플리케이션 실행"""
        print("[QML Launcher] Starting QML Editor...")
        self.app.exec()


if __name__ == "__main__":
    launcher = QMLEditorLauncher()
