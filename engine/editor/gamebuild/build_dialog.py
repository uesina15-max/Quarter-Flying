"""
editor/gamebuild/build_dialog.py

Build Game의 에디터 GUI. docs/GAME_BUILD_PLAN.md Phase 4.

**Build Game 기능에서 PySide6를 쓰는 곳은 이 파일뿐이다.** 실제 빌드는 Qt를 모르는 game_builder.py가
하고, 여기서는 설정 편집(Build Settings)과 진행 표시만 한다. 빌드는 QThread에서 돌려 에디터 창이
멈추지 않게 한다.
"""

from __future__ import annotations

import os

from PySide6.QtCore import QEventLoop, QObject, Qt, QThread, Signal, Slot
from PySide6.QtWidgets import (QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFileDialog, QFormLayout,
                               QHBoxLayout, QLineEdit, QMessageBox, QProgressDialog,
                               QPushButton, QSpinBox, QVBoxLayout, QWidget)

from gamebuild.game_builder import BuildCancelled, BuildError, BuildSettings, build_game, sanitize_name
from project import Project, ProjectError


class BuildSettingsDialog(QDialog):
    """project.json의 "build" 절을 편집한다."""

    def __init__(self, project: Project, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Build Settings")
        self._project = project
        b = project.build

        self.ed_name = QLineEdit(b.name or project.name)
        self.cb_scene = QComboBox()
        scenes = project.list_scenes()
        current = b.start_scene or project.start_scene
        if current and current not in scenes:
            scenes.insert(0, current)
        self.cb_scene.addItems(scenes)
        if current:
            self.cb_scene.setCurrentText(current)
        self.sp_width = QSpinBox(); self.sp_width.setRange(320, 7680); self.sp_width.setValue(b.window_width)
        self.sp_height = QSpinBox(); self.sp_height.setRange(240, 4320); self.sp_height.setValue(b.window_height)
        self.chk_fullscreen = QCheckBox("전체 화면"); self.chk_fullscreen.setChecked(b.fullscreen)
        self.ed_include = QLineEdit(", ".join(b.include_dirs))
        self.ed_include.setPlaceholderText("예: assets/prefabs  (스크립트가 실행 중에 직접 여는 파일이 있는 폴더)")
        self.ed_output = QLineEdit(b.output_dir)
        self.ed_output.setPlaceholderText(str(project.root / "Build" / sanitize_name(b.name or project.name)))
        btn_browse = QPushButton("…")
        btn_browse.clicked.connect(self._browse_output)
        out_row = QWidget(); out_layout = QHBoxLayout(out_row); out_layout.setContentsMargins(0, 0, 0, 0)
        out_layout.addWidget(self.ed_output); out_layout.addWidget(btn_browse)
        size_row = QWidget(); size_layout = QHBoxLayout(size_row); size_layout.setContentsMargins(0, 0, 0, 0)
        size_layout.addWidget(self.sp_width); size_layout.addWidget(self.sp_height); size_layout.addWidget(self.chk_fullscreen)

        form = QFormLayout()
        form.addRow("게임 이름", self.ed_name)
        form.addRow("시작 씬", self.cb_scene)
        form.addRow("창 크기", size_row)
        form.addRow("포함 폴더", self.ed_include)
        form.addRow("출력 폴더", out_row)

        buttons = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self._save)
        buttons.rejected.connect(self.reject)
        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(buttons)
        self.resize(560, self.sizeHint().height())

    def _browse_output(self):
        path = QFileDialog.getExistingDirectory(self, "출력 폴더", str(self._project.root))
        if path:
            self.ed_output.setText(path)

    def _save(self):
        b = self._project.build
        b.name = self.ed_name.text().strip()
        b.start_scene = self.cb_scene.currentText().strip()
        b.window_width = self.sp_width.value()
        b.window_height = self.sp_height.value()
        b.fullscreen = self.chk_fullscreen.isChecked()
        b.include_dirs = [d.strip() for d in self.ed_include.text().split(",") if d.strip()]
        b.output_dir = self.ed_output.text().strip()
        try:
            self._project.save()
        except (ProjectError, OSError) as e:
            QMessageBox.critical(self, "Build Settings", str(e))
            return
        self.accept()


class _BuildWorker(QObject):
    progress = Signal(str, float)
    finished = Signal(object, str)        # (BuildReport | None, 에러 메시지)

    def __init__(self, settings: BuildSettings):
        super().__init__()
        self._settings = settings
        self._cancel = False

    def cancel(self):
        self._cancel = True

    def run(self):
        try:
            report = build_game(self._settings, progress=self.progress.emit, is_cancelled=lambda: self._cancel)
            self.finished.emit(report, "")
        except BuildCancelled as e:
            self.finished.emit(None, str(e))
        except BuildError as e:
            self.finished.emit(None, str(e))
        except Exception as e:  # 예상 못 한 예외도 대화상자로 보여 준다(조용히 사라지지 않게)
            self.finished.emit(None, f"{type(e).__name__}: {e}")


