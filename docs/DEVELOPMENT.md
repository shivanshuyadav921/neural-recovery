# NeuroParse Development & Engineering Workflow (Phase 18 Final)

This document provides developer guidelines, toolchain requirements, roadmap status, and verification procedures for the NeuroParse repository.

---

## 1. Toolchain & Environment Requirements

- **C++ Compiler:** Modern C++20 compiler required (GCC 11+, Clang 12+, or MSVC 2019+). MinGW GCC 6.3 is rejected by CMake.
- **CMake:** Version 3.20 or newer.
- **Ninja:** Recommended build generator.
- **Python:** Python 3.11+ (Python 3.13 tested).
- **Node.js:** Node.js 18+ and npm (for VS Code extension development).

---

## 2. Standard Build & Verification Commands

```bash
# Master clean-checkout reproducibility script (runs all 11 stages)
python scripts/reproduce.py

# Native C++ build
python scripts/build.py

# C++ Unit Tests (All phases)
./build/neuroparse_unit_tests.exe

# Python Unit & Property Tests
python -m pytest tests/python -v

# VS Code Extension Tests
cd vscode-extension && npm test && cd ..

# Robustness Fuzzer (1,000 iterations)
python scripts/run_fuzz.py --iterations 1000 --seed 42

# Benchmark Evaluation
python scripts/run_benchmark.py
```

---

## 3. Project Roadmap Completion Matrix

| Phase | Milestone | Primary Focus | Status |
|---|---|---|---|
| **1** | Scaffold | CMake C++20 build definition & basic CLI | **Complete** |
| **2** | Lexer | UTF-8 tokenizer with line/col tracking & EOF invariants | **Complete** |
| **3** | Parser | Recursive-descent statement & Pratt expression parser | **Complete** |
| **4** | AST | Typed AST hierarchy, visitor, and ASCII pretty-printer | **Complete** |
| **5** | Recovery Baselines | Panic-mode and heuristic delimiter recovery | **Complete** |
| **6** | Recovery Controller | Dispatcher with consecutive retry limits ($K=50$) | **Complete** |
| **7** | Context Extractor | Local token window extraction ($[-50, +10]$) | **Complete** |
| **8** | Synthetic Dataset | Deterministic corruption engine with 0% data leakage | **Complete** |
| **9** | Neural Model | Multi-head PyTorch Transformer encoder ($123\text{k}$ params) | **Complete** |
| **10** | Model Training | AdamW training, early stopping, exact repair tracking | **Complete** |
| **11** | ONNX Export | Export to Opset 18 with verified numerical parity | **Complete** |
| **12** | Native Inference | In-process C++ neural predictor execution | **Complete** |
| **13** | Repair Validator | 6-stage non-mutating deterministic validation gate | **Complete** |
| **14** | Safe Resumption | Atomic token patcher (`RepairApplier`) & parser resume | **Complete** |
| **15** | Benchmarking | Empirical comparison against baseline recovery | **Complete** |
| **16** | VS Code Extension | Diagnostics provider & Quick Fix code action provider | **Complete** |
| **17** | Fuzzing | 1,000 adversarial mutations; zero crashes/hangs | **Complete** |
| **18** | Final Polish | Reproducibility runner, frozen interfaces, thesis report | **Complete** |

---

## 4. Code Standards & Architecture Guidelines

- **RAII & Memory Safety:** Use smart pointers (`std::unique_ptr`) exclusively for AST nodes. Zero manual pointer deallocations.
- **Const Correctness:** Enforce immutable `const` references across all analysis and extraction pipelines (`ErrorContext`, `ParserError`).
- **Authority Invariant:** Machine learning outputs are untrusted data structures; only the compiler-side validator may authorize AST/token mutations.
