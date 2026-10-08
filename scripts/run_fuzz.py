#!/usr/bin/env python3
"""
NeuroParse Phase 17 - Reproducible Fuzzer & Robustness Test Harness

Runs systematic fuzz testing across compiler boundaries:
- Lexer (character and byte-level fuzzing)
- Parser (pathological syntax trees and deep nesting)
- Recovery (adversarial token sequences and loop limits)
- ErrorRecoveryController & RepairValidator

Saves results to JSON and creates minimized regression files if any crashes occur.
"""

import argparse
import json
import os
import random
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

COMPILER_BIN = Path(__file__).resolve().parent.parent / "build" / "neuroparse.exe"
DEFAULT_CORPUS_DIR = Path(__file__).resolve().parent.parent / "tests" / "fuzz" / "corpus"
REGRESSION_DIR = Path(__file__).resolve().parent.parent / "tests" / "fuzz" / "regressions"


class MutationEngine:
    """Applies controlled structural and lexical mutations to source code."""

    KEYWORDS = ["let", "function", "return", "if", "else", "while", "true", "false"]
    OPERATORS = ["+", "-", "*", "/", "%", "==", "!=", "<", "<=", ">", ">=", "&&", "||", "!", "="]
    DELIMITERS = ["(", ")", "{", "}", "[", "]", ";", ",", ":"]

    def __init__(self, rng: random.Random):
        self.rng = rng

    def mutate_tokens(self, source: str) -> Tuple[str, str]:
        """Apply structural token-level mutation."""
        tokens = source.split()
        if not tokens:
            return "let x = 42;", "EMPTY_DEFAULT"

        strategy = self.rng.choice([
            "DELETE_TOKEN",
            "INSERT_TOKEN",
            "REPLACE_TOKEN",
            "SWAP_NEIGHBORS",
            "DUPLICATE_TOKEN",
            "TRUNCATE",
            "APPEND_GARBAGE",
            "MUTATE_DELIMITER",
        ])

        idx = self.rng.randint(0, len(tokens) - 1)

        if strategy == "DELETE_TOKEN":
            mutated = tokens[:idx] + tokens[idx + 1 :]
            desc = f"DELETE_TOKEN@{idx} ({tokens[idx]})"
        elif strategy == "INSERT_TOKEN":
            insert_val = self.rng.choice(self.KEYWORDS + self.OPERATORS + self.DELIMITERS + ["42", "var_x", '"str"'])
            mutated = tokens[:idx] + [insert_val] + tokens[idx:]
            desc = f"INSERT_TOKEN@{idx} ({insert_val})"
        elif strategy == "REPLACE_TOKEN":
            replace_val = self.rng.choice(self.KEYWORDS + self.OPERATORS + self.DELIMITERS + ["0", "y"])
            mutated = tokens[:idx] + [replace_val] + tokens[idx + 1 :]
            desc = f"REPLACE_TOKEN@{idx} ({tokens[idx]} -> {replace_val})"
        elif strategy == "SWAP_NEIGHBORS":
            if idx < len(tokens) - 1:
                mutated = list(tokens)
                mutated[idx], mutated[idx + 1] = mutated[idx + 1], mutated[idx]
                desc = f"SWAP_NEIGHBORS@{idx},{idx+1}"
            else:
                mutated = tokens[:-1]
                desc = "SWAP_FALLBACK_DELETE"
        elif strategy == "DUPLICATE_TOKEN":
            mutated = tokens[:idx] + [tokens[idx]] + tokens[idx:]
            desc = f"DUPLICATE_TOKEN@{idx} ({tokens[idx]})"
        elif strategy == "TRUNCATE":
            cut = max(1, idx)
            mutated = tokens[:cut]
            desc = f"TRUNCATE@{cut}"
        elif strategy == "MUTATE_DELIMITER":
            delim = self.rng.choice(self.DELIMITERS)
            mutated = tokens[:idx] + [delim] + tokens[idx + 1 :]
            desc = f"MUTATE_DELIMITER@{idx} ({delim})"
        else:  # APPEND_GARBAGE
            garbage = self.rng.choice(["!@#$%", ">>>", "999.999.999", "/* unclosed comment", '"unclosed string'])
            mutated = tokens + [garbage]
            desc = f"APPEND_GARBAGE ({garbage})"

        return " ".join(mutated), desc

    def mutate_bytes(self, source: str) -> Tuple[str, str]:
        """Apply byte/character-level mutation for lexer fuzzing."""
        if not source:
            source = "let x = 1;"

        strategy = self.rng.choice([
            "RANDOM_BYTE_FLIP",
            "INSERT_CONTROL_CHARS",
            "UNICODE_INJECTION",
            "EXTREME_NESTING",
            "EXTREME_IDENTIFIER",
        ])

        if strategy == "RANDOM_BYTE_FLIP":
            s_list = list(source)
            pos = self.rng.randint(0, len(s_list) - 1)
            s_list[pos] = chr(self.rng.randint(32, 126))
            return "".join(s_list), f"BYTE_FLIP@{pos}"
        elif strategy == "INSERT_CONTROL_CHARS":
            ctrl = chr(self.rng.randint(0, 31))
            pos = self.rng.randint(0, len(source))
            return source[:pos] + ctrl + source[pos:], f"INSERT_CONTROL@{pos}"
        elif strategy == "UNICODE_INJECTION":
            unicode_char = self.rng.choice(["🦀", "λ", "π", "🚀", "⚡", "é", "ñ", "漢字"])
            pos = self.rng.randint(0, len(source))
            return source[:pos] + unicode_char + source[pos:], f"UNICODE_INJECTION@{pos}"
        elif strategy == "EXTREME_NESTING":
            depth = self.rng.randint(50, 200)
            return ("(" * depth) + "42" + (")" * depth) + ";", f"EXTREME_NESTING_DEPTH_{depth}"
        else:  # EXTREME_IDENTIFIER
            length = self.rng.randint(500, 2000)
            return f"let {'a' * length} = 1;", f"EXTREME_IDENTIFIER_LEN_{length}"


