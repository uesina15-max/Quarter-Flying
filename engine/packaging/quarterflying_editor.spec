# -*- mode: python ; coding: utf-8 -*-
# 완제품 에디터의 PyInstaller 설정. 직접 실행하지 말고 build_editor_app.py(CMake editor_app 타깃)를 거친다.
#     pyinstaller quarterflying_editor.spec -- --stage <cmake --install 결과 폴더> [--icon <ico>]
import argparse
from pathlib import Path

_parser = argparse.ArgumentParser()
_parser.add_argument("--stage", required=True)
_parser.add_argument("--icon")
_opts = _parser.parse_args()

stage = Path(_opts.stage).resolve()
editor_dir = stage / "editor"
bin_dir = stage / "bin"          # quarterflying.pyd, quarterflying_engine.dll, glew32.dll, VC 런타임

a = Analysis(
    [str(editor_dir / "main.py")],
    pathex=[str(editor_dir), str(bin_dir)],
    binaries=[],
    datas=[],
    # engine_binding.py가 try 안에서 import한다. 정적 분석이 놓쳐도 반드시 들어가게 명시한다.
    hiddenimports=["quarterflying"],
    hookspath=[],
    runtime_hooks=[],
    # 다른 Qt 바인딩이 같은 환경에 있어도 섞이지 않게 막는다(전용 venv라 보통은 없다).
    excludes=["tkinter", "PyQt5", "PyQt6", "PySide2", "test", "idlelib"],
    noarchive=False,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="Quarter Flying",
    console=False,                # 콘솔 창 없음. 출력은 frozen_app.py가 로그 파일로 보낸다.
    icon=_opts.icon,
    upx=False,
)
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    name="Quarter Flying",
    upx=False,
)
