#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE_DIRS = [ROOT / "src", ROOT / "tests"]

FORBIDDEN = {
    "POSIX socket header": re.compile(r"#\s*include\s*[<\"]sys/socket\.h[>\"]"),
    "Winsock header": re.compile(r"#\s*include\s*[<\"]winsock2\.h[>\"]", re.IGNORECASE),
    "socket creation": re.compile(r"\bsocket\s*\("),
    "network connect": re.compile(r"\bconnect\s*\("),
    "network bind": re.compile(r"\bbind\s*\("),
    "network listen": re.compile(r"\blisten\s*\("),
    "network accept": re.compile(r"\baccept\s*\("),
    "ODBC": re.compile(r"\b(SQLDriverConnect|SQLConnect|SQLExecDirect)\b"),
    "SQLite": re.compile(r"\bsqlite3_(open|exec|prepare)\b"),
    "PostgreSQL": re.compile(r"\bPQconnectdb\b"),
    "MySQL": re.compile(r"\bmysql_real_connect\b"),
}

TEXT_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".py"}

def main() -> int:
    violations: list[str] = []
    for directory in SOURCE_DIRS:
        for path in directory.rglob("*"):
            if path.suffix.lower() not in TEXT_SUFFIXES:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for label, pattern in FORBIDDEN.items():
                if pattern.search(text):
                    violations.append(f"{path.relative_to(ROOT)}: {label}")

    if violations:
        print("Offline invariant failed:")
        for violation in violations:
            print(f"- {violation}")
        return 1

    print("Offline invariant passed: no socket or database APIs in runtime sources.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
