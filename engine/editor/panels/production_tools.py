from collections import deque
import json
import math
from pathlib import Path

from PySide6.QtCore import Qt, QTimer
from PySide6.QtWidgets import (QDialog, QVBoxLayout, QFormLayout, QHBoxLayout, QLabel,
    QPushButton, QFileDialog, QMessageBox, QTableWidget, QTableWidgetItem, QLineEdit,
    QDoubleSpinBox, QDialogButtonBox, QAbstractItemView)

from core.project_references import project_index, replace_reference, save_json
from project import current_project


def open_references(window):
    project = current_project()
    dialog = QDialog(window)
    dialog.setWindowTitle("Asset References")
    dialog.resize(920, 470)
    layout = QVBoxLayout(dialog)
    search = QLineEdit()
    search.setPlaceholderText("파일·컴포넌트 검색")
    table = QTableWidget(0, 4)
    table.setHorizontalHeaderLabels(["참조 파일", "항목", "대상", "상태"])
    table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
    table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
    layout.addWidget(search)
    layout.addWidget(table)
    status = QLabel()
    layout.addWidget(status)
    rows = []
    def refresh():
        items, issues = project_index(project)
        rows[:] = items
        table.setRowCount(len(rows))
        for index, row in enumerate(rows):
            values = [str(row.owner.relative_to(project.root)), ".".join(map(str, row.keys)),
                      row.value, "OK" if row.target.is_file() else "누락"]
            for column, value in enumerate(values):
                table.setItem(index, column, QTableWidgetItem(value))
        table.resizeColumnsToContents()
        filter_rows(search.text())
        status.setText(f"{len(rows)}개 참조 · 읽기 오류 {len(issues)}개")
        status.setToolTip("\n".join(issues))
    def filter_rows(text):
        # 표에 보이는 값만 검색한다. 절대 경로까지 넣으면 프로젝트 폴더 이름의 일부로 찾을 때 모든 줄이 걸린다.
        for index, row in enumerate(rows):
            shown = (row.owner.relative_to(project.root).as_posix(), ".".join(map(str, row.keys)), row.value)
            table.setRowHidden(index, text.casefold() not in " ".join(shown).casefold())
    def replace():
        if table.currentRow() < 0 or not window._require_edit_mode("Replace Reference"):
            return
        row = rows[table.currentRow()]
        active_scene = getattr(window, "_scene_path", None)
        reload_scene = active_scene and Path(active_scene).resolve() == row.owner.resolve()
        if reload_scene and window._is_scene_dirty():
            QMessageBox.warning(dialog, "참조 교체", "현재 씬의 편집을 먼저 저장하세요.")
            return
        if row.owner.name.endswith(".action.json") or row.keys == ("base",):
            QMessageBox.information(dialog, "참조 교체", "액션은 Motion Editor에서, 베이스는 새 Variant로 편집하세요.")
            return
        selected, _ = QFileDialog.getOpenFileName(dialog, "새 참조 파일", str(project.assets_dir))
        if not selected: return
        try:
            replace_reference(project, row, Path(selected))
            if reload_scene:
                window._load_scene_file(row.owner)
            if window._engine:
                window._engine.SetAssetRoot(str(project.root))
            refresh()
        except (OSError, ValueError, TypeError, KeyError, IndexError) as error:
            QMessageBox.warning(dialog, "참조 교체 실패", str(error))
    controls = QHBoxLayout()
    for text, action in (("새로 고침", refresh), ("선택한 참조 교체…", replace)):
        button = QPushButton(text)
        button.clicked.connect(action)
        controls.addWidget(button)
    layout.addLayout(controls)
    search.textChanged.connect(filter_rows)
    refresh()
    dialog.exec()


