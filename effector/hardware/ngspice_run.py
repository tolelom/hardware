"""ngspice 배치 실행과 결과 읽기. meas 결과와 print 결과(`이름 = 값`)를 사전으로 돌려준다."""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

SPICE_DIR = Path(__file__).resolve().parent / "spice"
RESULT = re.compile(r"^\s*([A-Za-z_][\w().,+\-*/]*)\s+=\s+([-+]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)")
ERROR_PATTERNS = [re.compile(pattern, re.IGNORECASE) for pattern in (
    r"^\s*error\b", r"timestep too small", r"simulation\(s\) aborted", r"^\s*warning from checkvalid",
    r"\bfailed!\s*$")]
IGNORED_PREFIXES = ("circuit:", "warning:")


class NgspiceError(Exception):
    pass


def error_lines(output: str) -> list[str]:
    """에러로 볼 줄. 회로 제목 줄과 복구되는 경고(gmin stepping failed 등)는 뺀다."""
    found = []
    for line in output.splitlines():
        if line.strip().lower().startswith(IGNORED_PREFIXES):
            continue
        if any(pattern.search(line) for pattern in ERROR_PATTERNS):
            found.append(line)
    return found


def available() -> bool:
    return shutil.which("ngspice") is not None


requires_ngspice = unittest.skipUnless(available(), "ngspice 없음. brew install ngspice 뒤 다시 실행")


def run(netlist: Path, expect: list[str] = ()) -> dict[str, float]:
    completed = subprocess.run(["ngspice", "-b", netlist.name], cwd=netlist.parent,
                               capture_output=True, text=True, timeout=1800)
    output = completed.stdout + completed.stderr
    if completed.returncode != 0 or error_lines(output):
        raise NgspiceError(f"{netlist.name}: ngspice 에러\n{output[-3000:]}")
    results = {}
    for line in output.splitlines():
        match = RESULT.match(line)
        if match:
            results[match.group(1).lower()] = float(match.group(2))
    missing = [name for name in expect if name.lower() not in results]
    if missing:
        raise NgspiceError(f"{netlist.name}: 결과 없음 {missing}\n{output[-3000:]}")
    return results


def run_text(text: str, directory: Path = SPICE_DIR, expect: list[str] = ()) -> dict[str, float]:
    handle, name = tempfile.mkstemp(suffix=".cir", prefix="_run_", dir=directory)
    try:
        with os.fdopen(handle, "w", encoding="utf-8") as stream:
            stream.write(text)
        return run(Path(name), expect)
    finally:
        os.unlink(name)
