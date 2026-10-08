# NeuroParse

**Neural Error Recovery & Context-Aware AI Embedded in the Compiler Parsing Pipeline for Intelligent Diagnostics and Code Correction**

NeuroParse is an experimental research compiler that embeds a lightweight, task-specific neural repair model ($123\text{k}$ parameters) directly into the native C++ recursive-descent parsing pipeline. Operating under the strict invariant **"AI proposes, compiler validates, parser recovers,"** NeuroParse extracts localized syntactic context upon syntax failure, predicts discrete structured edits (`INSERT_TOKEN`, `DELETE_TOKEN`, `REPLACE_TOKEN`), deterministically validates the proposal against grammar invariants, and safely resumes parsing to eliminate cascading errors.

---

> [!TIP]
> **LLM & AI Study Guide:** For an LLM or developer studying the architecture and codebase, see [LLM_STUDY_GUIDE.md](file:///c:/Users/shiva/OneDrive/Desktop/neurology/LLM_STUDY_GUIDE.md) and the comprehensive 16-chapter curriculum in [docs/llm_study_guide/](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/).

---

## Quick Start (Clean Checkout)

To build the native C++ compiler, run the full validation suite (C++ unit tests, 61 Python tests, VS Code extension tests), audit the dataset, verify ONNX parity, and run the end-to-end pipeline demonstration in one step:

```bash
python scripts/reproduce.py
```

Or step-by-step:

```bash
# 1. Build C++ native compiler & unit tests
python scripts/build.py

# 2. Run C++ unit tests
./build/neuroparse_unit_tests.exe

# 3. Run Python unit & property tests
python -m pytest tests/python -v

# 4. Try the compiler CLI on valid code
./build/neuroparse.exe examples/valid/hello.nl --dump-ast

# 5. Try the compiler on broken code with neural recovery and step trace
./build/neuroparse.exe examples/syntax_errors/missing_semicolon.nl --trace-recovery
```

---

## Research Motivation & Problem

Traditional compiler error recovery mechanisms (panic-mode synchronization, heuristic delimiter insertion) often discard valid tokens or introduce inappropriate delimiters, triggering cascades of spurious secondary errors. Conversely, applying large language models (LLMs) to compiler error correction introduces heavy latency ($> 500\text{ ms}$), hallucinations, and zero integration with compiler state.

**The NeuroParse Research Question:** Can a compact, local, offline neural model embedded within the compiler parse loop reduce cascading syntax errors without sacrificing compilation speed or compromising compiler determinism?

---

## Core Idea: "AI Proposes, Compiler Validates"

The neural model is treated strictly as an **untrusted candidate generator**. It never mutates compiler state or AST memory directly.

```
Source Code
    │
    ▼
[ Lexer ] ──► Token Stream
    │
    ▼
[ Parser ] ──► Syntax Error Encountered
    │                  │
    │                  ▼
    │       [ ContextExtractor ] ──► Local Token Window ([-50, +10])
    │                  │
    │                  ▼
    │      [ NeuralInferenceEngine ] ──► Repair Candidate Prediction
    │                  │
    │                  ▼
    │         [ RepairValidator ] ──► Deterministic Invariant Check
    │             │          │
    │       ACCEPT│          │REJECT
    │             ▼          ▼
    │      [RepairApplier] [Deterministic Fallback]
    │             │          │
    │             └────┬─────┘
    │                  │
    ▼                  ▼
[ Parser Resumption & Typed AST Construction ]
    │
    ▼
[ Diagnostics & IDE QuickFix Output ]
```

---

## Key Results (Empirically Verified)

All metrics below are measured directly on the 30 held-out test samples (`ml/dataset/generated/test.jsonl`) with **0% data leakage**:

| Strategy / Mode | Parse Success Rate | Remaining Syntax Errors | Cascading Error Reduction | Warm Inference Mean Latency |
|---|---|---|---|---|
| **None (No Recovery)** | 0.0% (0/30) | 35 errors | 0.0% (ref) | N/A |
| **Panic-Mode** | 60.0% (18/30) | 13 errors | -62.9% | N/A |
| **Heuristic Baseline** | 70.0% (21/30) | 14 errors | 0.0% (baseline) | N/A |
| **NeuroParse (Neural / Auto)** | **96.7% (29/30)** | **2 errors** | **-85.7%** | **1.19 ms (CPU)** |

- **Robustness Fuzzing:** 1,000 adversarial mutations tested with **0 crashes**, **0 hangs**, and **0 invariant violations**.
- **ONNX Model Size:** 341.5 KB ($123,214$ parameters).

---

## Technology Stack

- **Native Compiler Engine:** Modern C++20 (`neuroparse_core` library and `neuroparse` binary).
- **Build System:** CMake 3.20+, Ninja build tool.
- **Machine Learning & Training:** Python 3.11+, PyTorch, NumPy.
- **Model Format & Inference:** ONNX Opset 18 with ONNX Runtime CPU execution provider.
- **Testing & Verification:** Custom C++ test harness, PyTest, Hypothesis property-based testing.
- **IDE Client:** Visual Studio Code extension (TypeScript, Node.js).

---

## Repository Structure

```text
neuroparse/
├── CMakeLists.txt                # Root CMake C++20 build definition
├── LICENSE                       # MIT License
├── README.md                     # Project overview and quick start
├── THIRD_PARTY_NOTICES.md        # Open-source third-party dependencies
├── artifacts/
│   ├── onnx/                     # Exported ONNX model & weights
│   └── training/                 # Model checkpoints & training logs
├── benchmarks/
│   └── results/                  # Structured benchmark JSON and reports
├── compiler/
│   ├── include/neuroparse/       # C++ public headers (lexer, parser, ast, recovery, fuzz)
│   └── src/                      # C++ implementations (cli, lexer, parser, recovery, ast)
├── docs/                         # Detailed architecture, model, and research reports
│   ├── ARCHITECTURE.md           # Compiler pipeline and authority boundaries
│   ├── BENCHMARKS.md             # Benchmark evaluation and empirical tables
│   ├── DESIGN.md                 # NeuroLang grammar and design specifications
│   ├── EXPERIMENTS.md            # Experimental artifact index and checksums
│   ├── FUZZING.md                # Robustness testing and fuzzer invariants
│   ├── INTERFACES.md             # Formal subsystem interface contracts
│   ├── MODEL.md                  # Neural architecture and multi-task loss
│   ├── ONNX.md                   # ONNX export contract and numerical parity
│   ├── PROJECT_STATUS.md         # Subsystem status matrix (Phases 1–18)
│   ├── RECOVERY.md               # Controller, validator, and applier specs
│   ├── RESEARCH_REPORT.md        # 31-section thesis-ready research report
│   └── TRAINING.md               # Training dataset, loss, and dynamics
├── examples/
│   ├── valid/                    # Confirmed valid NeuroLang programs (.nl)
│   └── syntax_errors/            # Test programs with specific syntax corruptions
├── ml/
│   ├── dataset/                  # Dataset generator, schema, splitter, validator
│   ├── models/                   # PyTorch RepairModel, vocabulary, configuration
│   ├── training/                 # Training loop, loss functions, checkpoint manager
│   └── export/                   # ONNX export and parity verification CLI
├── models/                       # Deployment ONNX models (repair_model.onnx)
├── scripts/                      # Reproducibility, build, test, and benchmark scripts
│   ├── build.py / build.ps1      # C++ build scripts
│   ├── reproduce.py              # Master clean-checkout reproducibility runner
│   ├── run_benchmark.py          # Benchmark evaluation runner
│   └── run_fuzz.py               # Robustness fuzzer runner
├── tests/
│   ├── unit/                     # C++ unit tests (lexer, parser, recovery, ast, fuzz)
│   ├── python/                   # Python unit, property, and integration tests
│   └── fuzz/                     # Fuzzing seed corpus and regression cases
└── vscode-extension/             # VS Code extension source, syntaxes, and tests
```

---

## Build & Test Instructions

### Prerequisites
- C++20 compliant compiler: GCC 11+, Clang 12+, or MSVC 2019+
- Python 3.11+
- CMake 3.20+ and Ninja (`python -m pip install cmake ninja pytest`)
- Node.js 18+ and npm (for VS Code extension)

### Building the Native Compiler
```bash
python scripts/build.py
```

### Running Tests
```bash
# C++ Unit Tests (All phases)
./build/neuroparse_unit_tests.exe

# Python Tests (Dataset, Model, Training, ONNX, Property tests)
python -m pytest tests/python -v

# VS Code Extension Tests
cd vscode-extension && npm test && cd ..
```

---

## Compiler CLI Usage

```text
Usage:
  neuroparse <file.nl>                                      # Compile / check syntax
  neuroparse --dump-tokens <file.nl>                        # Dump lexical token stream
  neuroparse --dump-ast <file.nl>                           # Print typed AST hierarchy
  neuroparse --dump-context <file.nl>                       # Extract error context window
  neuroparse --explain-recovery <file.nl>                   # Print aggregate recovery report
  neuroparse --trace-recovery <file.nl>                     # Step-by-step recovery trace
  neuroparse --recovery=<auto|neural|heuristic|panic|none>  # Select recovery strategy
  neuroparse --diagnostics=json <file.nl>                   # Structured JSON diagnostics
  neuroparse --version                                      # Display compiler version
  neuroparse --help                                         # Display help information
```

---

## Machine Learning Pipeline

### 1. Synthetic Dataset Generation
```bash
python -m ml.dataset.generate --input-dir examples/valid --output-dir ml/dataset/generated --seed 42
python -m ml.dataset.validate --dataset ml/dataset/generated
```

### 2. Model Training & Evaluation
```bash
python -m ml.training.train --dataset ml/dataset/generated --output-dir artifacts/training --epochs 25 --lr 0.003
python -m ml.training.evaluate --checkpoint artifacts/training/checkpoints/best_checkpoint.pt --dataset ml/dataset/generated --split test
```

### 3. ONNX Model Export & Parity Verification
```bash
python -m ml.export.export_onnx --checkpoint artifacts/training/checkpoints/best_checkpoint.pt --output models/repair_model.onnx --opset 18
```

---

## Benchmarking & Robustness Fuzzing

```bash
# Run full empirical benchmark suite
python scripts/run_benchmark.py

# Run robustness fuzzer (1,000 iterations)
python scripts/run_fuzz.py --iterations 1000 --seed 42
```

---

## VS Code Extension

The extension contributes `.nl` language support, syntax highlighting, live compiler diagnostics, and automated Quick Fix code actions.

```bash
cd vscode-extension
npm install
npm run compile
npm test
```

---

## Limitations

1. **Frontend Language:** Currently targets the experimental NeuroLang v1 grammar.
2. **Context Window:** Fixed sequence length of 64 tokens.
3. **Semantic Analysis:** Does not currently perform semantic type checking or borrow checking.

---

## Project Status

NeuroParse has completed all 18 development and research phases:
- **Research-Demo Ready:** **YES**
- **Thesis / Report Ready:** **YES**
- **Production Ready:** **NO (Research Prototype)**

See [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) and [`docs/RESEARCH_REPORT.md`](docs/RESEARCH_REPORT.md) for full technical documentation.

---

## License

This project is licensed under the MIT License — see the [`LICENSE`](LICENSE) file for details. Third-party software notices are available in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
