import json
import tempfile
import unittest
from pathlib import Path
from core.project_references import project_index, replace_reference
from project import create_project


class ProjectReferencesTest(unittest.TestCase):
    def test_replace_checks_source_and_updates_only_selected_field(self):
        with tempfile.TemporaryDirectory() as directory:
            project = create_project(Path(directory) / "project")
            scene = project.start_scene_path
            doc = json.loads(scene.read_text(encoding="utf8"))
            doc["entities"][0]["components"]["UIImageComponent"] = {"imagePath":"assets/missing.png"}
            scene.write_text(json.dumps(doc), encoding="utf8")
            rows, errors = project_index(project)
            self.assertFalse(errors)
            row = next(row for row in rows if row.keys[-1] == "imagePath")
            self.assertFalse(row.target.exists())
            image = project.assets_dir / "textures/actual.png"
            image.touch()
            replace_reference(project, row, image)
            with self.assertRaises(ValueError):
                replace_reference(project, row, image)
            rows, errors = project_index(project)
            self.assertFalse(errors)
            self.assertEqual(next(row.target for row in rows if row.keys[-1] == "imagePath"), image)


if __name__ == "__main__":
    unittest.main()
