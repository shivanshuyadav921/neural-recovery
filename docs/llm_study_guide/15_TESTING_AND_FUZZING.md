# Chapter 15: Testing, Fuzzing & Master Reproducibility

This chapter covers the multi-tiered verification infrastructure in **NeuroParse**, including C++ unit tests, Python unit and property tests, adversarial robustness fuzzing, and the one-command reproducibility runner.

---

## 1. Multi-Tier Verification Matrix

| Level | Framework / Tool | Location | Coverage Target |
|---|---|---|---|
| **C++ Native Core** | Custom C++ Test Harness (`test_harness.hpp`) | [`tests/unit/`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/tests/unit/) | Lexer, Parser, AST, Validator, Applier, Controller, Fuzz Invariants |
| **Python ML & Dataset** | PyTest | [`tests/python/`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/tests/python/) | Dataset generation, schema, PyTorch model, Trainer, Metrics |
| **Property-Based Testing**| Hypothesis | [`tests/python/test_fuzz_property.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/tests/python/test_fuzz_property.py) | Arbitrary token stream permutations, invariant preservation |
| **Numerical Parity** | NumPy + ONNX Runtime | [`ml/export/onnx_exporter.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/export/onnx_exporter.py) | PyTorch vs. ONNX Runtime parity ($\le 10^{-5}$) |
| **Robustness Fuzzing** | Adversarial Fuzzing Runner | [`scripts/run_fuzz.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/scripts/run_fuzz.py) | 1,000 stochastic mutations; zero crashes/hangs/invariant violations |
| **IDE Client** | VS Code Test Suite | [`vscode-extension/src/test/`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/vscode-extension/src/test/) | Extension activation, diagnostic parsing, QuickFix application |
| **Master Orchestration**| Clean-Checkout Runner | [`scripts/reproduce.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/scripts/reproduce.py) | Full end-to-end clean build and audit validation |

---

## 2. C++ Unit Test Suite (`tests/unit/`)

All native C++ tests compile into a single high-speed test binary (`build/neuroparse_unit_tests.exe`):

- **`test_lexer.cpp`:** Tests tokenization of all 35 token types, identifiers, numbers, string escapes, comment skipping, and EOF position.
- **`test_parser.cpp`:** Tests valid declarations, statements, and nested Pratt expressions with precedence binding.
- **`test_ast.cpp`:** Verifies AST hierarchy, single ownership, node cloning, visitor dispatch, and ASCII tree printing.
- **`test_context_extractor.cpp`:** Tests sliding window slicing, boundary clamping, and format string generation.
- **`test_repair_validator.cpp`:** Rigorously evaluates all 6 invariant checks across valid repairs, out-of-bounds positions, unexpected tokens, and EOF protection.
- **`test_repair_applier.cpp`:** Verifies atomic insertion, deletion, replacement, and cursor realignment.
- **`test_recovery.cpp`:** Tests `ErrorRecoveryController`, strategy fallback, panic mode synchronization, and loop bounds.
- **`test_neural_recovery.cpp`:** Tests `NeuralRecoveryStrategy` using mock engines and rule-assisted candidate generators.
- **`test_fuzz.cpp`:** Validates invariant checking functions against malformed streams and ASTs.

---

## 3. Python Test Suite (`tests/python/`)

Run via `python -m pytest tests/python -v`:
- **`test_dataset.py`:** Validates sample schema, corruption generation, and dataset split integrity.
- **`test_model.py`:** Verifies PyTorch `RepairModel` parameter counts, tensor output dimensions, and multi-task loss computation.
- **`test_training.py`:** Tests training loop steps, AdamW optimizer updates, and checkpoint serialization.
- **`test_onnx_export.py`:** Verifies ONNX export wrapper, Opset 18 validity, and numerical parity checks.
- **`test_fuzz_property.py`:** Uses Hypothesis to generate thousands of random token sequences to ensure the parser never crashes or enters infinite loops.

---

## 4. Adversarial Robustness Fuzzing

The robustness fuzzer ([`scripts/run_fuzz.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/scripts/run_fuzz.py)) subjects the compiler to 1,000 stochastic adversarial mutations:

### Mutation Strategies:
1. **Token Drop:** Randomly deletes valid tokens.
2. **Token Duplicate:** Injects duplicates of adjacent tokens.
3. **Token Swap:** Inverts order of two adjacent tokens.
4. **Random Token Injection:** Inserts arbitrary tokens from vocabulary.
5. **Byte Noise:** Corrupts raw UTF-8 bytes to trigger lexer errors.

### Fuzzing Invariants Enforced:
```cpp
InvariantResult check_token_stream_invariants(const std::vector<lexer::Token>& tokens);
InvariantResult check_ast_invariants(const ast::ASTNode* root);
```
- Exactly 1 EOF at end of token stream.
- Monotonically non-decreasing token offsets.
- Non-inverted source spans ($start \le end$).
- Non-null AST pointers on constructed subtrees.
- **Results:** 1,000 iterations completed with **0 crashes, 0 hangs, and 0 invariant violations**.

---

## 5. Master Reproducibility Script (`reproduce.py`)

To verify the entire repository on a clean checkout in a single command:

```bash
python scripts/reproduce.py
```

### Automated Stages:
1. Environment & toolchain inspection (CMake, Ninja, Python, Node.js)
2. Native C++ configure & compilation
3. Native C++ unit tests execution
4. Python unit & property test execution
5. VS Code extension compilation & tests
6. Dataset schema and zero-leakage hash validation
7. ONNX export & numerical parity check ($\le 10^{-5}$)
8. Empirical benchmark evaluation across test set
9. Robustness fuzzer smoke test
10. End-to-end CLI demonstration and verification report
