# NeuroParse LLM Study Guide & Knowledge Base

Welcome to the **NeuroParse** Master LLM Study Guide. This document serves as the high-level roadmap and knowledge index for Large Language Models (LLMs) and developers studying, navigating, analyzing, or extending the NeuroParse codebase.

---

## 1. Executive Summary & Thesis

**NeuroParse** is an experimental research compiler that integrates a compact, task-specific neural network ($123\text{k}$ parameters, $341.5\text{ KB}$ ONNX) directly into a native C++20 recursive-descent parsing pipeline.

### The Central Thesis
Traditional compiler error recovery mechanisms (panic-mode token skipping or heuristic delimiter insertion) frequently introduce spurious secondary errors (cascading errors) or discard valid code. NeuroParse explores the hypothesis:
> *Can an embedded, compact neural model predict structured syntactic repairs within milliseconds without sacrificing compiler determinism or compilation speed?*

### The Fundamental Invariant
> **"AI proposes, compiler validates, parser recovers."**
- **Untrusted Candidate Generator:** The neural model is strictly an untrusted candidate proposer. It never mutates compiler state or AST nodes directly.
- **Deterministic Gatekeeper:** The `RepairValidator` deterministically validates all neural proposals against grammar invariants and parser expectations before any mutation occurs.
- **Fail-Safe Fallbacks:** If the neural proposal fails validation or inference is unavailable, the compiler deterministically falls back to heuristic or panic-mode recovery.

---

## 2. Quick Facts & Metrics

| Dimension | Specification / Metric |
|---|---|
| **Language Target** | NeuroLang (statically scoped, C-family experimental language) |
| **Compiler Implementation** | Modern C++20 (`neuroparse_core` library + `neuroparse` binary) |
| **Neural Architecture** | 2-layer Transformer Encoder + 3 Multi-Task Classification Heads |
| **Model Size** | $123,214$ parameters ($341.5\text{ KB}$ ONNX) |
| **Inference Runtime** | In-process ONNX Runtime (CPU Execution Provider) |
| **Inference Latency** | $\sim 1.19\text{ ms}$ CPU mean warm latency |
| **Test Set Success Rate** | **$96.7\%$** ($29/30$ held-out programs recovered to valid AST) |
| **Cascading Error Reduction** | **$-85.7\%$** compared to standard heuristic baseline |
| **Robustness Fuzzing** | $1,000$ adversarial mutations tested with **0 crashes, 0 hangs, 0 invariant violations** |
| **Tooling & IDE** | Visual Studio Code extension with live diagnostics & QuickFix code actions |

---

## 3. Directory Map & Component Index

```text
neurology/
├── LLM_STUDY_GUIDE.md               # [THIS FILE] Master index for LLMs
├── docs/llm_study_guide/            # Complete 16-chapter deep-dive LLM curriculum
│   ├── 01_ARCHITECTURE_AND_PIPELINE.md
│   ├── 02_NEUROLANG_GRAMMAR_AND_AST.md
│   ├── 03_LEXER_AND_TOKEN_SYSTEM.md
│   ├── 04_PARSER_IMPLEMENTATION.md
│   ├── 05_ERROR_RECOVERY_CONTROLLER.md
│   ├── 06_CONTEXT_EXTRACTION.md
│   ├── 07_REPAIR_VALIDATOR_AND_APPLIER.md
│   ├── 08_CLASSICAL_FALLBACKS.md
│   ├── 09_NEURAL_MODEL_ARCHITECTURE.md
│   ├── 10_DATASET_AND_CORRUPTION.md
│   ├── 11_TRAINING_AND_METRICS.md
│   ├── 12_ONNX_EXPORT_AND_INFERENCE.md
│   ├── 13_CLI_AND_STRUCTURED_DIAGNOSTICS.md
│   ├── 14_VSCODE_EXTENSION.md
│   ├── 15_TESTING_AND_FUZZING.md
│   └── 16_LLM_CHEATSHEET_AND_API_REFERENCE.md
├── compiler/                        # Native C++20 Compiler
│   ├── include/neuroparse/          # Public C++ headers
│   │   ├── ast/                     # Typed AST, SourceSpan, Visitors, ASTPrinter
│   │   ├── cli/                     # CLI options and runners
│   │   ├── fuzz/                    # Invariant validators for fuzzing
│   │   ├── lexer/                   # Lexer, Token, TokenType, SourceLocation
│   │   ├── parser/                  # Recursive descent + Pratt parser
│   │   └── recovery/                # Controller, Validator, Applier, Heuristics, Panic, Neural
│   └── src/                         # C++ implementation files
├── ml/                              # PyTorch & ONNX Machine Learning Subsystem
│   ├── corruption/                  # 6 synthetic syntax corruption strategies
│   ├── dataset/                     # Generator, Schema, Splitter, Validator
│   ├── export/                      # ONNX Exporter, Wrapper, Numerical Parity Checker
│   ├── models/                      # RepairModel, Config, Heads, Vocabulary
│   └── training/                    # Trainer, Metrics, Loss, CheckpointManager
├── tests/                           # Verification Suites
│   ├── unit/                        # C++ unit tests (Phase 1–18)
│   ├── python/                      # Pytest + Hypothesis property tests
│   └── fuzz/                        # Seed corpus & regression files
├── vscode-extension/                # Visual Studio Code Language Extension
└── scripts/                         # Build, Benchmark, Fuzzing, and Reproducibility runners
```

---

## 4. The 16 Deep-Dive Study Chapters

Click on any chapter below to explore the detailed architectural specifications, data structures, algorithms, and invariants:

1. [**Chapter 1: Architecture & Pipeline**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/01_ARCHITECTURE_AND_PIPELINE.md)  
   *End-to-end compiler lifecycle, Authority Boundaries, dataflow diagram, subsystem interactions.*

