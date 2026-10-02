"""hierarchy_model.py 단위 테스트 (Qt/엔진 불필요). python test_hierarchy_model.py 로 실행."""

import sys

from hierarchy_model import (ABOVE_ITEM, BELOW_ITEM, ON_ITEM, ON_VIEWPORT,
                             order_for_tree, resolve_drop_parent)


def test_parents_come_before_children_and_siblings_by_id():
    nodes = [(5, "Head", 3), (3, "Pole", 1), (1, "Lamp", 0), (4, "Base", 1), (2, "Cam", 0)]
    ordered = order_for_tree(nodes)
    assert [n.entity_id for n in ordered] == [1, 3, 5, 4, 2]
    assert [n.parent_id for n in ordered] == [0, 1, 3, 1, 0]


def test_missing_parent_is_shown_as_root():
    ordered = order_for_tree([(7, "Orphan", 99)])
    assert ordered[0].parent_id == 0


def test_cycle_nodes_are_still_listed_once():
    ordered = order_for_tree([(1, "A", 2), (2, "B", 1), (3, "C", 0)])
    assert sorted(n.entity_id for n in ordered) == [1, 2, 3]
    assert len(ordered) == 3
    # 순환 중 하나는 루트로 올라오고 나머지는 그 아래에 달린다
    assert sum(1 for n in ordered if n.parent_id == 0) == 2


def test_self_parent_is_root():
    ordered = order_for_tree([(1, "Self", 1)])
    assert ordered == [ordered[0]._replace(parent_id=0)]


def test_drop_resolution():
    assert resolve_drop_parent(ON_ITEM, 5, 3) == 5          # 위에 놓으면 자식
    assert resolve_drop_parent(ABOVE_ITEM, 5, 3) == 3       # 사이에 놓으면 형제
    assert resolve_drop_parent(BELOW_ITEM, 5, 0) == 0       # 루트 항목의 형제 = 루트
    assert resolve_drop_parent(ON_VIEWPORT, None, 0) == 0   # 빈 곳 = 루트
    assert resolve_drop_parent(ON_ITEM, None, 0) == 0


if __name__ == "__main__":
    tests = [v for k, v in list(globals().items()) if k.startswith("test_") and callable(v)]
    failed = 0
    for t in tests:
        try:
            t()
            print(f"PASS  {t.__name__}")
        except Exception as e:
            failed += 1
            print(f"FAIL  {t.__name__}: {type(e).__name__}: {e}")
    print(f"\n{len(tests) - failed}/{len(tests)} passed")
    sys.exit(1 if failed else 0)
