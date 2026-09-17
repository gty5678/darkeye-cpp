"""Render the Python WorkPage as a local visual migration baseline."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--python-root", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    python_root = Path(args.python_root).resolve()
    sys.path.insert(0, str(python_root))

    from PySide6.QtCore import QTimer
    from PySide6.QtWidgets import QApplication

    from core.database.db_queue import start_db_queue_worker, stop_db_queue_worker
    from main import load_app_stylesheet
    from ui.pages.WorkPage import WorkPage

    application = QApplication(sys.argv[:1])
    load_app_stylesheet(application)
    start_db_queue_worker()

    page = WorkPage()
    page.resize(1340, 800)
    page.show()

    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    result = {"saved": False}

    def save_and_quit() -> None:
        page.lazy_area.verticalScrollBar().setValue(0)
        application.processEvents()
        result["saved"] = page.grab().save(str(output))
        application.quit()

    QTimer.singleShot(2500, save_and_quit)
    application.exec()
    stop_db_queue_worker()
    return 0 if result["saved"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
