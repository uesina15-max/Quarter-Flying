"""
editor/project_actions.py

File 메뉴의 프로젝트·씬·빌드 동작. docs/GAME_BUILD_PLAN.md Phase 1, Phase 4.

EditorMainWindow(main.py)에 섞어 쓰는 믹스인이다. main.py가 이미 크기 때문에 분리했다. 쓰는
EditorMainWindow 속성: _engine, _world, is_playing, status, viewport, prefab_browser,
motion_editor(있으면), config_manager, _on_stop(), lbl_scene.

규칙:
  - 씬 파일 저장/불러오기는 엔진(World.SaveScene/LoadScene, engine/scene/SceneIO.h)이 한다.
    실패는 RuntimeError로 오며, 원인 메시지를 그대로 사용자에게 보여 준다.
  - Play 중에는 씬을 열거나 저장하지 않는다. Play 중의 월드는 실행 결과로 바뀐 상태라, 저장하면
    편집한 적 없는 값이 파일에 들어가고, 열면 Stop이 Play 시작 시점으로 되돌려 버린다.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Optional

from PySide6.QtWidgets import QFileDialog, QMessageBox

from project import (ProjectError, SCENE_SUFFIX, create_project, current_project, open_project,
                     set_current_project)


class ProjectActionsMixin:

    _scene_path: Optional[Path] = None   # 지금 편집 중인 씬 파일(없으면 아직 저장 안 한 새 씬)

    # ── 메뉴 ─────────────────────────────────────────────────────────────────

    def _add_project_menu_actions(self, file_menu) -> None:
        from PySide6.QtGui import QAction
        items = [
            ("New Project…", None, self._on_new_project),
            ("Open Project…", None, self._on_open_project),
            None,
            ("New Scene", "Ctrl+N", self._on_new_scene),
            ("Open Scene…", "Ctrl+O", self._on_open_scene),
            ("Save Scene", "Ctrl+S", self._on_save_scene),
            ("Save Scene As…", "Ctrl+Shift+S", self._on_save_scene_as),
            None,
            ("Build Settings…", None, self._on_build_settings),
            ("Build Game", "Ctrl+B", self._on_build_game),
        ]
        for item in items:
            if item is None:
                file_menu.addSeparator()
                continue
            text, shortcut, slot = item
            act = QAction(text, self)
            if shortcut:
                act.setShortcut(shortcut)
            act.triggered.connect(slot)
            file_menu.addAction(act)

    # ── 상태 확인 ────────────────────────────────────────────────────────────

    def _require_world(self, what: str) -> bool:
        if self._world is None:
            QMessageBox.warning(self, what, "엔진이 연결되지 않아 이 작업을 할 수 없습니다.")
            return False
        return True

    def _require_edit_mode(self, what: str) -> bool:
        """Play 중이면 멈출지 묻고, 멈추지 않으면 False."""
        if not self.is_playing:
            return True
        answer = QMessageBox.question(
            self, what, "Play 중에는 할 수 없습니다. Play를 멈추고 계속할까요?",
            QMessageBox.Yes | QMessageBox.No)
        if answer != QMessageBox.Yes:
            return False
        self._on_stop()
        return not self.is_playing

    def _update_title(self) -> None:
        project = current_project()
        scene = self._scene_path.name if self._scene_path else "Untitled"
        label = "Sample" if project.is_sample else project.name
        self.setWindowTitle(f"Quarter Flying Editor — {label} — {scene}")
        if hasattr(self, "lbl_scene"):
            self.lbl_scene.setText(f"  {scene}  ")

    # ── 프로젝트 ─────────────────────────────────────────────────────────────

    def _restore_last_project(self) -> None:
        """설정에 기록된 마지막 프로젝트를 연다. 실패하면 샘플 프로젝트로 시작한다(이유는 로그)."""
        last = self.config_manager.get("project", "lastPath", default="")
        if not last:
            return
        try:
            set_current_project(open_project(last))
            print(f"[Project] 마지막 프로젝트를 열었습니다: {last}")
        except ProjectError as e:
            print(f"[Project] 마지막 프로젝트를 열지 못해 샘플 프로젝트로 시작합니다: {e}")

    def _on_new_project(self) -> None:
        if not self._require_edit_mode("New Project"):
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "새 프로젝트 폴더 (만들 폴더 이름을 입력)", str(Path.home()), "Folder (*)")
        if not path:
            return
        try:
            project = create_project(path)
        except (ProjectError, OSError) as e:
            QMessageBox.critical(self, "New Project", str(e))
            return
        self._switch_project(project)

    def _on_open_project(self) -> None:
        if not self._require_edit_mode("Open Project"):
            return
        path = QFileDialog.getExistingDirectory(self, "프로젝트 폴더 열기", str(Path.home()))
        if not path:
            return
        try:
            project = open_project(path)
        except ProjectError as e:
            QMessageBox.critical(self, "Open Project", str(e))
            return
        self._switch_project(project)

    def _switch_project(self, project) -> None:
        set_current_project(project)
        if not project.is_sample:
            self.config_manager.set("project", "lastPath", value=str(project.root))
            self.config_manager.save_config()
        if self._engine is not None:
            # meshPath/texturePath는 프로젝트 폴더 기준이다(project.py 모듈 주석).
            self._engine.SetAssetRoot(str(project.root))
        self._refresh_project_panels()

        start = project.start_scene_path
        if self._world is not None:
            if start and start.is_file():
                self._load_scene_file(start)
            else:
                self._new_scene_in_world()
        self.status.log(f"프로젝트 열기: {project.name} ({project.root})")
        self._update_title()

    def _refresh_project_panels(self) -> None:
        """경로를 프로젝트에서 받는 패널들의 목록을 다시 읽게 한다."""
        try:
            self.prefab_browser.refresh()
        except Exception as e:
            print(f"[Project] 프리팹 목록 갱신 실패: {e}")
        mixer = getattr(getattr(getattr(self, "motion_editor", None), "preview", None), "mixer_panel", None)
        if mixer is not None:
            try:
                mixer.refresh_asset_lists()
            except Exception as e:
                print(f"[Project] 모션 목록 갱신 실패: {e}")

    def _add_project_scripts_to_path(self) -> None:
        """Play 전에 부른다. ScriptComponent.module_name은 프로젝트 scripts/ 폴더에서 import된다."""
        import importlib
        scripts = str(current_project().scripts_dir)
        if os.path.isdir(scripts) and scripts not in sys.path:
            sys.path.insert(0, scripts)
            importlib.invalidate_caches()

    # ── 씬 ───────────────────────────────────────────────────────────────────

    def _new_scene_in_world(self) -> None:
        """월드를 비우고 기본 Main Camera 하나를 만든다(카메라가 없으면 게임 화면이 비기 때문)."""
        from demo_scene_seed import _seed_main_camera
        self._world.NewScene()
        registry = self._world.GetRegistry()
        editor_api = getattr(self, "_editor_api", None)
        if editor_api is not None:
            try:
                _seed_main_camera(editor_api, registry)
            except Exception as e:
                self.status.log(f"기본 카메라 생성 실패: {e}")
        self._scene_path = None
        self._update_title()

    def _load_scene_file(self, path: Path) -> bool:
        try:
            report = self._world.LoadScene(str(path))
        except RuntimeError as e:
            QMessageBox.critical(self, "Open Scene", str(e))
            return False
        self._scene_path = Path(path)
        unknown = list(report.get("unknown_components", []))
        if unknown:
            QMessageBox.warning(
                self, "Open Scene",
                "이 씬 파일에 이 엔진이 모르는 컴포넌트가 있어 건너뛰었습니다:\n  " + "\n  ".join(unknown))
        self.status.log(f"씬 열기: {path.name} ({report.get('entity_count', 0)}개 엔티티)")
        self._update_title()
        return True

    def _save_scene_file(self, path: Path) -> bool:
        try:
            report = self._world.SaveScene(str(path))
        except RuntimeError as e:
            QMessageBox.critical(self, "Save Scene", str(e))
            return False
        self._scene_path = Path(path)
        missing = list(report.get("unregistered_components", []))
        if missing:
            QMessageBox.warning(
                self, "Save Scene",
                "다음 컴포넌트는 리플렉션에 등록되지 않아 파일에 저장되지 않았습니다:\n  " + "\n  ".join(missing))
        project = current_project()
        if not project.is_sample and not project.start_scene:
            # 첫 저장한 씬을 시작 씬으로 정한다(빌드에 시작 씬이 필요하다).
            try:
                project.start_scene = project.relative(path)
                project.save()
            except Exception as e:
                self.status.log(f"시작 씬 기록 실패: {e}")
        self.status.log(f"씬 저장: {path.name} ({report.get('entity_count', 0)}개 엔티티)")
        self._update_title()
        return True

    def _on_new_scene(self) -> None:
        if not self._require_world("New Scene") or not self._require_edit_mode("New Scene"):
            return
        self._new_scene_in_world()

    def _on_open_scene(self) -> None:
        if not self._require_world("Open Scene") or not self._require_edit_mode("Open Scene"):
            return
        start_dir = current_project().scenes_dir
        path, _ = QFileDialog.getOpenFileName(
            self, "씬 열기", str(start_dir if start_dir.is_dir() else current_project().root),
            f"Scene Files (*{SCENE_SUFFIX})")
        if path:
            self._load_scene_file(Path(path))

    def _on_save_scene(self) -> None:
        if self._scene_path is None:
            self._on_save_scene_as()
            return
        if not self._require_world("Save Scene") or not self._require_edit_mode("Save Scene"):
            return
        self._save_scene_file(self._scene_path)

    def _on_save_scene_as(self) -> None:
        if not self._require_world("Save Scene As") or not self._require_edit_mode("Save Scene As"):
            return
        scenes = current_project().scenes_dir
        scenes.mkdir(parents=True, exist_ok=True)
        path, _ = QFileDialog.getSaveFileName(
            self, "씬 저장", str(scenes / f"Main{SCENE_SUFFIX}"), f"Scene Files (*{SCENE_SUFFIX})")
        if not path:
            return
        if not path.endswith(SCENE_SUFFIX):
            # 대화상자가 ".json"만 붙이는 경우가 있어 접미사를 정규화한다(빌드의 씬 검색이 이 접미사를 쓴다).
            path = os.path.splitext(path)[0]
            if path.endswith(".scene"):
                path = path[: -len(".scene")]
            path += SCENE_SUFFIX
        self._save_scene_file(Path(path))

    # ── 빌드 (Phase 4) ───────────────────────────────────────────────────────

    def _on_build_settings(self) -> None:
        from gamebuild.build_dialog import BuildSettingsDialog
        project = current_project()
        if project.is_sample:
            QMessageBox.information(self, "Build Settings",
                                    "샘플 프로젝트는 빌드 설정을 저장할 수 없습니다. 프로젝트를 만들거나 여세요.")
            return
        BuildSettingsDialog(project, self).exec()

    def _on_build_game(self) -> None:
        from gamebuild.build_dialog import run_build_with_dialog
        project = current_project()
        if project.is_sample:
            QMessageBox.information(self, "Build Game",
                                    "샘플 프로젝트는 빌드할 수 없습니다. File > New Project로 프로젝트를 만드세요.")
            return
        if self._scene_path is not None and self._world is not None and not self.is_playing:
            answer = QMessageBox.question(
                self, "Build Game", "빌드 전에 현재 씬을 저장할까요?",
                QMessageBox.Yes | QMessageBox.No | QMessageBox.Cancel)
            if answer == QMessageBox.Cancel:
                return
            if answer == QMessageBox.Yes and not self._save_scene_file(self._scene_path):
                return
        run_build_with_dialog(project, self)
