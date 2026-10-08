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


def test_cli_valid_program():
    res = run_cli("examples/valid/hello.nl")
    assert res.returncode == 0


def test_cli_syntax_error():
    res = run_cli("examples/syntax_errors/missing_semicolon.nl")
    assert res.returncode == 1
    assert "syntax error" in res.stderr


def test_cli_explain_recovery():
    res = run_cli("--explain-recovery", "examples/syntax_errors/missing_semicolon.nl")
    assert res.returncode == 1
    assert "NeuroParse Recovery Report" in res.stdout
    assert "Recovery Rate: 100.00%" in res.stdout
    assert "Strategy: neural" in res.stdout or "Strategy: heuristic" in res.stdout


def test_cli_recovery_modes():
    for mode in ["panic", "heuristic", "none", "auto"]:
        res = run_cli(f"--recovery={mode}", "examples/syntax_errors/missing_semicolon.nl")
        assert res.returncode == 1
        assert "syntax error" in res.stderr


def test_cli_diagnostics_json():
    import json
    res = run_cli("--diagnostics=json", "--recovery=neural", "examples/syntax_errors/missing_semicolon.nl")
    assert res.returncode == 1
    data = json.loads(res.stdout)
    assert data["schema_version"] == "1.0.0"
    assert data["success"] is False
    assert len(data["diagnostics"]) >= 1
    diag = data["diagnostics"][0]
    assert diag["severity"] == "error"
    assert diag["line"] == 6
    assert "recovery" in diag
    assert diag["recovery"]["strategy"] == "neural"
    assert "suggested_edit" in diag["recovery"]

