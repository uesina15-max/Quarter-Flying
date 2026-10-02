"""
editor/hierarchy_model.py

Scene Hierarchy 트리(panels/scene_hierarchy.py)의 순수 로직. Qt와 엔진 없이 테스트한다
(test_hierarchy_model.py).

부모 규칙은 엔진(ecs/Hierarchy.h)이 정한다. 여기서는 엔진이 돌려준 parent id로 트리 순서를 정하고,
드롭 위치를 "새 부모"로 바꾸는 것만 한다. 부모를 실제로 바꾸는 것은 EditorAPI.set_parent(Undo 가능,
순환은 엔진이 거절)다.
"""

from typing import Dict, Iterable, List, NamedTuple, Optional, Tuple


class TreeNode(NamedTuple):
    entity_id: int
    name: str
    parent_id: int      # 0 = 루트


def order_for_tree(nodes: Iterable[Tuple[int, str, int]]) -> List[TreeNode]:
    """(id, name, parent_id)들을 부모가 항상 자식보다 먼저 오도록 정렬한다(전위 순회, 형제는 id 순).

    parent_id가 목록에 없거나(0 포함) 순환에 걸린 노드는 루트로 둔다 - 엔진의 ComputeWorldMatrix가
    순환을 루트처럼 끊는 것과 같은 방향이다. 어떤 경우에도 모든 노드가 정확히 한 번 나온다(트리에서
    엔티티가 조용히 사라지면 선택/삭제를 할 수 없게 된다).
    """
    by_id: Dict[int, TreeNode] = {}
    for entity_id, name, parent_id in nodes:
        by_id[entity_id] = TreeNode(entity_id, name, parent_id or 0)

    children: Dict[int, List[int]] = {}
    roots: List[int] = []
    for node in by_id.values():
        if node.parent_id in by_id and node.parent_id != node.entity_id:
            children.setdefault(node.parent_id, []).append(node.entity_id)
        else:
            roots.append(node.entity_id)

    ordered: List[TreeNode] = []
    visited = set()

    def visit(entity_id: int, parent_id: int):
        stack = [(entity_id, parent_id)]
        while stack:
            eid, pid = stack.pop()
            if eid in visited:
                continue
            visited.add(eid)
            ordered.append(TreeNode(eid, by_id[eid].name, pid))
            for child in sorted(children.get(eid, []), reverse=True):
                stack.append((child, eid))

    for root in sorted(roots):
        visit(root, 0)
    # 순환에 속한 노드는 어느 루트에서도 닿지 않는다 - 루트로 보여준다.
    for eid in sorted(by_id):
        if eid not in visited:
            visit(eid, 0)
    return ordered


# QAbstractItemView.DropIndicatorPosition과 같은 의미의 값(Qt 없이 테스트하려고 따로 둔다).
ON_ITEM = "on_item"
ABOVE_ITEM = "above_item"
BELOW_ITEM = "below_item"
ON_VIEWPORT = "on_viewport"


def resolve_drop_parent(indicator: str, target_id: Optional[int], target_parent_id: int) -> int:
    """드롭 위치 -> 새 부모 id(0 = 루트).

    - 항목 위에 놓음: 그 항목의 자식이 된다.
    - 항목 사이(위/아래)에 놓음: 그 항목의 형제가 된다(= 같은 부모).
    - 빈 곳에 놓음: 루트가 된다.
    형제 사이 순서는 저장하지 않는다(트리는 id 순) - 위/아래 구분은 부모를 정하는 데만 쓴다.
    """
    if target_id is None or indicator == ON_VIEWPORT:
        return 0
    if indicator == ON_ITEM:
        return target_id
    return target_parent_id or 0
