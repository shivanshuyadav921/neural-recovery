# NeuroParse Research Overview & Experimental Methodology (Phase 18 Final)

## 1. Abstract

NeuroParse embeds a lightweight, local, task-specific neural repair model ($123\text{k}$ parameters) inside the native parsing loop of a C++20 compiler. The model outputs structured, token-level edit operations (`INSERT_TOKEN`, `DELETE_TOKEN`, `REPLACE_TOKEN`). A multi-stage deterministic compiler validator gates every proposed modification before application. Experiments on held-out test splits evaluate recovery continuation, cascading error reduction, exact repair accuracy, and execution latency against classical panic-mode and heuristic baselines.

---

## 2. Problem Statement & Motivation

Conventional syntax error recovery mechanisms (panic-mode token skipping, phrase-level heuristic insertions) frequently fail to capture local grammatical context, resulting in cascading error reports that confuse developers and pollute IDE diagnostics. NeuroParse investigates whether an embedded, offline neural classifier—strictly gated by deterministic compiler validation—can reliably improve syntax recovery and suppress error cascades without incurring noticeable compilation delay.

---

## 3. Core Architectural Principle: "AI Proposes, Compiler Validates"

1. **Context Extraction:** When a syntax error occurs, the compiler captures a localized token window ($[-50, +10]$ tokens) along with grammar construct metadata.
2. **Neural Prediction:** A compact Transformer encoder predicts an edit operation, target vocabulary token, and relative sequence offset with an associated confidence score.
3. **Deterministic Safety Gating:** The `RepairValidator` evaluates candidate edits against 6 deterministic rules (bounds, token legality, parser expectation compatibility, structural safety, forward progress).
4. **Resumption:** If accepted, `RepairApplier` patches the token stream and the parser authoritatively resumes; if rejected or failed, the compiler falls back to deterministic recovery.

---

## 4. Synthetic Dataset Methodology

- **Corpus:** Syntactically verified valid NeuroLang base programs (`examples/valid/*.nl`).
- **Corruption Categories:** `missing_semicolon`, `missing_closing_paren`, `missing_closing_brace`, `missing_comma`, `replace_delimiter`, `unexpected_token`.
- **Round-Trip Validation:** Each sample is confirmed to produce a syntax error when parsed and to be 100% syntactically valid when the ground-truth inverse repair is applied.
- **Disjoint Partitioning:** 176 total samples split strictly by source file into **110 Train (4 programs)**, **36 Validation (1 program)**, and **30 Held-Out Test (1 program)**, guaranteeing 0% data leakage.

---

## 5. Summary of Real Empirical Results

All results below are directly measured on the 30 held-out test samples (`ml/dataset/generated/test.jsonl`):

### Overall Recovery Comparison
| System / Strategy | Parse Success Rate | Recovery Success Rate | AST Structural Validity | Remaining Errors | Cascading Error Reduction |
|---|---|---|---|---|---|
| **None (No Recovery)** | 0.0% (0/30) | 0.0% (0/30) | 3.3% (1/30) | 35 | 0.0% (ref) |
| **Panic-Mode** | 60.0% (18/30) | 86.7% (26/30) | 30.0% (9/30) | 13 | -62.9% |
| **Heuristic Baseline** | 70.0% (21/30) | 100.0% (30/30) | **63.3% (19/30)** | 14 | 0.0% (baseline) |
| **NeuroParse (Neural / Auto)** | **96.7% (29/30)** | **100.0% (30/30)** | 53.3% (16/30) | **2** | **-85.7%** |

### Latency Profile (ONNX Runtime CPU)
- **Cold Start Latency (Session Init + 1st inference):** 77.71 ms
- **Warm Steady-State Inference Mean:** **1.19 ms (1,191.37 µs)**
- **Warm Steady-State Inference Median:** **1.15 ms (1,152.85 µs)**
- **Warm Steady-State Inference P95:** **1.50 ms (1,500.30 µs)**
- **Mean Model Confidence:** **0.8817**

### Robustness Fuzzing (Phase 17)
- 1,000 adversarial mutations: **0 crashes**, **0 hangs**, **0 invariant violations**.

---

## 6. Limitations & Threats to Validity

1. **Synthetic vs. Real Developer Errors:** Benchmark data is syntactically generated. Real human typing errors may exhibit different token distributions.
2. **Context Length:** Fixed window of 64 tokens.
3. **Single Language Grammar:** Validated on NeuroLang v1. Generalization to larger grammars (e.g. C++, Rust) remains future research.
