"""
editor/frozen_app.py

완제품 에디터(engine/packaging/, PyInstaller로 exe에 묶은 것)에서만 하는 시작 준비.
개발 실행(`python main.py`, run_editor.bat)에서는 아무것도 하지 않는다.

완제품 폴더 구조(engine/packaging/build_editor_app.py):

    Quarter Flying/
    ├─ Quarter Flying.exe    에디터(Python 코드는 이 안에 바이트코드로 들어 있다)
    ├─ _internal/            python314.dll, Qt·엔진 DLL, .pyd
    ├─ assets/               샘플 프로젝트
    └─ runtime/win-x64/      Player 템플릿(Build Game이 복사해 간다)

에디터의 경로 기준(main.py `_EDITOR_DIR`, project.py `_ENGINE_DIR`, game_builder.py)은 모두
"에디터 코드 폴더의 부모 = 설치 루트"다. exe에서는 `_internal`이 에디터 코드 폴더 자리에 오므로
그 규칙이 그대로 맞는다. 여기서 경로 계산을 따로 고치지 않는 이유다.
"""

import os
import sys
import traceback
from pathlib import Path


def is_frozen():
    return bool(getattr(sys, "frozen", False))


def log_path():
    base = os.environ.get("LOCALAPPDATA") or str(Path.home())
    return Path(base) / "QuarterFlying" / "logs" / "editor.log"


def setup():
    """main.py가 PySide6를 import하기 전에 부른다."""
    if not is_frozen():
        return
    root = Path(sys.executable).resolve().parent

    # run_editor.bat의 `cd /d "%~dp0"`과 같은 역할. config.json은 작업 폴더 기준으로 읽고 쓴다.
    os.chdir(root)

    _redirect_output(log_path())
    _add_player_stdlib(root / "runtime" / "win-x64")
    sys.excepthook = _excepthook


def _redirect_output(path):
    """창 모드 exe에는 콘솔이 없어서 print와 엔진 로그가 사라진다. 둘 다 로그 파일로 보낸다."""
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        f = open(path, "w", encoding="utf-8", buffering=1)
    except OSError:
        return
    # 엔진 DLL(C++ Logger)은 C 런타임의 stdout/stderr(fd 1/2)에 쓴다. sys.stdout만 바꾸면 엔진 로그는
    # 남지 않으므로 fd 자체를 파일로 돌린다(엔진 DLL과 python314.dll은 같은 UCRT를 공유한다).
    os.dup2(f.fileno(), 1)
    os.dup2(f.fileno(), 2)
    sys.stdout = f
    sys.stderr = f


def _add_player_stdlib(template_dir):
    """Play 모드의 게임 스크립트가 Player와 같은 표준 라이브러리를 보게 한다.

    exe에는 에디터 코드가 쓰는 표준 모듈만 들어 있다. 엔진 ScriptSystem이 import하는 게임 스크립트가
    그 밖의 모듈(예: csv, heapq)을 쓰면 에디터에서만 ImportError가 난다. Player 템플릿의 표준 라이브러리
    (runtime/stage_template.py가 고른 것)를 뒤쪽에 붙여서, 에디터와 빌드된 게임이 같은 범위를 쓰게 한다.
    """
    stdlib_zip = template_dir / f"python{sys.version_info.major}{sys.version_info.minor}.zip"
    for p in (stdlib_zip, template_dir / "DLLs"):
        if p.exists() and str(p) not in sys.path:
            sys.path.append(str(p))


_error_shown = False


def _excepthook(exc_type, exc, tb):
    """처리되지 않은 예외: 로그에 남기고, 콘솔이 없으니 처음 한 번은 창으로도 알린다."""
    global _error_shown
    traceback.print_exception(exc_type, exc, tb)
    if _error_shown:
        return
    _error_shown = True
    try:
        import ctypes
        msg = f"오류가 발생했습니다:\n{exc_type.__name__}: {exc}\n\n로그: {log_path()}"
        ctypes.windll.user32.MessageBoxW(None, msg, "Quarter Flying", 0x10)
    except Exception:
        pass
