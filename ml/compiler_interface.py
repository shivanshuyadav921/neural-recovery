import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List, Optional, Tuple

ROOT = Path(__file__).resolve().parent.parent


def get_executable_path() -> str:
    exe_name = "neuroparse.exe" if sys.platform == "win32" else "neuroparse"
    exe_path = ROOT / "build" / exe_name
    if exe_path.exists():
        return str(exe_path)
    return str(ROOT / "build" / "neuroparse")


def run_compiler(*args: str, input_text: Optional[str] = None) -> subprocess.CompletedProcess:
    exe = get_executable_path()
    if input_text is not None:
        with tempfile.NamedTemporaryFile("w", suffix=".nl", delete=False, encoding="utf-8") as f:
            f.write(input_text)
            temp_path = f.name
        try:
            return subprocess.run([exe, *args, temp_path], capture_output=True, text=True)
        finally:
            if os.path.exists(temp_path):
                try:
                    os.remove(temp_path)
                except Exception:
                    pass
    else:
        return subprocess.run([exe, *args], capture_output=True, text=True)


def is_valid_neurolang(source: str) -> bool:
    res = run_compiler(input_text=source)
    return res.returncode == 0


def get_tokens(source: str) -> List[dict]:
    res = run_compiler("--dump-tokens", input_text=source)
    tokens = []
    # Pattern: [line:column] CATEGORY TYPE "lexeme"
    pattern = re.compile(r'\[(\d+):(\d+)\]\s+(\S+)\s+(\S+)\s+"(.*)"$')
    offset = 0
    for line in res.stdout.strip().splitlines():
        match = pattern.match(line)
        if match:
            line_num, col_num, category, tok_type, lexeme = match.groups()
            tokens.append({
                "line": int(line_num),
                "column": int(col_num),
                "category": category,
                "type": tok_type,
                "lexeme": lexeme,
                "offset": offset,
            })
            offset += len(lexeme)
    return tokens


def get_diagnostics_and_contexts(source: str) -> Tuple[List[dict], List[dict]]:
    res = run_compiler("--dump-context", input_text=source)
    errors = []
    contexts = []

    if res.returncode == 0 or "no syntax errors" in res.stdout:
        return errors, contexts

    # Parse Error Context blocks from stdout
    blocks = res.stdout.split("----------------------------------------")
    for block in blocks:
        block = block.strip()
        if not block:
            continue

        lines = block.splitlines()
        loc_match = re.search(r"(\d+):(\d+):\s+Error Context\s+\((.*)\)", lines[0])
        line_num = int(loc_match.group(1)) if loc_match else 1
        col_num = int(loc_match.group(2)) if loc_match else 1
        diag_msg = loc_match.group(3) if loc_match else ""

        token_idx_match = re.search(r"Token index:\s+(\d+)\s+of\s+(\d+)", block)
        token_idx = int(token_idx_match.group(1)) if token_idx_match else 0
        total_tokens = int(token_idx_match.group(2)) if token_idx_match else 0

        construct_match = re.search(r"Construct:\s+(.*)", block)
        construct = construct_match.group(1).strip() if construct_match else ""

        expected_tokens = []
        expected_match = re.search(r"Expected terminals:\s+(.*)", block)
        if expected_match:
            terminals = expected_match.group(1).split(",")
            expected_tokens = [t.strip().strip("'\"") for t in terminals if t.strip()]

        errors.append({
            "line": line_num,
            "column": col_num,
            "token_index": token_idx,
            "message": diag_msg,
            "expected_tokens": expected_tokens,
            "construct": construct,
        })

        # Token sections
        tokens_before = []
        current_token = {"type": "UNKNOWN", "lexeme": "", "line": line_num, "column": col_num}
        tokens_after = []

        tok_pattern = re.compile(r'^\s*\[(\d+):(\d+)\]\s+(\w+)\s+"(.*)"$')

        section = None
        for line in lines:
            line_str = line.strip()
            if line_str.startswith("Tokens before"):
                section = "before"
            elif line_str.startswith("Current error token:"):
                section = "current"
            elif line_str.startswith("Tokens after"):
                section = "after"
            elif line_str.startswith("Expected terminals:"):
                section = "expected"
            elif tok_pattern.match(line):
                m = tok_pattern.match(line)
                t_line, t_col, t_type, t_lex = m.groups()
                tok_dict = {
                    "line": int(t_line),
                    "column": int(t_col),
                    "type": t_type,
                    "lexeme": t_lex,
                }
                if section == "before":
                    tokens_before.append(tok_dict)
                elif section == "current":
                    current_token = tok_dict
                elif section == "after":
                    tokens_after.append(tok_dict)

        contexts.append({
            "token_index": token_idx,
            "total_tokens": total_tokens,
            "line": line_num,
            "column": col_num,
            "diagnostic_message": diag_msg,
            "construct": construct,
            "expected_tokens": expected_tokens,
            "tokens_before": tokens_before,
            "current_token": current_token,
            "tokens_after": tokens_after,
        })

    return errors, contexts