2. [**Chapter 2: NeuroLang Grammar & AST**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/02_NEUROLANG_GRAMMAR_AND_AST.md)  
   *Complete language specification, EBNF syntax, keywords, operators, Pratt precedence rules, AST hierarchy, memory management, visitor pattern.*

3. [**Chapter 3: Lexer & Token System**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/03_LEXER_AND_TOKEN_SYSTEM.md)  
   *`Token`, `TokenType` enum (35 variants), `SourceLocation`, `SourceSpan`, scanner state machine, token categories, invariants.*

4. [**Chapter 4: Parser Implementation**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/04_PARSER_IMPLEMENTATION.md)  
   *Recursive-descent parser, Pratt expression parser, cursor movement, error generation, hook into recovery controller.*

5. [**Chapter 5: Error Recovery Controller**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/05_ERROR_RECOVERY_CONTROLLER.md)  
   *Orchestrator of recovery, strategy chain dispatch, termination guarantees ($K=50$, same-pos threshold), telemetry and metrics.*

6. [**Chapter 6: Context Extraction**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/06_CONTEXT_EXTRACTION.md)  
   *Local token windowing ([-50, +10]), `ErrorContext` struct, clamping, context extraction CLI.*

7. [**Chapter 7: Repair Validator & Applier**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/07_REPAIR_VALIDATOR_AND_APPLIER.md)  
   *6 validation passes, non-mutating checks, atomic token applier, cursor adjustment.*

8. [**Chapter 8: Classical Fallback Strategies**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/08_CLASSICAL_FALLBACKS.md)  
   *Panic-mode synchronization (statement boundaries & starters), heuristic delimiter insertion, cascading fallback guarantees.*

9. [**Chapter 9: Neural Model Architecture**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/09_NEURAL_MODEL_ARCHITECTURE.md)  
   *PyTorch `RepairModel` architecture, embeddings, 2-layer Transformer encoder, 3 prediction heads, multi-task loss formulation.*

10. [**Chapter 10: Dataset & Corruption Pipeline**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/10_DATASET_AND_CORRUPTION.md)  
    *Synthetic dataset generation, 6 corruption strategies, JSONL schema, data splits, zero leakage guarantee.*

11. [**Chapter 11: Training & Evaluation**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/11_TRAINING_AND_METRICS.md)  
    *Training loop, hyperparameters, checkpointing, loss curves, evaluation metrics, cascading error reduction calculations.*

12. [**Chapter 12: ONNX Export & C++ Inference**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/12_ONNX_EXPORT_AND_INFERENCE.md)  
    *ONNX wrapper, dynamic axes, Opset 18, parity check ($\le 10^{-5}$), C++ `NeuralInferenceEngine` interface, runtime integration.*

13. [**Chapter 13: CLI & Structured Diagnostics**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/13_CLI_AND_STRUCTURED_DIAGNOSTICS.md)  
    *CLI interface, flags, human-readable recovery reports, traces, and JSON diagnostic schema 1.0.0.*

14. [**Chapter 14: Visual Studio Code Extension**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/14_VSCODE_EXTENSION.md)  
    *Extension architecture, diagnostic provider, debounce, QuickFix CodeActions, configuration options.*

15. [**Chapter 15: Testing, Fuzzing & Verification**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/15_TESTING_AND_FUZZING.md)  
    *C++ unit tests, Python unit and property tests, invariant checking, robustness fuzzer, master reproduction runner.*

16. [**Chapter 16: LLM Quick Reference & Cheat Sheet**](file:///c:/Users/shiva/OneDrive/Desktop/neurology/docs/llm_study_guide/16_LLM_CHEATSHEET_AND_API_REFERENCE.md)  
    *Fast symbol and API lookup, file paths, common tasks, debugging recipes, and key invariants checklist.*

---

## 5. End-to-End Pipeline Visualization

```
[ Source Code: .nl ]
         │
         ▼
[ Lexer ] ─────────────► std::vector<Token> (Monotonic offsets, 1 EOF)
         │
         ▼
[ Parser ] ────────────► On Syntax Error: Construct ParserError
         │                        │
         │                        ▼
         │             [ ContextExtractor ] ──► Local Window ([-50, +10] tokens)
         │                        │
         │                        ▼
         │          [ NeuralInferenceEngine ] ──► RepairPrediction (op, tok, pos, conf)
         │                        │
         │                        ▼
         │              [ RepairValidator ] ──► 6-Check Deterministic Gate
         │                  │           │
         │           ACCEPT │           │ REJECT
         │                  ▼           ▼
         │           [RepairApplier] [Classical Fallback: Heuristic / Panic]
         │                  │           │
         │                  └─────┬─────┘
         │                        │
         ▼                        ▼
[ Parser Resumption ] ◄───────────┘
         │
         ├──► Valid Complete AST (ast::Program)
         │
         ▼
[ Diagnostics Engine ] ──► CLI Human Reports / Structured JSON / VS Code QuickFix
```

---

## 6. How LLMs Should Read and Use This Repository

When an LLM is answering questions or implementing changes in NeuroParse:
1. **Never bypass `RepairValidator`:** The neural network never has authority to modify tokens or AST directly. Any new repair operator must be enforced in the validator.
2. **Follow RAII and Smart Pointers:** AST nodes strictly use `std::unique_ptr` with transfer of ownership.
3. **Respect Invariant Bounds:** Token stream indices must remain in bounds; the parser must maintain progress guarantees ($K \le 50$, consecutive attempts $\le 3$).
4. **Refer to Chapter 16 for Quick Symbol Lookups:** Check struct fields, method prototypes, and CLI flags directly.
