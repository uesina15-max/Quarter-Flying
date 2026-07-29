"""
editor/test_demo_scene.py

데모 씬 통합 테스트
"""

import sys
import os
import json

# 에디터 디렉토리를 경로에 추가
_EDITOR_DIR = os.path.dirname(os.path.abspath(__file__))
if _EDITOR_DIR not in sys.path:
    sys.path.insert(0, _EDITOR_DIR)

from demo_scene_integration import DemoSceneIntegration

def test_demo_scene_integration():
    """데모 씬 통합 기본 테스트"""
    print("=" * 50)
    print("Demo Scene Integration Test")
    print("=" * 50)
    
    # 테스트용 scene.json 생성
    test_scene_data = {
        "name": "Test Scene",
        "objects": [
            {
                "name": "TestCube",
                "model": "cube.obj",
                "material": {
                    "color": [1.0, 0.0, 0.0],
                    "roughness": 0.5,
                    "metallic": 0.0
                },
                "instances": [
                    {"position": [0, 0, 0]},
                    {"position": [1, 0, 0]}
                ]
            }
        ],
        "lights": [
            {
                "position": [0, 5, 0],
                "color": [1.0, 1.0, 1.0],
                "intensity": 1.0
            }
        ]
    }
    
    # 테스트 파일 작성
    test_scene_path = "test_scene.json"
    with open(test_scene_path, 'w') as f:
        json.dump(test_scene_data, f, indent=4)
    
    try:
        # DemoSceneIntegration 테스트
        demo = DemoSceneIntegration()
        
        # 시그널 핸들러
        loaded_called = False
        error_called = False
        
        def on_loaded(data):
            nonlocal loaded_called
            loaded_called = True
            print(f"✓ Scene loaded signal received")
        
        def on_error(msg):
            nonlocal error_called
            error_called = True
            print(f"✗ Scene error signal received: {msg}")
        
        demo.scene_loaded.connect(on_loaded)
        demo.scene_error.connect(on_error)
        
        # 씬 로드
        demo.load_scene(test_scene_path)
        
        # 결과 확인
        assert loaded_called, "Scene loaded signal not called"
        assert not error_called, "Scene error signal called unexpectedly"
        
        # 데이터 확인
        objects = demo.get_scene_objects()
        lights = demo.get_lights()
        
        assert len(objects) == 1, f"Expected 1 object, got {len(objects)}"
        assert len(lights) == 1, f"Expected 1 light, got {len(lights)}"
        
        obj = objects[0]
        assert obj['name'] == "TestCube", f"Expected 'TestCube', got {obj['name']}"
        assert len(obj['instances']) == 2, f"Expected 2 instances, got {len(obj['instances'])}"
        
        light = lights[0]
        assert light['intensity'] == 1.0, f"Expected intensity 1.0, got {light['intensity']}"
        
        print("✓ All assertions passed")
        print(f"✓ Objects loaded: {len(objects)}")
        print(f"✓ Lights loaded: {len(lights)}")
        
        return True
        
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
        
    finally:
        # 테스트 파일 정리
        if os.path.exists(test_scene_path):
            os.remove(test_scene_path)
            print("✓ Test file cleaned up")

def test_scene_hierarchy_integration():
    """씬 계층 패널 통합 테스트"""
    print("\n" + "=" * 50)
    print("Scene Hierarchy Integration Test")
    print("=" * 50)
    
    try:
        from panels.scene_hierarchy import SceneHierarchyPanel
        
        # 테스트 데이터
        test_scene_data = {
            "name": "Test Scene",
            "objects": [
                {
                    "name": "TestObject",
                    "model": "test.obj",
                    "instances": [
                        {"position": [0, 0, 0]}
                    ]
                }
            ],
            "lights": [
                {
                    "position": [0, 5, 0],
                    "color": [1.0, 1.0, 1.0]
                }
            ]
        }
        
        # SceneHierarchyPanel 테스트
        panel = SceneHierarchyPanel()
        panel.load_from_scene_data(test_scene_data)
        
        # 트리 아이템 확인
        root_count = panel.tree.topLevelItemCount()
        assert root_count > 0, f"Expected at least 1 root item, got {root_count}"
        
        root = panel.tree.topLevelItem(0)
        child_count = root.childCount()
        assert child_count > 0, f"Expected at least 1 child item, got {child_count}"
        
        print(f"✓ Tree root items: {root_count}")
        print(f"✓ Tree child items: {child_count}")
        print("✓ Scene hierarchy integration successful")
        
        return True
        
    except Exception as e:
        print(f"✗ Scene hierarchy test failed: {e}")
        return False

if __name__ == "__main__":
    # 테스트 실행
    test1_passed = test_demo_scene_integration()
    test2_passed = test_scene_hierarchy_integration()
    
    print("\n" + "=" * 50)
    print("Test Results Summary")
    print("=" * 50)
    print(f"Demo Scene Integration: {'PASS' if test1_passed else 'FAIL'}")
    print(f"Scene Hierarchy Integration: {'PASS' if test2_passed else 'FAIL'}")
    
    all_passed = test1_passed and test2_passed
    print(f"\nOverall: {'ALL TESTS PASSED' if all_passed else 'SOME TESTS FAILED'}")
    
    sys.exit(0 if all_passed else 1)
