"""프로젝트 폴더 (docs/GAME_BUILD_PLAN.md D4). PySide6를 import하지 않는다."""

from project.project import (  # noqa: F401
    BuildConfig,
    PROJECT_FILE,
    Project,
    ProjectError,
    SCENE_SUFFIX,
    create_project,
    default_scene_document,
    current_project,
    empty_scene_document,
    open_project,
    sample_project,
    set_current_project,
)
