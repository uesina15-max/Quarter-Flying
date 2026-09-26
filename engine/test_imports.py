import sys
import os

# Add editor directory to path
editor_dir = os.path.join(os.path.dirname(__file__), 'editor')
sys.path.insert(0, editor_dir)

print("Testing imports...")
print(f"Python version: {sys.version}")
print(f"Editor dir: {editor_dir}")

try:
    import PySide6
    print(f"PySide6 version: {PySide6.__version__}")
except ImportError as e:
    print(f"PySide6 import failed: {e}")

try:
    from PySide6.QtWidgets import QApplication
    print("PySide6.QtWidgets import successful")
except ImportError as e:
    print(f"PySide6.QtWidgets import failed: {e}")

try:
    from panels.scene_hierarchy import SceneHierarchyPanel
    print("scene_hierarchy import successful")
except ImportError as e:
    print(f"scene_hierarchy import failed: {e}")

try:
    from panels.inspector import InspectorPanel
    print("inspector import successful")
except ImportError as e:
    print(f"inspector import failed: {e}")

try:
    from panels.status_bar import StatusBar
    print("status_bar import successful")
except ImportError as e:
    print(f"status_bar import failed: {e}")

try:
    from style.theme import apply_theme
    print("theme import successful")
except ImportError as e:
    print(f"theme import failed: {e}")

try:
    from demo_scene_integration import DemoSceneIntegration
    print("demo_scene_integration import successful")
except ImportError as e:
    print(f"demo_scene_integration import failed: {e}")

try:
    from config_manager import ConfigManager
    print("config_manager import successful")
except ImportError as e:
    print(f"config_manager import failed: {e}")

print("\nAll basic imports completed")
