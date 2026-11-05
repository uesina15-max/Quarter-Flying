"""
2026-11-04 제작 기능(docs/MASTER_PLAN.md 맨 위 표)의 회귀 검사. 실제 엔진 모듈과 네이티브 Qt 창을 쓴다.

프리팹 베리언트, 인게임 UI 콜백, 일시정지 중 UI, GPU 파티클 컬링, Material Editor 덮어쓰기, HiDPI 뷰포트.
화면 픽셀을 보는 검사는 창을 캡처할 수 없는 환경(잠긴 화면 등)이면 건너뛴다 - 건너뛰었다고 동작이 확인된 것은 아니다.

실행: PYTHONPATH에 engine/build/Release를 넣고 python test_production_features.py (QT_QPA_PLATFORM=offscreen 금지 -
GL 창이 필요하다).
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import time
import unittest
from pathlib import Path

_EDITOR = Path(__file__).resolve().parent
sys.path.insert(0, str(_EDITOR))

from PySide6.QtCore import Qt, QTimer  # noqa: E402
from PySide6.QtWidgets import (QApplication, QDialogButtonBox, QDoubleSpinBox, QFileDialog, QMainWindow,  # noqa: E402
                               QMessageBox, QWidget)

import quarterflying as qf  # noqa: E402

from core.prefab_variants import write_instance  # noqa: E402
from project import create_project, set_current_project  # noqa: E402

# 모듈이 끝까지 쥐고 있어야 한다. 테스트 함수의 지역 변수로 두면 함수가 끝날 때 QApplication이 사라지고, 위젯이 남은 채
# 인터프리터가 끝나면 종료 순서에 따라 access violation으로 죽는다(Python 프레임 없음, 실행마다 다름).
APP = QApplication.instance() or QApplication([])


class EngineTestCase(unittest.TestCase):
    width, height = 640, 360
    app = APP

    @classmethod
    def setUpClass(cls):
        cls.folder = Path(tempfile.mkdtemp(prefix="qf_production_한글_"))
        cls.project = create_project(cls.folder / "proj")
        set_current_project(cls.project)
        cls.host = QWidget()
        cls.host.setAttribute(Qt.WA_NativeWindow, True)
        cls.host.resize(cls.width, cls.height)
        cls.host.show()
        cls.app.processEvents()
        config = qf.EngineConfig()
        config.windowWidth, config.windowHeight = cls.width, cls.height
        config.assetRoot = str(cls.project.root)
        config.scriptHotReload = False
        cls.engine = qf.Engine()
        try:
            cls.engine.InitializeFromWindowHandle(int(cls.host.winId()), config)
        except RuntimeError as error:
            raise unittest.SkipTest(f"GL 창을 만들 수 없음: {error}")
        cls.world = cls.engine.CreateWorld("Production features")
        cls.engine.SetActiveWorld(cls.world)
        cls.registry = cls.world.GetRegistry()
        cls.api = qf.EditorAPI.get_instance()
        cls.api.set_registry(cls.registry)
        cls.api.set_project_root(str(cls.project.root))
        cls.engine.SetViewCamera(qf.ViewCamera.Game)

    @classmethod
    def tearDownClass(cls):
        cls.api.set_registry(None)
        qf.CommandManager.get_instance().clear()
        cls.engine.Shutdown()
        cls.host.close()
        cls.host.deleteLater()
        cls.host = None
        cls.app.processEvents()
        shutil.rmtree(cls.folder, ignore_errors=True)

    def setUp(self):
        self.world.NewScene()
        qf.CommandManager.get_instance().clear()

    def tearDown(self):
        self.world.Stop()
        self.tick(1)   # Edit 상태 한 프레임 = 게임 오버레이 정리(마우스·호버 상태가 다음 검사로 넘어가지 않게)

    def add(self, name, **components):
        entity = self.registry.CreateEntity()
        self.registry.SetEntityName(entity, name)
        for component, data in components.items():
            self.registry.SetComponentJson(entity, component, json.dumps(data))
        return entity

    def tick(self, frames=3):
        for _ in range(frames):
            self.engine.TickFrame()

    def run_for(self, seconds):
        end = time.perf_counter() + seconds
        while time.perf_counter() < end:
            self.engine.TickFrame()
            time.sleep(0.005)

    def grab(self):
        self.app.processEvents()
        return self.app.primaryScreen().grabWindow(int(self.host.winId())).toImage()

    def color(self, x, y):
        pixel = self.grab().pixelColor(x, y)
        return pixel.red(), pixel.green(), pixel.blue()


class PrefabVariantTests(EngineTestCase):
    def write_prefab(self, name, document):
        folder = self.project.assets_dir / "prefabs"
        folder.mkdir(exist_ok=True)
        path = folder / f"{name}.prefab.json"
        path.write_text(json.dumps(document), encoding="utf8")
        return path

    def test_untouched_instance_makes_an_empty_variant_that_inherits_base_changes(self):
        # 파일의 값(0.2, 1.6)은 float32를 거치면 달라지고, 파일에 없는 필드는 스냅샷에만 있다. 예전에는 이것들이 전부
        # override로 기록돼 이후 원본 변경이 상속되지 않았다.
        base = self.write_prefab("Sparks", {"version": 1, "name": "Sparks", "components": {
            "TransformComponent": {"position": [0.1, 0.2, 0.3], "scale": [0.6, 1.2, 0.6]},
            "ParticleEffectComponent": {"lifetime": 1.6, "drag": 0.2},
            "RenderableComponent": {"meshHandle": 0, "baseColor": [0.1, 0.2, 0.3]},
        }})
        instance = self.api.instantiate_prefab(base, qf.Vec3(10, 0, 0))
        variant = base.with_name("SparksVariant.prefab.json")
        write_instance(qf, self.registry, instance, self.project.root, base, variant)
        self.assertEqual(json.loads(variant.read_text(encoding="utf8"))["overrides"], [])

        document = json.loads(base.read_text(encoding="utf8"))
        document["components"]["ParticleEffectComponent"]["lifetime"] = 3.5
        document["components"]["TransformComponent"]["scale"] = [2, 2, 2]
        base.write_text(json.dumps(document), encoding="utf8")
        resolved = json.loads(qf.ReadPrefab(variant, str(self.project.root)))["components"]
        self.assertEqual(resolved["ParticleEffectComponent"]["lifetime"], 3.5)
        self.assertEqual(resolved["TransformComponent"]["scale"], [2, 2, 2])

    def test_variant_records_only_the_real_change_and_keeps_removals(self):
        base = self.write_prefab("Crate", {"version": 1, "name": "Crate", "components": {
            "TransformComponent": {"position": [1, 2, 3]},
            "RenderableComponent": {"baseColor": [0.9, 0.1, 0.1]},
            "BoxColliderComponent": {"halfExtents": [1, 1, 1]},
        }})
        instance = self.api.instantiate_prefab(base, qf.Vec3(5, 0, 0))
        self.registry.SetComponentJson(instance, "RenderableComponent", json.dumps({"baseColor": [0, 1, 0]}))
        self.api.remove_component(instance, "BoxColliderComponent")
        variant = base.with_name("CrateVariant.prefab.json")
        write_instance(qf, self.registry, instance, self.project.root, base, variant)
        overrides = json.loads(variant.read_text(encoding="utf8"))["overrides"]
        self.assertEqual(overrides, [{"path": [], "components": {
            "BoxColliderComponent": None, "RenderableComponent": {"baseColor": [0.0, 1.0, 0.0]}}}])
        resolved = json.loads(qf.ReadPrefab(variant, str(self.project.root)))["components"]
        self.assertNotIn("BoxColliderComponent", resolved)
        self.assertEqual(resolved["TransformComponent"]["position"], [1, 2, 3])

        # Apply to the variant keeps it a variant, with its own overrides only.
        second = self.api.instantiate_prefab(variant, qf.Vec3(9, 0, 0))
        self.registry.SetComponentJson(second, "RenderableComponent", json.dumps({"baseColor": [0, 0, 1]}))
        write_instance(qf, self.registry, second, self.project.root, variant)
        document = json.loads(variant.read_text(encoding="utf8"))
        self.assertIn("base", document)
        self.assertEqual(document["overrides"][0]["components"]["RenderableComponent"], {"baseColor": [0.0, 0.0, 1.0]})
        self.assertEqual(sorted(p.name for p in base.parent.glob("tmp*")), [])

    def test_inspector_shows_engine_errors_instead_of_failing_silently(self):
        # quarterflying.EngineError는 RuntimeError의 자식이 아니다. 예전에는 대화상자 없이 콘솔에만 남았다.
        from panels.inspector import InspectorPanel
        target = self.add("Target", TransformComponent={"position": [0, 0, 0]})
        prefab = self.write_prefab("Follower", {"version": 1, "name": "Follower", "components": {"TransformComponent": {}}})
        follower = self.api.instantiate_prefab(prefab, qf.Vec3(0, 0, 0))
        self.registry.SetComponentJson(follower, "CameraFollowComponent",
                                       json.dumps({"target": self.registry.GetEntityUUID(target)}))
        inspector = InspectorPanel()
        inspector.registry = self.registry
        messages = []
        original_warning, original_question = QMessageBox.warning, QMessageBox.question
        QMessageBox.warning = staticmethod(lambda _parent, title, text, *_a, **_k: messages.append((title, text)))
        QMessageBox.question = staticmethod(lambda *_a, **_k: QMessageBox.StandardButton.Yes)
        try:
            inspector._write_prefab(follower.id, "assets/prefabs/Follower.prefab.json", False)
        finally:
            QMessageBox.warning, QMessageBox.question = original_warning, original_question
        self.assertEqual(len(messages), 1, messages)
        self.assertIn("EntityRef", messages[0][1])


class GameUiTests(EngineTestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        (cls.folder / "ui_probe_module.py").write_text(textwrap.dedent("""
            import quarterflying
            calls = []
            class Probe:
                def __init__(self, entity):
                    self.entity = entity
                def on_ui_enter(self):
                    calls.append("enter")
                def on_ui_click(self):
                    calls.append("click")
                    quarterflying.input.set_cursor_locked(False)
                    calls.append("input-ok")
        """), encoding="utf8")
        sys.path.insert(0, str(cls.folder))

    def pointer(self, kind, x, y):
        event = qf.InputEvent()
        event.type, event.mouseButton = kind, qf.MouseButton.Left
        event.mouseX, event.mouseY = x, y
        self.engine.PushInputEvent(event)

    def add_button(self):
        return self.add("Button", RectTransformComponent={"x": 100, "y": 50, "width": 200, "height": 80},
                        UIButtonComponent={"interactable": True}, UITextComponent={"text": "Start"},
                        ScriptComponent={"module_name": "ui_probe_module", "class_name": "Probe"})

    def test_click_callback_can_use_the_input_api(self):
        import ui_probe_module
        ui_probe_module.calls.clear()
        self.add_button()
        self.world.Play()
        self.tick()
        self.pointer(qf.InputEventType.MouseMove, 150, 80)
        self.tick()
        self.pointer(qf.InputEventType.MouseButtonDown, 150, 80)
        self.pointer(qf.InputEventType.MouseButtonUp, 150, 80)
        self.tick()
        # "input-ok"가 없으면 on_ui_click 안에서 "no engine frame is running" 예외가 난 것이다.
        self.assertEqual(ui_probe_module.calls, ["enter", "click", "input-ok"])

    def test_ui_stays_on_screen_while_paused_and_ignores_clicks(self):
        import ui_probe_module
        ui_probe_module.calls.clear()
        self.add_button()
        self.world.Play()
        self.tick()
        playing = self.color(110, 60)
        if playing != (39, 55, 72):   # GameOverlay.cpp Paint의 버튼 배경색
            self.skipTest(f"창을 캡처할 수 없는 환경으로 보임: {playing}")
        self.world.Pause()
        self.tick()
        self.assertEqual(self.color(110, 60), playing)
        self.pointer(qf.InputEventType.MouseMove, 150, 80)
        self.pointer(qf.InputEventType.MouseButtonDown, 150, 80)
        self.pointer(qf.InputEventType.MouseButtonUp, 150, 80)
        self.tick()
        self.assertEqual(ui_probe_module.calls, [])
        self.world.Play()
        self.tick()
        self.assertEqual(ui_probe_module.calls, [])   # 멈춘 동안의 입력이 재개 후 클릭으로 처리되지 않는다


class ParticleCullingTests(EngineTestCase):
    def bright_pixels(self):
        image, count = self.grab(), 0
        for y in range(0, image.height(), 4):
            for x in range(0, image.width(), 4):
                pixel = image.pixelColor(x, y)
                count += min(pixel.red(), pixel.green(), pixel.blue()) >= 200
        return count

    def test_gpu_motion_particle_flying_into_view_is_not_culled_at_its_spawn_point(self):
        for gpu in (False, True):
            self.world.NewScene()
            self.add("Emitter", TransformComponent={"position": [0, -60, 0]},
                     ParticleStyleComponent={"gpuMotion": gpu},
                     ParticleEffectComponent={"spawnRate": 0, "burst": 1, "lifetime": 8, "initialSpeed": 40, "speedVariance": 0,
                                              "directionSpread": 0, "directionBase": [0, 1, 0], "gravity": 0, "drag": 0,
                                              "startSize": 3, "endSize": 3, "sizeVariance": 0, "startAlpha": 1, "endAlpha": 1,
                                              "startColor": [1, 1, 1], "endColor": [1, 1, 1], "maxParticle": 4})
            self.run_for(1.5)
            bright = self.bright_pixels()
            if not gpu and bright == 0:
                self.skipTest("CPU 경로도 보이지 않는다 - 창을 캡처할 수 없는 환경으로 보임")
            self.assertGreater(bright, 0, f"gpuMotion={gpu}")


class MaterialEditorTests(unittest.TestCase):
    def test_new_material_can_overwrite_an_existing_file(self):
        # 저장 대화상자에서 덮어쓰기를 승인한 뒤 "[WinError 183]"으로 실패하던 버그.
        from panels import production_tools as tools

        class Window(QMainWindow):
            _engine = None

            def _require_edit_mode(self, _what):
                return True

        app = APP
        folder = Path(tempfile.mkdtemp(prefix="qf_material_한글_"))
        try:
            project = create_project(folder / "proj")
            set_current_project(project)
            target = project.assets_dir / "materials" / "Used.material.json"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(json.dumps({"format": "quarterflying.material", "version": 1, "metallic": 1.0}), encoding="utf8")
            warnings = []
            originals = QFileDialog.getSaveFileName, QMessageBox.warning
            QFileDialog.getSaveFileName = staticmethod(lambda *_a, **_k: (str(target), ""))
            QMessageBox.warning = staticmethod(lambda _parent, title, text, *_a, **_k: warnings.append((title, text)))

            def fill_and_save():
                dialog = app.activeModalWidget()
                if dialog is None:
                    QTimer.singleShot(20, fill_and_save)
                    return
                dialog.findChildren(QDoubleSpinBox)[3].setValue(0.25)
                dialog.findChild(QDialogButtonBox).button(QDialogButtonBox.StandardButton.Save).click()
                if dialog.isVisible():
                    dialog.reject()

            window = Window()
            try:
                QTimer.singleShot(20, fill_and_save)
                tools.open_material(window)
            finally:
                QFileDialog.getSaveFileName, QMessageBox.warning = originals
                window.deleteLater()
                app.processEvents()
            self.assertEqual(warnings, [])
            saved = json.loads(target.read_text(encoding="utf8"))
            self.assertEqual(saved["roughness"], 0.25)
            self.assertEqual(saved["metallic"], 0.0)
            self.assertEqual([p.name for p in target.parent.iterdir()], [target.name])
        finally:
            shutil.rmtree(folder, ignore_errors=True)


_HIDPI_CHILD = r"""
import os, sys, tempfile
os.environ["QT_SCALE_FACTOR"] = "1.5"
sys.path.insert(0, sys.argv[1])
from pathlib import Path
from PySide6.QtWidgets import QApplication
app = QApplication([])
import quarterflying as qf
from project import create_project, set_current_project
from viewport import EngineViewport
project = create_project(Path(tempfile.mkdtemp(prefix="qf_hidpi_")) / "proj")
set_current_project(project)
view = EngineViewport(editor_camera=False)
view.resize(640, 360)
view.show()
app.processEvents()
print("RESULT", view.devicePixelRatioF(), view._to_pixels(view.width()), view._to_pixels(view.height()), view.initialized)
sys.stdout.flush()
view.timer.stop()
view.engine.Shutdown()
"""


class HiDpiTests(unittest.TestCase):
    def test_engine_gets_physical_pixels_on_scaled_displays(self):
        # 배율 150%에서 3D 화면과 UI가 위젯의 왼쪽 아래 2/3에만 그려지던 버그. QT_SCALE_FACTOR는 QApplication 전에 정해야
        # 하므로 별도 프로세스로 돌린다.
        environment = dict(os.environ)
        environment.pop("QT_QPA_PLATFORM", None)
        environment["PYTHONIOENCODING"] = "utf-8"
        done = subprocess.run([sys.executable, "-B", "-c", _HIDPI_CHILD, str(_EDITOR)], capture_output=True, text=True,
                              encoding="utf-8", errors="replace", env=environment, timeout=120)
        line = next((l for l in done.stdout.splitlines() if l.startswith("RESULT")), None)
        self.assertIsNotNone(line, done.stdout[-2000:] + done.stderr[-2000:])
        _tag, ratio, width, height, initialized = line.split()
        if initialized != "True":
            self.skipTest("GL 창을 만들 수 없음")
        self.assertEqual((float(ratio), int(width), int(height)), (1.5, 960, 540))


if __name__ == "__main__":
    unittest.main(verbosity=2)