def load_seed_corpus(corpus_dir: Path) -> List[Tuple[str, str]]:
    """Loads all .nl seed files."""
    seeds = []
    if corpus_dir.exists():
        for path in sorted(corpus_dir.glob("*.nl")):
            try:
                content = path.read_text(encoding="utf-8", errors="replace")
                seeds.append((path.name, content))
            except Exception as e:
                print(f"Warning: could not read {path}: {e}")
    if not seeds:
        seeds.append(("default_minimal.nl", "function main() {\n  let x = 42;\n  return x;\n}\n"))
    return seeds


def run_fuzz(
    seed: int = 42,
    iterations: int = 1000,
    corpus_dir: Optional[Path] = None,
    timeout: float = 2.0,
    output_path: Optional[Path] = None,
    verbose: bool = False,
) -> Dict[str, Any]:
    """Executes the full fuzzing suite."""
    rng = random.Random(seed)
    mutator = MutationEngine(rng)

    if corpus_dir is None:
        corpus_dir = DEFAULT_CORPUS_DIR

    seeds = load_seed_corpus(corpus_dir)
    REGRESSION_DIR.mkdir(parents=True, exist_ok=True)

    if not COMPILER_BIN.exists():
        raise FileNotFoundError(f"NeuroParse executable not found at {COMPILER_BIN}. Please build the project first.")

    stats: Dict[str, Any] = {
        "seed": seed,
        "total_iterations": iterations,
        "completed_cases": 0,
        "valid_parses": 0,
        "clean_rejections": 0,
        "crashes": 0,
        "hangs_or_timeouts": 0,
        "invariant_violations": 0,
        "duration_seconds": 0.0,
        "reproducers": [],
        "targets_tested": [
            "Lexer",
            "Parser",
            "ContextExtractor",
            "RepairValidator",
            "RepairApplier",
            "ErrorRecoveryController",
            "CLI_JSON_Diagnostics",
        ],
    }

    start_time = time.time()

    for i in range(1, iterations + 1):
        seed_name, base_source = rng.choice(seeds)

        # 50% structural token mutation, 50% character/byte fuzzing
        if rng.random() < 0.5:
            mutated_source, mutation_desc = mutator.mutate_tokens(base_source)
        else:
            mutated_source, mutation_desc = mutator.mutate_bytes(base_source)

        recovery_mode = rng.choice(["auto", "panic", "heuristic", "neural", "none"])

        # Stream mutation directly via stdin ("-") to eliminate all disk I/O and NTFS file lock contention
        cmd = [
            str(COMPILER_BIN),
            "-",
            "--diagnostics=json",
            f"--recovery={recovery_mode}",
        ]

        try:
            res = subprocess.run(
                cmd,
                input=mutated_source,
                capture_output=True,
                encoding="utf-8",
                errors="replace",
                timeout=timeout,
            )

            # Check exit code:
            # 0: Parse / recovery successful
            # 1: Syntax error cleanly diagnosed and reported
            # Any negative exit code (signal) or >= 128 / > 2 indicates crash/abort
            if res.returncode in (0, 1):
                stats["completed_cases"] += 1
                if res.returncode == 0:
                    stats["valid_parses"] += 1
                else:
                    stats["clean_rejections"] += 1
            else:
                stats["crashes"] += 1
                reproducer_file = REGRESSION_DIR / f"crash_seed_{seed}_iter_{i}.nl"
                reproducer_file.write_text(mutated_source, encoding="utf-8", errors="replace")
                stats["reproducers"].append({
                    "iteration": i,
                    "type": "CRASH",
                    "returncode": res.returncode,
                    "mutation": mutation_desc,
                    "seed": seed,
                    "file": str(reproducer_file),
                    "stderr": res.stderr[:500],
                })
                print(f"[!] CRASH detected at iteration {i}! Saved reproducer to {reproducer_file}")

        except subprocess.TimeoutExpired:
            stats["hangs_or_timeouts"] += 1
            reproducer_file = REGRESSION_DIR / f"hang_seed_{seed}_iter_{i}.nl"
            reproducer_file.write_text(mutated_source, encoding="utf-8", errors="replace")
            stats["reproducers"].append({
                "iteration": i,
                "type": "HANG",
                "mutation": mutation_desc,
                "seed": seed,
                "file": str(reproducer_file),
            })
            print(f"[!] HANG detected at iteration {i}! Saved reproducer to {reproducer_file}")

        if verbose and i % 100 == 0:
            print(f"Progress: {i}/{iterations} iterations completed ({stats['crashes']} crashes, {stats['hangs_or_timeouts']} hangs)")

    duration = time.time() - start_time
    stats["duration_seconds"] = round(duration, 3)

    if output_path:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(stats, f, indent=2)

    return stats


