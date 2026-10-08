# NeuroParse Fuzzing, Robustness & Adversarial Testing (Phase 17)

## 1. Overview & Objectives

Phase 17 establishes a comprehensive robustness-testing and fuzzing infrastructure for the NeuroParse research compiler. The goal is to stress every internal compiler boundary under hostile, malformed, and adversarial inputs to guarantee bounded execution, memory safety, structural AST/token invariants, and deterministic failure modes.

The core safety property enforced across the entire system is:

$$\text{Malformed / Adversarial Input} \longrightarrow \text{Compiler} \longrightarrow \text{Bounded, Deterministic Diagnostics / Recovery}$$

---

## 2. Robustness Threat Model

| Hostile Input Vector | Attack Scenario | Defense & Invariant Layer |
| :--- | :--- | :--- |
| **Pathological Source** | Deeply nested delimiters (depth > 500), 10k-char identifiers, binary byte streams, invalid UTF-8. | Lexer bounds checking, `check_token_stream_invariants`, recursive descent step limits. |
| **Cascading Syntax Errors** | Multiple syntax errors in adjacent expressions causing recovery loops. | `ErrorRecoveryController` loop protection (`consecutive_same_pos_attempts_ >= 2`, `kMaxRecoveriesPerParse = 50`), fallback to panic mode. |
| **Adversarial Neural Outputs** | Out-of-bounds token positions, negative indices, NaN/Inf confidences, invalid token types. | `RepairValidator` strict 6-stage validation suite rejecting all out-of-bounds / unsafe repair proposals. |
| **Token Stream Corruption** | Removing EOF, reordering tokens, inserting invalid tokens at boundary positions. | `RepairApplier` atomic application, `check_token_stream_invariants` asserting monotonic source locations and single trailing EOF. |
| **Malformed Model Output Tensors** | Neural model emitting NaN logits, wrong dimensions, or unmapped token IDs. | Tensor shape assertions, PyTorch/ONNX output decoding sanitization, safe fallback to heuristic recovery. |

---

## 3. Fuzzing Architecture & Subsystems

```
                                  ┌────────────────────────┐
                                  │      Seed Corpus       │
                                  │  (tests/fuzz/corpus/)  │
                                  └───────────┬────────────┘
                                              │
                                              ▼
                                  ┌────────────────────────┐
                                  │     MutationEngine     │
                                  │  (Token/Byte Mutator)  │
                                  └───────────┬────────────┘
                                              │
                                              ▼
 ┌────────────────────────────────────────────────────────────────────────────────────────┐
 │                                NeuroParse Compiler CLI                                 │
 │                                                                                        │
 │    ┌─────────────┐       ┌─────────────┐       ┌─────────────┐       ┌─────────────┐   │
 │    │    Lexer    │ ───►  │   Parser    │ ───►  │ ContextExtr │ ───►  │ NeuralModel │   │
 │    └─────────────┘       └─────────────┘       └─────────────┘       └─────────────┘   │
 │           │                     │                     │                     │          │
 │           ▼                     ▼                     ▼                     ▼          │
 │   Token Invariants        AST Invariants        Window Bounds        Validator Check   │
 └────────────────────────────────────────────────────────────────────────────────────────┘
                                              │
                                              ▼
                                  ┌────────────────────────┐
                                  │   Result Collector     │
                                  │ (JSON Stats / Repros)  │
                                  └────────────────────────┘
```

### 3.1 Formal Invariant Checkers
Located in `compiler/include/neuroparse/fuzz/invariants.hpp` and `compiler/src/fuzz/invariants.cpp`:
- **`check_token_stream_invariants(tokens)`**:
  - Stream is non-empty.
  - Exactly one EOF token exists at `tokens.back()`.
  - No internal EOF tokens exist.
  - Source locations are strictly monotonic (`line` and `column` non-decreasing).
  - No `TokenType::Invalid` exists in sanitized streams.
