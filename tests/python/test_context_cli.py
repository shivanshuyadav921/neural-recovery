import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent


def get_executable():
    exe_name = "neuroparse.exe" if sys.platform == "win32" else "neuroparse"
    exe_path = ROOT / "build" / exe_name
    if exe_path.exists():
        return str(exe_path)
    return str(ROOT / "build" / "neuroparse")


def run_cli(*args):
    return subprocess.run([get_executable(), *args], capture_output=True, text=True)


def test_cli_dump_context_valid_program():
    res = run_cli("--dump-context", "examples/valid/hello.nl")
    assert res.returncode == 0
    assert "no syntax errors" in res.stdout


def test_cli_dump_context_invalid_program():
    res = run_cli("--dump-context", "examples/syntax_errors/missing_semicolon.nl")
    assert res.returncode == 1
    assert "Error Context" in res.stdout
    assert "Tokens before" in res.stdout
    assert "Current error token" in res.stdout
    assert "Expected terminals" in res.stdout
