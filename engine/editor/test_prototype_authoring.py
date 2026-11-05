"""Real engine + Qt regression tests; requires the Release module on PYTHONPATH.

Run on Windows: python -B test_prototype_authoring.py
Creates an unshown native widget for the engine context; does not run the editor.
"""
import json
import tempfile
import unittest

from PySide6.QtWidgets import QApplication, QWidget
from engine_binding import binding as engine, HAS_ENGINE
from core.follow_camera import component, connect_follow_camera
from core.keyboard_movement import COMPONENT, connect_keyboard, disconnect_keyboard, set_speed
from panels.inspector import InspectorPanel


@unittest.skipUnless(HAS_ENGINE, 'Release quarterflying module required')
class PrototypeAuthoringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        cls.host = QWidget()
        cls.host.resize(320, 240)
        cls.temp = tempfile.TemporaryDirectory(prefix='qf-authoring-')
        cls.engine = engine.Engine()
        config = engine.EngineConfig()
        config.assetRoot = cls.temp.name
        config.logFrameInterval = 0
        cls.engine.InitializeFromWindowHandle(int(cls.host.winId()), config)
        cls.world = cls.engine.CreateWorld('Authoring regression')
        cls.engine.SetActiveWorld(cls.world)
        cls.registry = cls.world.GetRegistry()
        cls.api = engine.EditorAPI.get_instance()
        cls.api.set_registry(cls.registry)
        cls.commands = engine.CommandManager.get_instance()

    @classmethod
    def tearDownClass(cls):
        cls.world.Stop()
        cls.commands.clear()
        cls.api.set_registry(None)
        cls.engine.Shutdown()
        cls.host.close()
        cls.temp.cleanup()

    def setUp(self):
        self.world.NewScene()
        self.target = self.api.create_entity('Object')
        self.api.add_component(self.target, 'TransformComponent')
        self.uuid = self.registry.GetEntityUUID(self.target)
        self.camera_uuid = connect_follow_camera(self.registry, self.api, self.uuid, 0)
        connect_keyboard(self.registry, self.api, self.uuid)
        self.panel = InspectorPanel()
        self.panel.connect_registry(self.registry)
        self.panel.connect_editor(self.api)
        self.panel.on_entity_selected(self.target.id)
        self.movement = self.panel.keyboard_movement
        self.follow = self.panel.follow_camera
        self.assertIsNotNone(self.movement)
        self.assertIsNotNone(self.follow)
        self.commands.clear()

    def tearDown(self):
        self.world.Stop()
        self.panel.close()
        self.panel.deleteLater()
        self.app.sendPostedEvents()

    def data(self, uuid, name):
        return component(self.registry, self.registry.GetEntityByUUID(uuid), name)

    def precise_offset(self):
        entity = self.registry.GetEntityByUUID(self.camera_uuid)
        name = 'CameraFollowComponent'
        version = json.loads(self.registry.GetComponentSchema(name))['version']
        self.api.set_component_field(entity, name, 'offset', json.dumps([4.123456, 6.987654, 8.123456]), version)
        self.follow.reload(self.camera_uuid)
        self.commands.clear()
        return self.data(self.camera_uuid, name)['offset']

    def test_disconnect_discards_unapplied_speed_and_unblocks_toolbar_play(self):
        self.movement.speed.setValue(12)
        self.assertTrue(self.panel.pending_authoring_message())
        self.movement.remove_button.click()
        self.assertFalse(self.data(self.uuid, COMPONENT)['enabled'])
        self.assertEqual(self.panel.pending_authoring_message(), '')
        self.assertEqual(self.movement.speed.value(), 4)
        self.assertEqual(self.movement.speed_apply.text(), '속도 적용')
        self.assertFalse(self.movement.speed_apply.isEnabled())

    def test_external_disconnect_reload_discards_draft(self):
        self.movement.speed.setValue(12)
        disconnect_keyboard(self.registry, self.api, self.uuid)
        self.movement.reload()
        self.assertFalse(self.movement._speed_dirty)
        self.assertEqual(self.movement.speed.value(), 4)

    def test_disconnect_undo_redo_preserves_saved_speed(self):
        set_speed(self.registry, self.api, self.uuid, 7.5)
        self.commands.clear()
        self.movement.reload()
        self.movement.speed.setValue(12)
        self.movement.remove_button.click()
        self.commands.undo()
        self.movement.reload()
        self.assertTrue(self.data(self.uuid, COMPONENT)['enabled'])
        self.assertEqual(self.movement.speed.value(), 7.5)
        self.assertTrue(self.movement.play_button.isEnabled())
        self.commands.redo()
        self.movement.reload()
        self.assertFalse(self.data(self.uuid, COMPONENT)['enabled'])
        self.assertEqual(self.data(self.uuid, COMPONENT)['speed'], 7.5)

    def test_connected_reload_preserves_draft(self):
        self.movement.speed.setValue(12)
        self.movement.reload()
        self.assertTrue(self.movement._speed_dirty)
        self.assertEqual(self.movement.speed.value(), 12)
        self.assertEqual(self.data(self.uuid, COMPONENT)['speed'], 4)
        self.assertFalse(self.movement.play_button.isEnabled())

    def test_read_only_does_not_apply_or_discard_draft(self):
        self.movement.speed.setValue(12)
        self.panel.set_play_mode(True)
        self.movement._remove()
        self.movement._apply_speed()
        self.assertTrue(self.movement._speed_dirty)
        self.assertTrue(self.data(self.uuid, COMPONENT)['enabled'])
        self.assertEqual(self.data(self.uuid, COMPONENT)['speed'], 4)
        self.panel.set_play_mode(False)
        self.assertTrue(self.movement.speed_apply.isEnabled())

    def test_float32_speed_restored_to_display_value_allows_play(self):
        set_speed(self.registry, self.api, self.uuid, 0.1)
        self.movement.reload()
        self.movement.speed.setValue(0.2)
        self.movement.speed.setValue(0.1)
        self.assertFalse(self.movement._speed_dirty)
        self.assertTrue(self.movement.play_button.isEnabled())

    def test_camera_restored_values_unblock_all_play_controls(self):
        self.precise_offset()
        distance, height = self.follow.distance.value(), self.follow.height.value()
        self.follow.distance.setValue(distance + 1)
        self.follow.height.setValue(height + 1)
        self.assertTrue(self.panel.pending_authoring_message())
        self.follow.distance.setValue(distance)
        self.assertTrue(self.follow._offset_edited)
        self.follow.height.setValue(height)
        self.assertEqual(self.panel.pending_authoring_message(), '')
        self.assertTrue(self.follow.play_button.isEnabled())
        played = []
        self.follow.play_requested.connect(lambda: played.append(True))
        self.follow.play_button.click()
        self.assertEqual(played, [True])

    def test_restored_distance_does_not_round_horizontal_offset(self):
        original = self.precise_offset()
        distance = self.follow.distance.value()
        self.follow.distance.setValue(distance + 1)
        self.follow.distance.setValue(distance)
        self.follow.height.setValue(5)
        self.follow.apply_button.click()
        actual = self.data(self.camera_uuid, 'CameraFollowComponent')['offset']
        self.assertEqual(actual, [original[0], 5, original[2]])
        self.commands.undo()
        self.assertEqual(self.data(self.camera_uuid, 'CameraFollowComponent')['offset'], original)
        self.commands.redo()
        self.assertEqual(self.data(self.camera_uuid, 'CameraFollowComponent')['offset'], actual)

    def test_restored_height_does_not_round_vertical_offset(self):
        original = self.precise_offset()
        height = self.follow.height.value()
        self.follow.height.setValue(height + 1)
        self.follow.height.setValue(height)
        self.follow.distance.setValue(12)
        self.follow.apply_button.click()
        self.assertEqual(self.data(self.camera_uuid, 'CameraFollowComponent')['offset'][1], original[1])

    def test_restored_camera_fields_do_not_create_undo_entry(self):
        original = self.precise_offset()
        before = self.commands.get_change_count()
        height = self.follow.height.value()
        self.follow.height.setValue(height + 1)
        self.follow.height.setValue(height)
        self.follow.apply_button.click()
        self.assertEqual(self.data(self.camera_uuid, 'CameraFollowComponent')['offset'], original)
        self.assertEqual(self.commands.get_change_count(), before)
        self.assertFalse(self.commands.can_undo())


if __name__ == '__main__':
    unittest.main(verbosity=2)
