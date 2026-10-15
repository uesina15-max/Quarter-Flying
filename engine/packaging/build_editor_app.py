"""
packaging/build_editor_app.py

완제품 에디터(docs/GAME_BUILD_PLAN.md의 ②)를 만든다. CMake `editor_app` 타깃이 실행한다:

    cmake --build . --config Release --target editor_app     →  build/app/Quarter Flying/

결과는 Python·PySide6·Visual Studio가 없는 PC에서도 실행되는 exe + DLL 폴더다.

    Quarter Flying/
    ├─ Quarter Flying.exe    에디터. Python 코드는 바이트코드로 exe 안에 들어 있다(.py 파일 없음)
    ├─ _internal/            python314.dll, Qt·엔진 DLL, .pyd
    ├─ assets/               샘플 프로젝트
    └─ runtime/win-x64/      Player 템플릿(Build Game이 복사해 간다)

순서:
  1. 전용 venv(--venv)를 만들고 requirements.txt를 설치한다. 개발용 Python의 다른 패키지(PyQt6 등)가
     섞이지 않게 하고, 버전을 고정해 같은 소스에서 같은 결과가 나오게 하려는 것이다.
     requirements.txt가 바뀌지 않았으면 설치를 건너뛴다.
  2. venv의 PyInstaller로 quarterflying_editor.spec을 실행한다(입력: --stage = cmake --install 결과).
  3. assets/와 runtime/win-x64/를 exe 옆에 복사한다.
  4. 결과를 검사한다: .py 소스가 섞이지 않았는지, 엔진 모듈과 Player 템플릿이 있는지.

venv는 .pyd를 빌드한 Python(이 스크립트를 실행한 인터프리터)으로 만든다. 버전이 다르면 .pyd를
불러오지 못한다.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import sys
import venv
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = HERE / "quarterflying_editor.spec"
REQUIREMENTS = HERE / "requirements.txt"
APP_NAME = "Quarter Flying"


def fail(msg: str) -> None:
    print(f"[editor_app] 오류: {msg}", file=sys.stderr)
    sys.exit(1)


def venv_python(venv_dir: Path) -> Path:
    return venv_dir / "Scripts" / "python.exe"


def ensure_venv(venv_dir: Path) -> Path:
    py = venv_python(venv_dir)
    if not py.exists():
        print(f"[editor_app] venv 생성: {venv_dir}")
        venv.EnvBuilder(with_pip=True, clear=True).create(venv_dir)

    marker = venv_dir / ".requirements.sha256"
    digest = hashlib.sha256(REQUIREMENTS.read_bytes()).hexdigest()
    if marker.exists() and marker.read_text().strip() == digest:
        return py
    print("[editor_app] 패키징 도구 설치 (requirements.txt)")
    subprocess.run([str(py), "-m", "pip", "install", "--disable-pip-version-check",
                    "-r", str(REQUIREMENTS)], check=True)
    marker.write_text(digest)
    return py


def check_stage(stage: Path) -> None:
    missing = [p for p in (stage / "editor" / "main.py", stage / "assets",
                           stage / "runtime" / "win-x64" / "runtime_manifest.json")
               if not p.exists()]
    if not list((stage / "bin").glob("quarterflying*.pyd")):
        missing.append(stage / "bin" / "quarterflying*.pyd")
    if missing:
        fail("설치 결과(--stage)에 다음이 없습니다: " + ", ".join(map(str, missing)))


def copy_tree(src: Path, dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns("__pycache__"))


def verify(app: Path) -> None:
    internal = app / "_internal"
    problems = []
    if not (app / f"{APP_NAME}.exe").exists():
        problems.append(f"{APP_NAME}.exe 없음")
    if not list(internal.glob("quarterflying*.pyd")):
        problems.append("_internal에 엔진 모듈(quarterflying*.pyd) 없음")
    if not (internal / "quarterflying_engine.dll").exists():
        problems.append("_internal에 quarterflying_engine.dll 없음")
    if not (app / "runtime" / "win-x64" / "runtime_manifest.json").exists():
        problems.append("Player 템플릿 없음")
    sources = [p for p in internal.rglob("*.py")]
    if sources:
        problems.append(f"_internal에 .py 소스 {len(sources)}개 (예: {sources[0].relative_to(app)})")
    for qt in ("PyQt5", "PyQt6"):
        if (internal / qt).exists():
            problems.append(f"다른 Qt 바인딩({qt})이 섞임")
    if problems:
        fail("결과 검사 실패: " + "; ".join(problems))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--stage", required=True, type=Path, help="cmake --install 결과 폴더")
    ap.add_argument("--out", required=True, type=Path, help="완제품 폴더를 만들 상위 폴더")
    ap.add_argument("--work", required=True, type=Path, help="PyInstaller 작업 폴더")
    ap.add_argument("--venv", required=True, type=Path, help="패키징 전용 venv 폴더")
    ap.add_argument("--icon", type=Path, help=".ico 파일(선택)")
    args = ap.parse_args()

    stage = args.stage.resolve()
    check_stage(stage)
    py = ensure_venv(args.venv.resolve())

    cmd = [str(py), "-m", "PyInstaller", str(SPEC), "--noconfirm", "--clean",
           "--distpath", str(args.out.resolve()), "--workpath", str(args.work.resolve()),
           "--", "--stage", str(stage)]
    if args.icon:
        cmd += ["--icon", str(args.icon.resolve())]
    subprocess.run(cmd, check=True)

    app = args.out.resolve() / APP_NAME
    copy_tree(stage / "assets", app / "assets")
    copy_tree(stage / "runtime" / "win-x64", app / "runtime" / "win-x64")
    verify(app)

    size = sum(f.stat().st_size for f in app.rglob("*") if f.is_file())
    print(f"[editor_app] 완료: {app} ({size / 2**20:.0f} MB)")


if __name__ == "__main__":
    main()