class _BuildController(QObject):
    """빌드 진행/완료 시그널을 GUI 스레드에서 받는 수신자.

    증상: Build Game이 진행 대화상자를 띄운 채 영원히 멈췄다(자동화 검증에서 180초 타임아웃).
    원인: 예전에는 진행·완료 시그널을 지역 함수에 연결했다. QObject가 아닌 수신자는 시그널을 보낸
    스레드(작업자 스레드)에서 바로 실행되므로, 완료 처리 안의 thread.wait()가 자기 자신을 기다려
    교착했고, 진행 대화상자(GUI 객체)도 작업자 스레드에서 건드리고 있었다. 이 객체는 GUI 스레드에서
    만들어지므로 작업자 스레드의 시그널이 큐로 넘어와 GUI 스레드에서 처리된다.
    """

    def __init__(self, dialog: QProgressDialog, thread: QThread, status_parent):
        super().__init__(dialog)
        self._dialog = dialog
        self._thread = thread
        self._status_parent = status_parent
        self.done = False
        self.report = None
        self.error = ""
        self.loop = QEventLoop(self)

    @Slot(str, float)
    def on_progress(self, stage: str, fraction: float):
        self._dialog.setLabelText(stage)
        self._dialog.setValue(int(fraction * 1000))
        if self._status_parent is not None and hasattr(self._status_parent, "status"):
            self._status_parent.status.log(f"[Build] {stage}")

    @Slot(object, str)
    def on_finished(self, report, error: str):
        self._thread.quit()
        self._thread.wait()
        self.report, self.error, self.done = report, error, True
        self._dialog.close()
        self.loop.quit()


def run_build_with_dialog(project: Project, parent=None) -> None:
    settings = BuildSettings.from_project(project)
    if not settings.start_scene:
        QMessageBox.warning(parent, "Build Game", "시작 씬이 없습니다. 씬을 저장하거나 Build Settings에서 고르세요.")
        return

    dlg = QProgressDialog("빌드 준비 중…", "취소", 0, 1000, parent)
    dlg.setWindowTitle("Build Game")
    dlg.setWindowModality(Qt.WindowModal)
    dlg.setMinimumDuration(0)
    dlg.setAutoClose(False)
    dlg.setAutoReset(False)

    thread = QThread(parent)
    worker = _BuildWorker(settings)
    worker.moveToThread(thread)
    controller = _BuildController(dlg, thread, parent)
    thread.started.connect(worker.run)
    # 취소 플래그는 GUI 스레드에서 직접 세운다. worker.cancel을 그냥 연결하면 작업자 스레드의 큐로 가는데,
    # 그 스레드는 빌드(run)가 끝날 때까지 큐를 처리하지 않으므로 취소가 빌드가 끝난 뒤에야 도착했다.
    dlg.canceled.connect(worker.cancel, Qt.DirectConnection)
    worker.progress.connect(controller.on_progress)
    worker.finished.connect(controller.on_finished)
    thread.start()
    dlg.exec()
    if not controller.done:
        # 취소 버튼을 누르면 대화상자는 바로 닫힌다. 빌드가 취소 지점에 닿아 정리를 끝낼 때까지 기다린다.
        controller.loop.exec()

    if controller.report is None:
        error = controller.error
        box = QMessageBox(QMessageBox.Critical, "Build Game", "빌드하지 못했습니다.", parent=parent)
        box.setInformativeText(error.splitlines()[0] if error else "")
        box.setDetailedText(error)
        box.exec()
        return
    _show_report(controller.report, parent)


def _show_report(report, parent) -> None:
    text = (f"빌드 완료: {report.exe_path}\n"
            f"파일 {report.file_count}개, {report.total_bytes / 1e6:.1f} MB"
            + (f"\nzip: {report.zip_path}" if report.zip_path else ""))
    box = QMessageBox(QMessageBox.Information, "Build Game", text, parent=parent)
    if report.warnings:
        box.setIcon(QMessageBox.Warning)
        box.setInformativeText(f"경고 {len(report.warnings)}건 — 자세히 보기를 누르세요.")
    details = []
    if report.warnings:
        details.append("경고:\n  " + "\n  ".join(report.warnings))
    details.append("복사한 게임 파일:\n  " + "\n  ".join(report.content_files))
    box.setDetailedText("\n\n".join(details))
    open_btn = box.addButton("폴더 열기", QMessageBox.ActionRole)
    box.addButton(QMessageBox.Ok)
    box.exec()
    if box.clickedButton() is open_btn:
        os.startfile(str(report.output_dir))  # noqa: S606 (Windows 전용 에디터)
