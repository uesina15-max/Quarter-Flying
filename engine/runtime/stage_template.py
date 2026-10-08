"""
runtime/stage_template.py

Player 템플릿(runtime/win-x64)을 조립한다. docs/GAME_BUILD_PLAN.md Phase 3.
CMake의 `player_template` 타깃이 빌드 직후 실행한다(도구 개발자 환경 ①에서만 돈다).

템플릿 = Build Game이 그대로 복사해 가는 원본 폴더:

    win-x64/
    ├─ quarterflying_player.exe        (Build Game이 <게임 이름>.exe로 바꾼다)
    ├─ quarterflying_engine.dll, quarterflying.<abi>.pyd
    ├─ VC 런타임 DLL
    ├─ python314.dll, python3.dll, python314.zip, DLLs/, python314._pth   ← Player 전용 Python 런타임
    ├─ python/                        runtime_bootstrap.py, core/ (Qt 없는 런타임 Python)
    └─ runtime_manifest.json          템플릿/엔진 버전, Python ABI, 파일별 SHA-256

Python 런타임을 템플릿에 넣는 이유(§6 미해결 1의 결론): 에디터 완제품이 Python을 어떤 방식으로
포함하든(폴더 동봉/실행 파일로 묶기) Build Game이 영향을 받지 않게 하기 위해서다. 표준 라이브러리는
빌드에 쓰인 Python에서 복사하므로 .pyd와 ABI가 항상 일치한다.

**PySide6/site-packages는 절대 넣지 않는다**(설계 제약 C4). 마지막에 검사한다.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
import zipfile
from pathlib import Path

TEMPLATE_FORMAT = "quarterflying.runtime-template"
TEMPLATE_VERSION = 1
MANIFEST = "runtime_manifest.json"

# 표준 라이브러리에서 뺄 것: 게임 런타임에 필요 없고 크기만 큰 것, GUI(Tk), 개발 도구.
STDLIB_EXCLUDE_DIRS = {
    "test", "idlelib", "tkinter", "turtledemo", "ensurepip", "venv", "pydoc_data",
    "site-packages", "__pycache__", "lib2to3", "__phello__",
}
STDLIB_EXCLUDE_FILES = {"turtle.py"}
DLLS_EXCLUDE_PREFIXES = ("_tkinter", "tcl", "tk", "_test", "xxlimited", "_ctypes_test", "_testcapi")

# 템플릿에 있으면 안 되는 이름(설계 제약 C4). Build Game도 출력 폴더에서 같은 검사를 한다.
FORBIDDEN_NAMES = ("PySide6", "shiboken6", "Qt6")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def copy(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def build_stdlib_zip(lib_dir: Path, out_zip: Path) -> int:
    count = 0
    tmp = out_zip.with_suffix(".zip.tmp")
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as z:
        for root, dirs, files in os.walk(lib_dir):
            rel_root = Path(root).relative_to(lib_dir)
            dirs[:] = sorted(d for d in dirs if d not in STDLIB_EXCLUDE_DIRS)
            for name in sorted(files):
                if not name.endswith(".py") or name in STDLIB_EXCLUDE_FILES:
                    continue
                z.write(Path(root) / name, (rel_root / name).as_posix())
                count += 1
    os.replace(tmp, out_zip)
    return count


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="템플릿 폴더 (예: build/runtime/win-x64)")
    ap.add_argument("--bin", required=True, help="quarterflying_player.exe 등이 있는 빌드 폴더")
    ap.add_argument("--python-home", required=True, help="빌드에 쓰인 Python 설치 폴더")
    ap.add_argument("--python-dll", required=True, help="예: python314")
    ap.add_argument("--pyd", required=True, help="quarterflying.<abi>.pyd 파일 이름")
    ap.add_argument("--engine-version", required=True)
    ap.add_argument("--runtime-python", nargs="*", default=[], help="python/에 넣을 파일")
    ap.add_argument("--runtime-python-core", nargs="*", default=[], help="python/core/에 넣을 파일")
    ap.add_argument("--extra-dll", nargs="*", default=[], help="VC 런타임 등 함께 넣을 DLL 경로")
    args = ap.parse_args()

    out = Path(args.out)
    bin_dir = Path(args.bin)
    home = Path(args.python_home)

    # 이전 템플릿을 지우고 새로 만든다(남은 파일이 매니페스트에 섞이지 않게).
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    for name in ("quarterflying_player.exe", "quarterflying_engine.dll", args.pyd):
        src = bin_dir / name
        if not src.is_file():
            print(f"[stage_template] missing build output: {src}", file=sys.stderr)
            return 1
        copy(src, out / name)
    for dll in args.extra_dll:
        p = Path(dll)
        if p.is_file():
            copy(p, out / p.name)

    # Python 런타임
    for dll in (f"{args.python_dll}.dll", "python3.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
        src = home / dll
        if src.is_file() and not (out / dll).exists():
            copy(src, out / dll)
    if not (out / f"{args.python_dll}.dll").is_file():
        print(f"[stage_template] {args.python_dll}.dll not found under {home}", file=sys.stderr)
        return 1
    for src in sorted((home / "DLLs").glob("*")):
        if src.suffix.lower() in (".pyd", ".dll") and not src.name.lower().startswith(DLLS_EXCLUDE_PREFIXES):
            copy(src, out / "DLLs" / src.name)
    zip_count = build_stdlib_zip(home / "Lib", out / f"{args.python_dll}.zip")
    # ._pth가 있으면 Python은 격리 모드로 이 목록만 sys.path로 쓴다(site-packages를 읽지 않는다).
    # 경로는 이 파일이 있는 폴더 기준이다. "."은 quarterflying.pyd, scripts는 게임 스크립트.
    (out / f"{args.python_dll}._pth").write_text(
        f"{args.python_dll}.zip\nDLLs\n.\npython\nscripts\n", encoding="utf-8")

    for f in args.runtime_python:
        copy(Path(f), out / "python" / Path(f).name)
    for f in args.runtime_python_core:
        copy(Path(f), out / "python" / "core" / Path(f).name)

    # C4 검사
    for p in out.rglob("*"):
        if any(p.name.startswith(n) for n in FORBIDDEN_NAMES):
            print(f"[stage_template] forbidden file in template (Qt must not ship): {p}", file=sys.stderr)
            return 1

    files = {}
    for p in sorted(out.rglob("*")):
        if p.is_file() and p.name != MANIFEST:
            files[p.relative_to(out).as_posix()] = sha256(p)
    abi = "cp" + args.pyd.split(".cp", 1)[1].split("-", 1)[0] if ".cp" in args.pyd else ""
    manifest = {
        "format": TEMPLATE_FORMAT,
        "templateVersion": TEMPLATE_VERSION,
        "engineVersion": args.engine_version,
        "pythonAbi": abi,
        "pythonDll": args.python_dll,
        "player": "quarterflying_player.exe",
        "files": files,
    }
    (out / MANIFEST).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    total = sum((out / rel).stat().st_size for rel in files)
    print(f"[stage_template] {out}: {len(files)} files, {total / 1e6:.1f} MB (stdlib modules: {zip_count})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
