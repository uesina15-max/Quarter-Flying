"""
ge_transaction.py
Transaction context manager for the Quarter Flying editor.

Usage
-----
    from ge_transaction import transaction

    with transaction(editor, "Move entities"):
        editor.move_entity(e1, Vec3(1, 0, 0))
        editor.move_entity(e2, Vec3(2, 0, 0))

    # On successful exit  : commit_transaction() is called automatically.
    # On exception        : cancel_transaction() is called and the exception re-raised.
"""
from contextlib import contextmanager


@contextmanager
def transaction(editor, name: str = "Transaction"):
    """Context manager that wraps the EditorAPI transaction lifecycle.

    Parameters
    ----------
    editor:
        An ``EditorAPI`` instance (``ge_python.EditorAPI``).
    name:
        Human-readable label shown in the Undo history.

    Raises
    ------
    Any exception thrown inside the ``with`` block is propagated after
    ``cancel_transaction()`` has been called.  Exceptions thrown by
    ``begin_transaction`` itself are not suppressed.
    """
    editor.begin_transaction(name)
    try:
        yield
        editor.commit_transaction()
    except Exception:
        editor.cancel_transaction()
        raise