- **`check_ast_invariants(root)`**:
  - Traverses the entire AST with a visitor.
  - Asserts non-null child nodes across all `Declaration`, `Statement`, and `Expression` nodes (`Program`, `FunctionDeclaration`, `IfStatement`, `WhileStatement`, `BinaryExpression`, `UnaryExpression`, `CallExpression`).
  - Asserts non-inverted source spans.

---

## 4. Fuzzing Targets

1. **Lexer**:
   - Random byte fuzzing, extreme identifier lengths (5,000+ chars), huge numeric literals, unterminated strings with escape sequences, unterminated nested comments, binary/control characters, Unicode identifiers (e.g. `🦀`, `π`).
2. **Parser**:
   - Deeply nested parentheses/braces, repeated binary/unary operators (`++++`, `****`), mismatched delimiters, truncated programs at EOF.
3. **ContextExtractor**:
   - Boundary cursor indices (0, middle, EOF, past EOF, empty token stream, window sizes from 0 to 1,000).
4. **RepairValidator**:
   - Malicious `RepairPrediction` structs with NaN confidences, negative positions, out-of-range token IDs, and invalid operation enums.
5. **RepairApplier**:
   - Boundary insertions, deletions, and replacements at index 0, mid-stream, before EOF, and out-of-bounds positions.
6. **ErrorRecoveryController**:
   - Cycle detection under synthetic adversary engines proposing repeating non-progressing repairs.
7. **Property-Based Testing (Hypothesis)**:
   - Automated property testing of ContextEncoder, RepairModel tensor dimensions, NaN/Inf decoding, and synthetic dataset generator.

---

## 5. Seed Corpus

The seed corpus is located in `tests/fuzz/corpus/`:
- `seed_01_minimal.nl`: Simple function and variable declaration.
- `seed_02_expressions.nl`: Complex arithmetic, comparison, logical expressions, and calls.
- `seed_03_control_flow.nl`: If-else statements, while loops, block nesting.
- `seed_04_complex_program.nl`: Multi-function program with realistic structures.
- `seed_05_missing_delimiter.nl`: Missing semicolon and parentheses.
- `seed_06_unclosed_string.nl`: Unterminated string literal.
- `seed_07_nested_errors.nl`: Cascading syntax errors in nested expressions.
- `seed_08_eof_error.nl`: Unclosed block at EOF.

---

## 6. Sanitizer Configuration

For environments supporting Clang/GCC sanitizers:

### AddressSanitizer (ASan) & UndefinedBehaviorSanitizer (UBSan)
```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan
./build-asan/neuroparse_unit_tests
```

*Note: On Windows MinGW toolchains without ASan runtime libraries, run the standard C++ test suite and Python fuzzer harness.*

---

## 7. Execution of Reproducible Fuzzer

### Command Line Usage
```bash
python scripts/run_fuzz.py --seed 42 --iterations 1000 --corpus tests/fuzz/corpus --timeout 2.0 --output benchmarks/results/phase17_fuzz_results.json
```

### Actual Fuzz Run Results (Seed 42)
- **Seed**: `42`
- **Total Iterations**: `1000`
- **Completed Cases**: `1000`
- **Valid Parses**: `189`
- **Clean Rejections (Accurate Diagnostics)**: `811`
- **Crashes**: `0`
- **Hangs / Timeouts**: `0`
- **Invariant Violations**: `0`
- **Duration**: `24.05s`
- **Output Artifact**: `benchmarks/results/phase17_fuzz_results.json`

---

## 8. Failure Reproduction Workflow

If a future fuzz run uncovers a crash or hang, `scripts/run_fuzz.py` automatically preserves the reproducer in `tests/fuzz/regressions/` and logs:
```
seed=12345
iteration=812
mutation=DELETE_TOKEN@17
input=tests/fuzz/regressions/crash_seed_12345_iter_812.nl
```

To reproduce:
```bash
./build/neuroparse tests/fuzz/regressions/crash_seed_12345_iter_812.nl --diagnostics=json --recovery=auto
```
