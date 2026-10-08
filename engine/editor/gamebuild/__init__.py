"""Build Game (docs/GAME_BUILD_PLAN.md Phase 4).

game_builder.py는 PySide6를 import하지 않는다(D3). 에디터 GUI는 build_dialog.py에만 있다.
(패키지 이름이 build가 아닌 이유: 저장소 .gitignore의 `build/` 규칙에 걸려 소스가 무시되고,
PyPI의 `build` 패키지 이름과도 겹치기 때문이다.)
"""
