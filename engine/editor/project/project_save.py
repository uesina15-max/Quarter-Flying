"""
editor/project/project_save.py

프로젝트 저장(File > Save Project)과 "저장 안 된 변경" 확인의 공통 실행부.

프로젝트의 저장 대상은 서로 다른 모듈이 각자 가진다: 씬(엔진 SceneIO), 입력 액션 맵(엔진 InputActions),
액션(Motion Editor, core/action_data.py), project.json, 세션(project/session.py). 이 모듈은 그 대상들을
**SaveItem 목록**으로 받아 차례로 저장하고, 결과를 대상 이름별로 모은다. 대상을 모으는 것(어느 것이 바뀌었나)은
에디터(project_actions.py)가 하고, 실제 저장은 각 모듈의 기존 저장 함수가 한다 - 형식 지식은 여기 없다.

실패를 대상 이름으로 남기는 이유(CLAUDE.md 관례 4): 여러 저장을 try 하나로 묶으면 "프로젝트 저장 실패"만 보이고
어느 파일이 안 써졌는지 모른다. 한 대상이 실패해도 나머지는 계속 저장한다 - 하나 때문에 다른 작업까지 잃지 않게.

PySide6를 import하지 않는다.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, List, Tuple


@dataclass
class SaveItem:
    label: str                      # 사용자에게 보이는 이름("씬 Main.scene.json", "액션 Attack")
    save: Callable[[], bool]        # True = 저장함. False = 취소했거나 실패(이유는 save가 이미 보여 줬다)


@dataclass
class SaveResult:
    saved: List[str] = field(default_factory=list)
    not_saved: List[Tuple[str, str]] = field(default_factory=list)   # (label, 이유)

    @property
    def ok(self) -> bool:
        return not self.not_saved

    def summary(self) -> str:
        """상태바 한 줄."""
        if not self.saved and not self.not_saved:
            return "저장할 변경 없음"
        parts = []
        if self.saved:
            parts.append(f"저장 {len(self.saved)}개")
        if self.not_saved:
            parts.append(f"저장 못 함 {len(self.not_saved)}개: " + ", ".join(label for label, _ in self.not_saved))
        return " / ".join(parts)

    def failure_details(self) -> str:
        return "\n".join(f"  {label}: {reason}" for label, reason in self.not_saved)


def save_items(items: List[SaveItem]) -> SaveResult:
    """items를 순서대로 저장한다. 예외가 나도 다음 대상으로 넘어가고, 예외 메시지를 그 대상의 이유로 남긴다."""
    result = SaveResult()
    for item in items:
        try:
            ok = item.save()
        except Exception as e:   # 대상 하나의 버그가 나머지 저장을 막지 않게 - 이유는 대상 이름과 함께 남는다
            result.not_saved.append((item.label, f"{type(e).__name__}: {e}"))
            continue
        if ok:
            result.saved.append(item.label)
        else:
            result.not_saved.append((item.label, "취소했거나 실패했습니다(앞의 메시지 참고)"))
    return result
