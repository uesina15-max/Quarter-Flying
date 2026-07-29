"""
editor/test_viewport_compatibility.py

기존 뷰포트 호환성 검증 테스트
"""

import sys
import os

# 에디터 디렉토리를 경로에 추가
_EDITOR_DIR = os.path.dirname(os.path.abspath(__file__))
if _EDITOR_DIR not in sys.path:
    sys.path.insert(0, _EDITOR_DIR)

try:
    from viewport import EngineViewport
    from main import EditorMainWindow
    from PySide6.QtWidgets import QApplication
    HAS_ENGINE = True
except ImportError as e:
    print(f"[Test] Import failed: {e}")
    HAS_ENGINE = False

def test_viewport_import():
    """뷰포트 임포트 테스트"""
    try:
        from viewport import EngineViewport
        print("✓ EngineViewport import successful")
        return True
    except ImportError as e:
        print(f"✗ EngineViewport import failed: {e}")
        return False

def test_main_editor_import():
    """메인 에디터 임포트 테스트"""
    try:
        from main import EditorMainWindow
        print("✓ EditorMainWindow import successful")
        return True
    except ImportError as e:
        print(f"✗ EditorMainWindow import failed: {e}")
        return False

def test_ge_python_binding():
    """ge_python 바인딩 테스트"""
    try:
        import ge_python
        print("✓ ge_python import successful")
        
        # EngineConfig 테스트
        config = ge_python.EngineConfig()
        config.windowTitle = "Test"
        config.windowWidth = 800
        config.windowHeight = 600
        print("✓ EngineConfig creation successful")
        
        # InputEvent 테스트
        event = ge_python.InputEvent()
        event.type = ge_python.InputEventType.KeyDown
        event.keyCode = ge_python.KeyCode.A
        print("✓ InputEvent creation successful")
        
        return True
    except ImportError as e:
        print(f"✗ ge_python import failed: {e}")
        return False
    except Exception as e:
        print(f"✗ ge_python binding test failed: {e}")
        return False

def test_engine_initialization():
    """엔진 초기화 테스트 (윈도우 없이)"""
    try:
        import ge_python
        
        # Engine 객체 생성만 테스트 (초기화는 윈도우 핸들 필요)
        engine = ge_python.Engine()
        print("✓ Engine object creation successful")
        
        return True
    except Exception as e:
        print(f"✗ Engine creation failed: {e}")
        return False

def test_window_handle_binding():
    """윈도우 핸들 바인딩 테스트"""
    try:
        import ge_python
        from PySide6.QtWidgets import QApplication, QWidget
        
        app = QApplication.instance()
        if app is None:
            app = QApplication(sys.argv)
        
        widget = QWidget()
        window_handle = int(widget.winId())
        
        print(f"✓ Window handle retrieval successful: {window_handle}")
        
        # InitializeFromWindowHandle 메서드 존재 확인
        engine = ge_python.Engine()
        if hasattr(engine, 'InitializeFromWindowHandle'):
            print("✓ InitializeFromWindowHandle method exists")
            return True
        else:
            print("✗ InitializeFromWindowHandle method not found")
            return False
            
    except Exception as e:
        print(f"✗ Window handle binding test failed: {e}")
        return False

def run_all_tests():
    """모든 테스트 실행"""
    print("=" * 50)
    print("Viewport Compatibility Test Suite")
    print("=" * 50)
    
    tests = [
        ("Viewport Import", test_viewport_import),
        ("Main Editor Import", test_main_editor_import),
        ("ge_python Binding", test_ge_python_binding),
        ("Engine Initialization", test_engine_initialization),
        ("Window Handle Binding", test_window_handle_binding)
    ]
    
    results = []
    for test_name, test_func in tests:
        print(f"\n[Testing] {test_name}")
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"✗ {test_name} raised exception: {e}")
            results.append((test_name, False))
    
    print("\n" + "=" * 50)
    print("Test Results Summary")
    print("=" * 50)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "PASS" if result else "FAIL"
        print(f"{status}: {test_name}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    
    return passed == total

if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
