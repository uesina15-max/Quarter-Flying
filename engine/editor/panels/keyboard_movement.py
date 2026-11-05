from PySide6.QtCore import Signal
from PySide6.QtWidgets import QGroupBox,QVBoxLayout,QHBoxLayout,QLabel,QPushButton,QDoubleSpinBox,QFormLayout
from engine_binding import binding as engine
from core.keyboard_movement import COMPONENT,eligibility,connect_keyboard,disconnect_keyboard,set_speed
from core.follow_camera import component

class KeyboardMovementWidget(QGroupBox):
    status=Signal(str)
    play_requested=Signal()
    def __init__(self,registry,api,uuid,can_edit,parent=None):
        super().__init__('키보드로 움직이기',parent)
        self.registry,self.api,self.uuid,self.can_edit=registry,api,uuid,can_edit
        self._read_only=False
        self._speed_dirty=False
        self._loaded_speed=4.0
        self._preview_ready=False
        layout=QVBoxLayout(self)
        self.summary=QLabel();self.summary.setWordWrap(True);layout.addWidget(self.summary)
        self.apply_button=QPushButton('이 객체를 키보드로 조작')
        self.remove_button=QPushButton('연결 해제')
        row=QHBoxLayout();row.addWidget(self.apply_button);row.addWidget(self.remove_button);layout.addLayout(row)
        self.speed=QDoubleSpinBox();self.speed.setRange(0,1000);self.speed.setDecimals(3);self.speed.setSingleStep(0.5)
        self.speed.setToolTip('1초 동안 이동하는 월드 거리. 0이면 움직이지 않습니다.')
        form=QFormLayout();form.setFieldGrowthPolicy(QFormLayout.AllNonFixedFieldsGrow)
        form.addRow('이동 속도 (단위/초)',self.speed);layout.addLayout(form)
        self.speed_apply=QPushButton('속도 적용');layout.addWidget(self.speed_apply)
        self.speed.valueChanged.connect(self._speed_edited);self.speed_apply.clicked.connect(self._apply_speed)
        self.play_button=QPushButton('움직임 시험 (Play)');layout.addWidget(self.play_button)
        self.apply_button.clicked.connect(self._apply)
        self.remove_button.clicked.connect(self._remove)
        self.play_button.clicked.connect(self._play)
        self.reload()
    def reload(self):
        entity=self.registry.GetEntityByUUID(self.uuid)
        reason=eligibility(self.registry,entity)
        connected=bool(component(self.registry,entity,COMPONENT).get('enabled',False)) if entity.IsValid() else False
        editable=not self._read_only and self.can_edit()
        camera=engine.SelectActiveCamera(self.registry).entity.IsValid()
        self.apply_button.setEnabled(editable and not reason and not connected)
        self.remove_button.setEnabled(editable and connected)
        self._preview_ready=editable and connected and not reason and camera
        self.speed.setEnabled(editable and connected and not reason)
        # Disconnecting left an unapplied draft behind while disabling its Apply
        # button, so Inspector's pending-edit guard blocked even toolbar Play.
        # A disconnected control has no editable draft; restore its saved speed.
        if not connected:
            self._speed_dirty=False
        if not self._speed_dirty:
            value=component(self.registry,entity,COMPONENT).get('speed',4.0) if entity.IsValid() else 4.0
            self.speed.blockSignals(True);self.speed.setValue(value);self.speed.blockSignals(False)
            # Compare edits with the displayed value. speed is float32, so 0.1 comes back as 0.10000000149...;
            # comparing with that raw value marked "0.2 -> 0.1" as an unapplied change and disabled Play.
            self._loaded_speed=self.speed.value()
        self._speed_edited()
        if reason: text=reason
        else:
            text=('연결됨. ' if connected else '연결하면 이전 조작 대상은 해제됩니다. ')
            text+='WASD·방향키로 카메라 기준 수평 이동.\n충돌·점프 없이 이동 감각을 시험합니다.'
            if not camera: text+='\n아래에서 게임 카메라를 연결하면 바로 시험할 수 있습니다.'
        self.summary.setText(text)
    def _speed_edited(self):
        self._speed_dirty=self.speed.value()!=self._loaded_speed
        self.speed_apply.setEnabled(self.speed.isEnabled() and self._speed_dirty)
        self.play_button.setEnabled(self._preview_ready and not self._speed_dirty)
        self.speed_apply.setText('속도 적용 · 미적용 변경' if self._speed_dirty else '속도 적용')
    def _apply_speed(self):
        if self._read_only or not self.can_edit(): return
        try:
            set_speed(self.registry,self.api,self.uuid,self.speed.value())
            self._speed_dirty=False;self.reload();self.speed_apply.setText('속도 적용')
            self.status.emit('이동 속도 적용 완료. Play로 시험하거나 Undo로 되돌릴 수 있습니다. 씬을 저장하면 유지됩니다.')
        except Exception as error:
            self.summary.setText(str(error));self.status.emit(f'이동 속도 적용 실패: {error}')
    def _apply(self):
        if self._read_only or not self.can_edit(): return
        try:
            connect_keyboard(self.registry,self.api,self.uuid);self.reload()
            self.status.emit('키보드 조작 연결 완료. Play 후 게임 화면을 클릭하고 WASD·방향키를 누르세요.')
        except Exception as error:
            self.summary.setText(str(error));self.status.emit(f'키보드 조작 연결 실패: {error}')
    def _remove(self):
        if self._read_only or not self.can_edit(): return
        try:
            had_draft=self._speed_dirty
            disconnect_keyboard(self.registry,self.api,self.uuid);self.reload()
            text='키보드 조작 연결을 해제했습니다.'
            if had_draft: text+=' 미적용 속도 변경은 취소했습니다.'
            self.status.emit(text)
        except Exception as error: self.summary.setText(str(error))
    def _play(self):
        self.reload()
        if self.play_button.isEnabled(): self.play_requested.emit()
    def set_read_only(self,value):
        self._read_only=value;self.reload()