def open_material(window):
    if not window._require_edit_mode("Material Editor"): return
    project = current_project()
    folder = project.assets_dir / "materials"
    dialog = QDialog(window)
    dialog.setWindowTitle("Material Editor")
    layout = QFormLayout(dialog)
    path_label = QLabel("새 머티리얼")
    layout.addRow(path_label)
    fields = {}
    for name, default in (("Red",1.), ("Green",1.), ("Blue",1.), ("Roughness",.5), ("Metallic",0.)):
        spin = QDoubleSpinBox()
        spin.setDecimals(5)
        spin.setRange(0, 1)
        spin.setSingleStep(.05)
        spin.setValue(default)
        fields[name] = spin
        layout.addRow(name, spin)
    texture = QLineEdit()
    texture.setPlaceholderText("assets/textures/…")
    layout.addRow("Texture", texture)
    path = original = None
    document = {"format":"quarterflying.material","version":1}
    def load():
        nonlocal path, original, document
        chosen, _ = QFileDialog.getOpenFileName(dialog, "머티리얼 열기", str(folder), "Material (*.material.json)")
        if not chosen: return
        try:
            selected = Path(chosen).resolve()
            selected.relative_to(project.root.resolve())
            source = selected.read_bytes()
            data = json.loads(source.decode("utf-8-sig"))
            if data.get("format") != "quarterflying.material" or data.get("version") != 1:
                raise ValueError("지원하지 않는 머티리얼 형식입니다.")
            values = [*data.get("baseColor",[1,1,1]), data.get("roughness",.5), data.get("metallic",0)]
            if len(values) != 5 or not all(math.isfinite(value) and 0 <= value <= 1 for value in values):
                raise ValueError("지원 범위 밖의 색상 또는 물성 값입니다.")
            for spin, value in zip(fields.values(), values):
                spin.setValue(value)
            texture.setText(data.get("texture",""))
            path, original, document = selected, source, data
            path_label.setText(str(path.relative_to(project.root)))
        except (OSError, ValueError, TypeError, AttributeError) as error:
            QMessageBox.warning(dialog, "열기 실패", str(error))
    def save():
        destination = path
        if destination is None:
            chosen, _ = QFileDialog.getSaveFileName(dialog, "새 머티리얼", str(folder/"New.material.json"), "Material (*.material.json)")
            if not chosen: return
            destination = Path(chosen).resolve()
        try:
            destination.relative_to(project.root.resolve())
            if not destination.name.endswith(".material.json"): raise ValueError(".material.json 파일명을 사용하세요.")
            image = texture.text().strip()
            if image:
                resolved = (project.root / image).resolve()
                image = resolved.relative_to(project.root.resolve()).as_posix()
                if not resolved.is_file(): raise ValueError("텍스처 파일이 없습니다.")
            values = [spin.value() for spin in fields.values()]
            updated = {**document, "baseColor":values[:3], "roughness":values[3], "metallic":values[4], "texture":image}
            # 새 머티리얼을 기존 파일 이름으로 저장: 저장 대화상자에서 덮어쓰기를 이미 승인했다. 예전에는 original이 None이라
            # 새 파일 전용 경로(os.link)로 가서 "[WinError 183] 파일이 이미 있으므로 만들 수 없습니다"로 실패했다.
            current = destination.read_bytes() if path is None and destination.exists() else original
            save_json(destination, updated, current)
            if window._engine: window._engine.SetAssetRoot(str(project.root))
            dialog.accept()
        except (OSError, ValueError) as error:
            QMessageBox.warning(dialog, "저장 실패", str(error))
    controls = QDialogButtonBox(QDialogButtonBox.StandardButton.Save | QDialogButtonBox.StandardButton.Cancel)
    controls.addButton("기존 파일 열기…", QDialogButtonBox.ButtonRole.ActionRole).clicked.connect(load)
    controls.accepted.connect(save)
    controls.rejected.connect(dialog.reject)
    layout.addRow(controls)
    dialog.exec()


def open_profiler(window):
    current = getattr(window, "_profiler", None)
    if current is not None:
        current.raise_()
        return
    viewport = window.viewport
    if not getattr(viewport, "initialized", False):
        QMessageBox.information(window, "Frame Profiler", "엔진 뷰포트가 필요합니다.")
        return
    dialog = QDialog(window)
    window._profiler = dialog
    dialog.setWindowTitle("Frame Profiler")
    dialog.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
    layout = QVBoxLayout(dialog)
    layout.addWidget(QLabel("최근 240프레임 · 메인 스레드 경과 시간 (ms)"))
    table = QTableWidget(4, 4)
    table.setHorizontalHeaderLabels(["구간", "평균", "P95", "최대"])
    table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
    layout.addWidget(table)
    frames = deque(maxlen=240)
    def sample(costs):
        frames.append((*costs, sum(costs)))
    def refresh():
        if not frames: return
        for index, name in enumerate(("월드 업데이트", "렌더 제출", "Present", "세 구간 합계")):
            times = sorted(row[index] for row in frames)
            for column, value in enumerate((name, sum(times)/len(times), times[math.ceil(.95*len(times))-1], times[-1])):
                table.setItem(index, column, QTableWidgetItem(value if isinstance(value,str) else f"{value:.2f}"))
    timer = QTimer(dialog)
    timer.timeout.connect(refresh)
    timer.start(250)
    viewport.frame_profile.connect(sample)
    viewport.profile_enabled = True
    def stop(*_args):
        viewport.profile_enabled = False
        viewport.frame_profile.disconnect(sample)
        window._profiler = None
    dialog.finished.connect(stop)
    dialog.resize(510, 240)
    dialog.show()
