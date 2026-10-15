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

    # 저장 안 된 변경 표시(창 제목의 '*'). 기준은 엔진 CommandManager의 변경 카운터 하나다
    # (Execute/Undo/Redo가 성공할 때마다 오른다, core/CommandManager.h). 씬을 저장·열기·새로 만들 때의
    # 값을 기억해 두고 지금 값과 다르면 변경이 있다. 패널마다 "바꿨음" 플래그를 따로 세우지 않는다 -
    # 한 곳이라도 빠뜨리면 표시가 틀리고, 어느 패널이 빠뜨렸는지 추적하기 어렵다.
    # 한계: 명령을 거치지 않는 편집(editor_api가 없을 때의 Inspector 폴백 등)은 잡지 못한다.
    _clean_change_count: Optional[int] = None
    _dirty_before_play: bool = False
    _last_seen_change_count: Optional[int] = None

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
        if self._is_scene_dirty():
            scene = "*" + scene
        label = "Sample" if project.is_sample else project.name
        self.setWindowTitle(f"Quarter Flying Editor — {label} — {scene}")
        if hasattr(self, "lbl_scene"):
            self.lbl_scene.setText(f"  {scene}  ")

    # ── 저장 안 된 변경 ──────────────────────────────────────────────────────

    @staticmethod
    def _scene_change_count() -> Optional[int]:
        """엔진 CommandManager의 변경 카운터. 엔진이 없으면 None(그때는 변경 표시를 하지 않는다)."""
        try:
            from engine_binding import binding, HAS_ENGINE
            if not HAS_ENGINE:
                return None
            return int(binding.CommandManager.get_instance().get_change_count())
        except Exception:
            return None

    def _mark_scene_clean(self) -> None:
        """지금 월드를 "저장된 상태"로 본다. 씬을 저장·열기·새로 만든 직후에 부른다."""
        self._clean_change_count = self._scene_change_count()
        self._last_seen_change_count = self._clean_change_count
        self._update_title()

    def _is_scene_dirty(self) -> bool:
        if self._clean_change_count is None:
            return False
        return self._scene_change_count() != self._clean_change_count

    def _poll_scene_dirty(self) -> None:
        """주기적으로 부른다(main._on_fps_tick). 카운터가 바뀌었을 때만 제목을 다시 쓴다."""
        count = self._scene_change_count()
        if count != self._last_seen_change_count:
            self._last_seen_change_count = count
            self._update_title()

    def _remember_dirty_before_play(self) -> None:
        self._dirty_before_play = self._is_scene_dirty()

    def _restore_dirty_after_stop(self) -> None:
        """Stop은 월드를 Play 시작 시점으로 되돌린다. 그 사이 카운터가 올랐어도 변경 여부는 Play 전과 같다."""
        if self._dirty_before_play:
            self._clean_change_count = -1   # 어떤 카운터 값과도 다르다 -> 계속 "변경 있음"
        else:
            self._clean_change_count = self._scene_change_count()
        self._last_seen_change_count = None
        self._update_title()

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
        self._set_editor_project_root()
        self._load_input_actions()
        self._refresh_project_panels()

        start = project.start_scene_path
        if self._world is not None:
            if start and start.is_file():
                self._load_scene_file(start)
            else:
                self._new_scene_in_world()
        self.status.log(f"프로젝트 열기: {project.name} ({project.root})")
        self._update_title()

    def _set_editor_project_root(self) -> None:
        """EditorAPI에 현재 프로젝트 폴더를 알린다. 프리팹 경로를 이 폴더 기준 상대 경로로 기록·해석한다
        (engine/core/ProjectPath.h). 엔진 연결 때와 프로젝트를 바꿀 때 부른다."""
        editor_api = getattr(self, "_editor_api", None)
        if editor_api is not None:
            editor_api.set_project_root(str(current_project().root))

    def _load_input_actions(self) -> None:
        """현재 프로젝트의 입력 액션 맵(assets/input/actions.json)을 엔진에 불러온다.

        프로젝트를 열 때와 엔진이 연결될 때 부른다. 형식 검사는 엔진(InputActions::LoadActions)이 하고, 여기서는
        결과만 보여 준다. 거절된 파일이면 엔진은 이전 맵을 그대로 쓰므로, 사용자가 "왜 키가 안 먹지"를 찾지 않도록
        경고 창으로 알린다. 건너뛴 바인딩(모르는 키 이름 등)은 로그로 남긴다.
        """
        if self._engine is None:
            return
        path = current_project().input_actions_path
        try:
            report = self._engine.LoadInputActions(str(path))
        except RuntimeError as e:
            print(f"[InputActions] {path} 불러오기 실패: {e}")
            QMessageBox.warning(self, "입력 액션 맵",
                                f"입력 액션 맵을 불러오지 못했습니다. 이전 설정을 그대로 씁니다.\n\n{e}")
            return
        for w in report.get("warnings", []):
            print(f"[InputActions] {w}")
        panel = getattr(self, "input_actions_panel", None)
        if panel is not None:
            panel.reload_from_engine()
        if report.get("file_missing"):
            self.status.log(f"입력 액션 맵 없음: {path} (조작 액션 없이 시작)")
        else:
            skipped = len(report.get("warnings", []))
            self.status.log(f"입력 액션 {report.get('action_count', 0)}개 불러옴"
                            + (f" (경고 {skipped}개 - 콘솔 참고)" if skipped else ""))

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
        self._mark_scene_clean()   # 기본 카메라 생성도 명령을 거치므로, 그 뒤를 기준으로 삼는다

    def _load_scene_file(self, path: Path) -> bool:
        try:
            # 프로젝트 폴더를 넘겨야 옛 씬(version 1)의 절대 프리팹 경로가 프로젝트 상대 경로로 정규화된다.
            report = self._world.LoadScene(str(path), str(current_project().root))
        except RuntimeError as e:
            QMessageBox.critical(self, "Open Scene", str(e))
            return False
        self._scene_path = Path(path)
        for w in report.get("warnings", []):
            print(f"[SceneIO] {w}")
            self.status.log(f"씬 경고: {w}")
        unknown = list(report.get("unknown_components", []))
        if unknown:
            QMessageBox.warning(
                self, "Open Scene",
                "이 씬 파일에 이 엔진이 모르는 컴포넌트가 있어 건너뛰었습니다:\n  " + "\n  ".join(unknown))
        self.status.log(f"씬 열기: {path.name} ({report.get('entity_count', 0)}개 엔티티)")
        self._mark_scene_clean()
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
        self.status.log(f"씬 저장: {path.name} ({report.get('entity_count', 0)}개 엔티티) - Saved")
        self._mark_scene_clean()
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
