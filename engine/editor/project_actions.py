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
  - 프로젝트 단위 저장(File > Save Project): 씬·입력 액션 맵·Motion Editor 액션 중 바뀐 것 + project.json +
    세션(<프로젝트>/.quarterflying/session.json - 열어 둔 씬·액션, 에디터 카메라, 모드)을 한 번에 저장한다.
    무엇이 바뀌었는지는 _unsaved_items() 한 곳에서 모은다. 같은 목록으로 종료·프로젝트 전환·씬 전환 전에
    "저장 / 저장 안 함 / 취소"를 묻는다(예전에는 묻지 않아서 저장 안 한 작업이 경고 없이 사라졌다).
"""

from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import List, Optional

from PySide6.QtWidgets import QFileDialog, QMessageBox

from project import (CameraView, EditorSession, MODE_MOTION, MODE_SCENE, ProjectError, SCENE_SUFFIX, SaveItem,
                     create_project, current_project, from_session_path, load_session, open_project, save_items,
                     save_session, set_current_project, to_session_path)


class ProjectActionsMixin:

    _scene_path: Optional[Path] = None   # 지금 편집 중인 씬 파일(없으면 아직 저장 안 한 새 씬)
    _session: Optional[EditorSession] = None   # 지금 프로젝트를 열 때 읽은 세션(project/session.py)

    # 저장 안 된 변경 표시(창 제목의 '*'). 기준은 엔진 CommandManager의 변경 카운터 하나다
    # (Execute/Undo/Redo가 성공할 때마다 오른다, core/CommandManager.h). 씬을 저장·열기·새로 만들 때의
    # 값을 기억해 두고 지금 값과 다르면 변경이 있다. 패널마다 "바꿨음" 플래그를 따로 세우지 않는다 -
    # 한 곳이라도 빠뜨리면 표시가 틀리고, 어느 패널이 빠뜨렸는지 추적하기 어렵다.
    # 한계: 명령을 거치지 않는 편집은 잡지 못한다. 예전 이 주석은 그런 편집이 "editor_api가 없을 때의 Inspector
    # 폴백"뿐이라고 가정했지만, 실제로는 엔진이 연결된 평상시에도 Inspector의 position·parent 외 필드가 전부
    # 레지스트리에 직접 쓰여서 rotation 등을 바꾸고 닫으면 경고 없이 사라졌다(2026-10-22 수정,
    # docs/EDITOR_BUGFIX_PLAN_2026-10-22.md F1). 새 편집 경로를 만들면 EditorAPI 명령을 거치게 할 것.
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
            ("Save Project", "Ctrl+Alt+S", self._on_save_project),
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
            # Undo/Redo 메뉴도 여기서 갱신한다. 예전에는 계층 패널 동작·Undo/Redo 직후에만 갱신해서, Inspector
            # 편집만 한 경우 Edit > Undo(Ctrl+Z)가 꺼진 채로 남아 되돌릴 수 없었다.
            if hasattr(self, "_update_undo_redo_actions"):
                self._update_undo_redo_actions()

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
        if not path or not self._confirm_unsaved("New Project", include_project_data=True):
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
        if not path or not self._confirm_unsaved("Open Project", include_project_data=True):
            return
        try:
            project = open_project(path)
        except ProjectError as e:
            QMessageBox.critical(self, "Open Project", str(e))
            return
        self._switch_project(project)

    def _switch_project(self, project) -> None:
        # 떠나는 프로젝트의 "하던 자리"를 먼저 기록한다(저장 안 된 변경은 호출부가 _confirm_unsaved로 이미 물었다).
        self._write_session_quietly()
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

        # 세션: 액션 목록·카메라는 바로, 씬은 세션 씬(있으면) → 시작 씬 순서로, 모드는 씬을 연 뒤에.
        self._apply_session_ui(self._load_project_session())
        start = self._session_scene_path() or project.start_scene_path
        if self._world is not None:
            if start and start.is_file():
                self._load_scene_file(start)
            else:
                self._new_scene_in_world()
        self._apply_session_mode()
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
        if not self._confirm_unsaved("New Scene", include_project_data=False):
            return
        self._new_scene_in_world()

    def _on_open_scene(self) -> None:
        if not self._require_world("Open Scene") or not self._require_edit_mode("Open Scene"):
            return
        start_dir = current_project().scenes_dir
        path, _ = QFileDialog.getOpenFileName(
            self, "씬 열기", str(start_dir if start_dir.is_dir() else current_project().root),
            f"Scene Files (*{SCENE_SUFFIX})")
        if path and self._confirm_unsaved("Open Scene", include_project_data=False):
            self._load_scene_file(Path(path))

    def _on_save_scene(self) -> None:
        self._save_current_scene()

    def _save_current_scene(self) -> bool:
        """Save Scene. 파일이 없는 새 씬이면 위치를 묻는다. 저장했으면 True(Save Project도 이것을 부른다)."""
        if self._scene_path is None:
            return self._save_scene_as()
        if not self._require_world("Save Scene") or not self._require_edit_mode("Save Scene"):
            return False
        return self._save_scene_file(self._scene_path)

    def _on_save_scene_as(self) -> None:
        self._save_scene_as()

    def _save_scene_as(self) -> bool:
        if not self._require_world("Save Scene As") or not self._require_edit_mode("Save Scene As"):
            return False
        scenes = current_project().scenes_dir
        scenes.mkdir(parents=True, exist_ok=True)
        path, _ = QFileDialog.getSaveFileName(
            self, "씬 저장", str(scenes / f"Main{SCENE_SUFFIX}"), f"Scene Files (*{SCENE_SUFFIX})")
        if not path:
            return False
        if not path.endswith(SCENE_SUFFIX):
            # 대화상자가 ".json"만 붙이는 경우가 있어 접미사를 정규화한다(빌드의 씬 검색이 이 접미사를 쓴다).
            path = os.path.splitext(path)[0]
            if path.endswith(".scene"):
                path = path[: -len(".scene")]
            path += SCENE_SUFFIX
        return self._save_scene_file(Path(path))

    # ── 프로젝트 저장 / 저장 안 된 변경 ──────────────────────────────────────

    def _unsaved_items(self, include_project_data: bool) -> List[SaveItem]:
        """저장 안 된 변경 목록. 판단 기준은 대상마다 하나씩이다:
          - 씬: CommandManager 변경 카운터(_is_scene_dirty)
          - 입력 액션 맵: InputActionsPanel.is_dirty(프로퍼티)
          - 액션: ActionData.is_modified()(파일 내용과 비교)
        include_project_data=False면 씬만 본다(씬을 바꿀 때 - 액션·입력 맵은 씬과 무관하게 남는다).
        """
        items: List[SaveItem] = []
        if self._world is not None and self._is_scene_dirty():
            name = self._scene_path.name if self._scene_path else "Untitled (새 씬)"
            items.append(SaveItem(f"씬 {name}", self._save_current_scene))
        if not include_project_data:
            return items
        panel = getattr(self, "input_actions_panel", None)
        if panel is not None and panel.is_dirty:
            items.append(SaveItem("입력 액션 맵 (assets/input/actions.json)", panel.save))
        motion = getattr(self, "motion_editor", None)
        if motion is not None and hasattr(motion, "unsaved_actions"):
            for action in motion.unsaved_actions():
                label = f"액션 {action.name}" + ("" if action.source_path else " (새 액션)")
                items.append(SaveItem(label, lambda a=action: motion.save_action_data(a)))
        return items

    def _confirm_unsaved(self, what: str, include_project_data: bool) -> bool:
        """저장 안 된 변경이 있으면 저장 / 저장 안 함 / 취소를 묻는다. 계속해도 되면 True.

        "저장"을 골랐는데 하나라도 저장하지 못하면(저장 위치 대화상자 취소 포함) 계속하지 않는다 - 계속하면
        그 변경이 사라진다.
        """
        items = self._unsaved_items(include_project_data)
        if not items:
            return True
        names = "\n  ".join(item.label for item in items)
        answer = QMessageBox.question(
            self, what, f"저장하지 않은 변경이 있습니다:\n  {names}\n\n저장할까요?",
            QMessageBox.Save | QMessageBox.Discard | QMessageBox.Cancel, QMessageBox.Save)
        if answer == QMessageBox.Discard:
            return True
        if answer != QMessageBox.Save:
            return False
        result = save_items(items)
        self.status.log(f"{what}: {result.summary()}")
        self._update_title()
        if not result.ok:
            QMessageBox.warning(self, what, "다음 항목을 저장하지 못해 중단합니다:\n" + result.failure_details())
            return False
        return True

    def _on_save_project(self) -> None:
        """File > Save Project: 바뀐 것을 전부 저장하고 project.json·세션을 기록한다. 대상별 결과를 상태바에 남긴다."""
        if not self._require_edit_mode("Save Project"):
            return
        items = self._unsaved_items(include_project_data=True)
        if not current_project().is_sample:
            items.append(SaveItem("project.json", self._save_project_file))
        items.append(SaveItem("세션 (.quarterflying/session.json)", self._write_session))
        result = save_items(items)
        self.status.log(f"프로젝트 저장: {result.summary()}" + (" - Saved" if result.ok else ""))
        self._update_title()
        if not result.ok:
            QMessageBox.warning(self, "Save Project", "다음 항목을 저장하지 못했습니다:\n" + result.failure_details())

    @staticmethod
    def _save_project_file() -> bool:
        current_project().save()
        return True

    # ── 세션 (project/session.py) ────────────────────────────────────────────

    def _capture_session(self) -> EditorSession:
        """지금 에디터 상태 → 세션. 디스크에 있는 파일만 적는다(저장 안 한 새 씬·새 액션은 다시 열 수 없다)."""
        project = current_project()
        session = EditorSession()
        if self._scene_path is not None and Path(self._scene_path).is_file():
            session.scene = to_session_path(project, self._scene_path)
        motion = getattr(self, "motion_editor", None)
        if motion is not None and hasattr(motion, "open_action_paths"):
            session.actions = [to_session_path(project, p) for p in motion.open_action_paths() if os.path.isfile(p)]
            selected = motion.selected_action_path()
            session.selected_action = to_session_path(project, selected) if selected else ""
        stack = getattr(self, "stack", None)
        session.mode = MODE_MOTION if stack is not None and stack.currentIndex() == MODE_MOTION else MODE_SCENE
        viewport = getattr(self, "viewport", None)
        view = viewport.editor_camera_view() if hasattr(viewport, "editor_camera_view") else None
        if view is not None:
            target, distance, yaw, pitch = view
            session.camera = CameraView(target=list(target), distance=distance, yaw=yaw, pitch=pitch)
        return session

    def _write_session(self) -> bool:
        path = save_session(current_project(), self._capture_session())
        print(f"[Session] 세션 저장: {path}")
        return True

    def _write_session_quietly(self) -> None:
        """종료·프로젝트 전환 때. 세션은 편의 데이터라 실패해도 막지 않고 원인만 남긴다."""
        try:
            self._write_session()
        except OSError as e:
            print(f"[Session] 세션을 저장하지 못했습니다: {type(e).__name__}: {e}")

    def _load_project_session(self) -> EditorSession:
        session, warnings = load_session(current_project())
        for w in warnings:
            print(f"[Session] {w}")
        if warnings and hasattr(self, "status"):
            self.status.log(f"세션 경고 {len(warnings)}개 - 콘솔 참고")
        self._session = session
        return session

    def _apply_session_ui(self, session: EditorSession) -> None:
        """엔진 없이 복원할 수 있는 것: Motion Editor의 액션 목록, 에디터 카메라. 씬과 모드는 따로 부른다."""
        project = current_project()
        motion = getattr(self, "motion_editor", None)
        if motion is not None and hasattr(motion, "restore_actions"):
            paths = []
            for value in session.actions:
                path = from_session_path(project, value)
                if path.is_file():
                    paths.append(str(path))
                else:
                    print(f"[Session] 액션 파일이 없어 건너뜁니다: {path}")
            selected = str(from_session_path(project, session.selected_action)) if session.selected_action else ""
            for w in motion.restore_actions(paths, selected):
                print(f"[Session] 액션을 열지 못했습니다: {w}")
        viewport = getattr(self, "viewport", None)
        if session.camera is not None and hasattr(viewport, "set_editor_camera_view"):
            c = session.camera
            viewport.set_editor_camera_view(c.target, c.distance, c.yaw, c.pitch)
        elif hasattr(viewport, "reset_editor_camera"):
            viewport.reset_editor_camera()   # 다른 프로젝트의 카메라 위치가 남지 않게

    def _session_scene_path(self) -> Optional[Path]:
        """세션의 씬 파일(디스크에 있을 때만). 없으면 None - 호출부가 시작 씬을 연다."""
        session = self._session
        if session is None or not session.scene:
            return None
        path = from_session_path(current_project(), session.scene)
        if path.is_file():
            return path
        print(f"[Session] 마지막에 열었던 씬이 없어 시작 씬을 엽니다: {path}")
        return None

    def _apply_session_mode(self) -> None:
        """세션의 모드(Scene / Motion Editor)로 바꾼다. Play는 복원하지 않는다(session.py RESTORABLE_MODES)."""
        session = self._session
        if session is None or self.is_playing or not hasattr(self, "stack"):
            return
        mode = session.mode
        if mode == MODE_MOTION and getattr(self, "motion_editor", None) is None:
            mode = MODE_SCENE
        if self.stack.currentIndex() != mode:
            self._set_mode(mode)

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