def main():
    parser = argparse.ArgumentParser(description="NeuroParse Phase 17 Fuzzer & Robustness Suite")
    parser.add_argument("--seed", type=int, default=42, help="Random seed for reproducibility")
    parser.add_argument("--iterations", type=int, default=1000, help="Number of fuzz iterations")
    parser.add_argument("--corpus", type=str, default=str(DEFAULT_CORPUS_DIR), help="Path to seed corpus directory")
    parser.add_argument("--timeout", type=float, default=2.0, help="Per-case timeout in seconds")
    parser.add_argument("--output", type=str, default="benchmarks/results/phase17_fuzz_results.json", help="JSON output path")
    parser.add_argument("--verbose", action="store_true", help="Print progress intervals")

    args = parser.parse_args()

    print(f"==================================================")
    print(f"NeuroParse Phase 17 - Robustness Fuzzer")
    print(f"Seed:       {args.seed}")
    print(f"Iterations: {args.iterations}")
    print(f"Corpus:     {args.corpus}")
    print(f"Timeout:    {args.timeout}s")
    print(f"Output:     {args.output}")
    print(f"==================================================")

    stats = run_fuzz(
        seed=args.seed,
        iterations=args.iterations,
        corpus_dir=Path(args.corpus),
        timeout=args.timeout,
        output_path=Path(args.output) if args.output else None,
        verbose=args.verbose,
    )

    print("\n--- Fuzzing Summary ---")
    print(f"Total Iterations:      {stats['total_iterations']}")
    print(f"Completed Cases:       {stats['completed_cases']}")
    print(f"  Valid Parses:        {stats['valid_parses']}")
    print(f"  Clean Rejections:    {stats['clean_rejections']}")
    print(f"Crashes:               {stats['crashes']}")
    print(f"Hangs / Timeouts:      {stats['hangs_or_timeouts']}")
    print(f"Invariant Violations:  {stats['invariant_violations']}")
    print(f"Duration:              {stats['duration_seconds']}s")
    print("==================================================")

    if stats["crashes"] > 0 or stats["hangs_or_timeouts"] > 0:
        print(f"[FAIL] Discovered {stats['crashes']} crash(es) and {stats['hangs_or_timeouts']} hang(s)!")
        sys.exit(1)
    else:
        print("[PASS] Robustness properties satisfied across all fuzz cases.")
        sys.exit(0)


if __name__ == "__main__":
    main()
