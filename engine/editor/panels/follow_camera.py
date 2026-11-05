"""One selected object, one follow-camera workflow. No project sidecar state."""
import math

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (QGroupBox, QVBoxLayout, QFormLayout, QLabel,
                               QComboBox, QDoubleSpinBox, QPushButton)

from core.follow_camera import (camera_choices, suggested_camera, camera_offset,
                                component, connect_follow_camera, resized_offset)


class FollowCameraWidget(QGroupBox):
    status = Signal(str)
    play_requested = Signal()

    def __init__(self, registry, editor_api, target_uuid, can_edit, parent=None):
        super().__init__('따라가기 카메라', parent)
        self.registry, self.editor_api = registry, editor_api
        self.target_uuid, self.can_edit = target_uuid, can_edit
        self._read_only = False
        self._offset_edited = False
        outer = QVBoxLayout(self)
        form = QFormLayout()
        form.setFieldGrowthPolicy(QFormLayout.AllNonFixedFieldsGrow)
        self.cameras = QComboBox()
        self.cameras.setMinimumWidth(0)
        self.cameras.setSizeAdjustPolicy(QComboBox.AdjustToMinimumContentsLengthWithIcon)
        self.cameras.setMinimumContentsLength(8)
        form.addRow('사용할 카메라', self.cameras)
        self.distance = QDoubleSpinBox()
        self.distance.setRange(0, 1e9)
        self.distance.setDecimals(3)
        self.distance.setSingleStep(0.5)
        self.distance.setToolTip('대상과의 수평 거리. 기존 카메라의 수평 방향은 유지합니다.')
        self.height = QDoubleSpinBox()
        self.height.setRange(-1e9, 1e9)
        self.height.setDecimals(3)
        self.height.setSingleStep(0.5)
        self.height.setToolTip('대상 위치를 기준으로 한 카메라 높이')
        form.addRow('거리', self.distance)
        form.addRow('높이', self.height)
        outer.addLayout(form)
        self.summary = QLabel()
        self.summary.setWordWrap(True)
        outer.addWidget(self.summary)
        self.apply_button = QPushButton('연결 · 게임 카메라로 사용')
        self.play_button = QPushButton('게임 화면 확인 (Play)')
        outer.addWidget(self.apply_button)
        outer.addWidget(self.play_button)
        self.cameras.currentIndexChanged.connect(self._camera_changed)
        self.distance.valueChanged.connect(lambda: self._edited('distance'))
        self.height.valueChanged.connect(lambda: self._edited('height'))
        self.apply_button.clicked.connect(self._apply)
        self.play_button.clicked.connect(self._play)
        self.reload()

    def reload(self, preferred=None):
        self.choices = camera_choices(self.registry, self.target_uuid)
        self.cameras.blockSignals(True)
        self.cameras.clear()
        self.cameras.addItem('카메라 선택', None)
        names = [c.name for c in self.choices]
        for choice in self.choices:
            label = choice.name
            if names.count(choice.name) > 1:
                # Distinguish equal names without using the label as a reference.
                label += f' (ID {self.registry.GetEntityByUUID(choice.uuid).id})'
            if choice.active:
                label += ' · 게임 카메라'
            if choice.connected:
                label += ' · 연결됨'
            if choice.reason:
                label += ' · 사용 불가'
            # UUID is unsigned 64-bit; Qt's signed integer QVariant overflows
            # for half the IDs and the Inspector then fails to build its form.
            self.cameras.addItem(label, str(choice.uuid))
            item = self.cameras.model().item(self.cameras.count() - 1)
            item.setEnabled(not choice.reason)
            item.setToolTip(choice.reason or label)
        self.cameras.addItem('새 따라가기 카메라 만들기', '0')
        chosen = preferred if preferred is not None else suggested_camera(self.choices)
        index = self.cameras.findData(str(chosen) if chosen is not None else None)
        self.cameras.setCurrentIndex(max(index, 0))
        self.cameras.blockSignals(False)
        self._camera_changed()

    def selected_camera_uuid(self):
        value = self.cameras.currentData()
        return int(value) if value is not None else None

    def _camera_changed(self):
        uuid = self.selected_camera_uuid()
        self._original_offset = camera_offset(self.registry, uuid)
        for box in (self.distance, self.height):
            box.blockSignals(True)
        self.distance.setValue(math.hypot(self._original_offset[0], self._original_offset[2]))
        self.height.setValue(self._original_offset[1])
        for box in (self.distance, self.height):
            box.blockSignals(False)
        self._loaded_fields = {'distance': self.distance.value(), 'height': self.height.value()}
        self._offset_edited = False
        self._edited_fields = set()
        self._update_state()

    def _edited(self, field):
        # Merely touching a field kept Play blocked even after restoring its
        # displayed value. Applying another field then rounded the restored
        # offset too, because the old set tracked edit history, not differences.
        # Compare display values so float32/three-decimal rounding is harmless.
        box = self.distance if field == 'distance' else self.height
        if box.value() == self._loaded_fields[field]:
            self._edited_fields.discard(field)
        else:
            self._edited_fields.add(field)
        self._offset_edited = bool(self._edited_fields)
        self._update_state()

    def _update_state(self):
        uuid = self.selected_camera_uuid()
        choice = next((c for c in self.choices if c.uuid == uuid), None)
        enabled = not self._read_only and self.can_edit()
        self.cameras.setEnabled(enabled)
        self.distance.setEnabled(enabled and uuid is not None)
        self.height.setEnabled(enabled and uuid is not None)
        self.apply_button.setEnabled(enabled and uuid is not None)
        ready = bool(choice and choice.connected and choice.active and not self._offset_edited)
        self.play_button.setEnabled(enabled and ready)
        if uuid is None:
            self.summary.setText('카메라를 선택하거나 새로 만드세요.')
            return
        target = self.registry.GetEntityByUUID(self.target_uuid)
        target_name = self.registry.GetEntityName(target)
        camera_name = choice.name if choice else '새 카메라'
        text = f'{camera_name} → {target_name}\n'
        if ready:
            text += '연결됨. Play에서 따라가는 구도를 확인하세요.\nStop 후에도 연결 전 상태로 Undo할 수 있습니다.'
        else:
            text += '적용하면 이 카메라로 게임 화면을 봅니다. 기존 게임 카메라 선택도 변경됩니다.'
            if choice:
                camera = self.registry.GetEntityByUUID(uuid)
                refs = [component(self.registry, camera, name).get('target', 0)
                        for name in ('CameraFollowComponent', 'CameraLookAtComponent')]
                if any(ref and ref != self.target_uuid for ref in refs):
                    text += '\n현재 다른 대상을 보는 연결이 선택한 대상으로 바뀝니다.'
        if self._offset_edited:
            text += '\n거리·높이 변경은 적용 후 시험할 수 있습니다.'
        self.summary.setText(text)

    def _apply(self):
        if self._read_only or not self.can_edit():
            return
        try:
            offset = None
            if self._offset_edited:
                offset = list(self._original_offset)
                height = self.height.value() if 'height' in self._edited_fields else offset[1]
                if 'distance' in self._edited_fields:
                    offset = resized_offset(offset, self.distance.value(), height)
                else:
                    offset[1] = height
            uuid = connect_follow_camera(self.registry, self.editor_api, self.target_uuid,
                                         self.selected_camera_uuid(), offset)
            self.reload(uuid)
            self.status.emit('따라가기 카메라 적용 완료. Stop 후에도 Undo 한 번으로 이전 설정으로 돌아갑니다.')
        except Exception as error:
            self.summary.setText(str(error))
            self.status.emit(f'따라가기 카메라 적용 실패: {error}')

    def _play(self):
        if self._read_only or not self.can_edit() or self._offset_edited:
            return
        # Re-check the live ECS: a camera could have been deleted or changed by
        # another panel since this widget was drawn. Never preview a stale link.
        try:
            choices = camera_choices(self.registry, self.target_uuid)
            if any(c.uuid == self.selected_camera_uuid() and c.connected and c.active and not c.reason for c in choices):
                self.play_requested.emit()
            else:
                self.reload()
        except Exception as error:
            self.play_button.setEnabled(False)
            self.summary.setText(str(error))

    def set_read_only(self, read_only):
        self._read_only = read_only
        self._update_state()
