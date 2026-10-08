# NeuroParse Experimental Benchmark Artifact Index (Phase 18)

This document indexes every empirical experiment executed across the NeuroParse research lifecycle, including datasets, model checkpoints, ONNX graphs, execution commands, output artifacts, and actual measured results.

> **Research Integrity Standard:** Every number in this document was directly obtained from executed test suites, evaluation scripts, and benchmark runners. Zero metrics are fabricated or projected.

---

## 1. Artifact Checksum Index

| Artifact Description | Path | Size (Bytes) | SHA-256 Checksum |
|---|---|---|---|
| **Synthetic Training Split** | `ml/dataset/generated/train.jsonl` | 277,259 | `dd5469e28ebdc21a2e399f3a5e8203b66d8f3579516230ae5505c796978f0fb2` |
| **Synthetic Validation Split** | `ml/dataset/generated/validation.jsonl` | 93,440 | `c8cc69782ebf16987bb6414019e1046872487bf6b0a22fba68ceae23e9a21f87` |
| **Held-Out Test Split** | `ml/dataset/generated/test.jsonl` | 72,368 | `84fdffc6330de5a9a3590a546dd22b41a6d827df5fb02a5982dd0c1f72de2ce0` |
| **Dataset Metadata** | `ml/dataset/generated/metadata.json` | 442 | `abc867f6325c0ea2e0d8b603179cdbed232e5f225b2d53091e9f46e727a23188` |
| **Trained Best Checkpoint** | `artifacts/training/checkpoints/best_checkpoint.pt` | 1,530,119 | `c0366d23f9ecece21cba3b428dcd84a5601fe453541a7017e7a3a851de8fd08e` |
| **ML Evaluation Metrics** | `artifacts/training/test_evaluation.json` | 2,040 | `6b7c1831e2c428012d82004cc2b49bf0d6dd01089982ff20fe90af3aee08f2fd` |
| **Exported ONNX Model** | `models/repair_model.onnx` | 341,492 | `fb384e325e2f984abcdefaee35acf66a23d450a07f313a26b9cadfc080074d77` |
| **ONNX Weight Data** | `models/repair_model.onnx.data` | 488,704 | (External tensor weight store) |
| **Phase 15 Benchmark JSON** | `benchmarks/results/phase15_results.json` | 150,013 | `574a53ca4a2e7fae22d41d5aaa46e76e6876d1894120401657ce5e81d1b458cd` |
| **Phase 17 Fuzz Results** | `benchmarks/results/phase17_fuzz_results.json` | 438 | `3feca50d95aa4d8e6f789fb4efc3e50ab42bde4bfaed6df09d01ee48ee74972a` |

---

## 2. Experiment 1: Synthetic Dataset Generation & Partitioning (Phase 8)

- **Purpose:** Construct syntactically verified, deterministic error corruptions from valid NeuroLang programs with strict data leakage prevention.
- **Input Corpus:** `examples/valid/*.nl` (6 source programs: `hello.nl`, `expressions.nl`, `control_flow.nl`, `functions.nl`, `nested.nl`, `fibonacci.nl`).
- **Generation Command:**
  ```bash
  python -m ml.dataset.generate --input-dir examples/valid --output-dir ml/dataset/generated --seed 42 --samples-per-file 30
  ```
- **Verification Command:**
  ```bash
  python -m ml.dataset.validate --dataset ml/dataset/generated
  ```
- **Output Artifacts:** `ml/dataset/generated/train.jsonl`, `validation.jsonl`, `test.jsonl`, `metadata.json`, `report.json`.
- **Key Metrics & Verified Results:**
  - **Total Generated Samples:** 176
  - **Train Split:** 110 samples (from 4 source programs)
  - **Validation Split:** 36 samples (from 1 source program)
  - **Test Split:** 30 samples (from 1 source program)
  - **Train-Test Overlap:** 0 (Strictly disjoint)
  - **Rejection of Invalid Base Programs:** 100% verified.

---

## 3. Experiment 2: Neural Repair Model Training & Evaluation (Phase 10)

- **Purpose:** Train multi-head Transformer encoder (`RepairModel`) on `ErrorContext` representations and evaluate classification and exact repair accuracy.
- **Configuration:**
  - `vocab_size`: 42
  - `embedding_dim`: 64
  - `num_layers`: 2
  - `num_heads`: 4
  - `feed_forward_dim`: 256
  - `max_sequence_length`: 128
  - `dropout`: 0.1
  - `epochs`: 21 (Early stopping triggered, patience 8)
  - `batch_size`: 16
  - `learning_rate`: 0.003 (AdamW)
- **Execution Command:**
  ```bash
  python -m ml.training.train --dataset ml/dataset/generated --output-dir artifacts/training --epochs 25 --batch-size 16 --lr 0.003 --seed 42
  ```
- **Evaluation Command:**
  ```bash
  python -m ml.training.evaluate --checkpoint artifacts/training/checkpoints/best_checkpoint.pt --dataset ml/dataset/generated --split test
  ```
- **Output Artifacts:** `artifacts/training/checkpoints/best_checkpoint.pt`, `artifacts/training/test_evaluation.json`, `training_history.json`.
- **Actual Measured Test Split Results (30 Samples):**
  - **Exact Structured Repair Accuracy:** **30.00%**
  - **Operation Head Accuracy:** **50.00%** (Macro F1: 0.2883, Weighted F1: 0.4713)
  - **Token Head Accuracy:** **41.67%**
  - **Position Head Accuracy:** **86.67%**
  - **Invalid Prediction Rate:** **0.00%** (Zero out-of-grammar operation classes)
  - **Mean Confidence (Correct):** 0.5200 vs **Mean Confidence (Incorrect):** 0.4343
  - **Exact Repair by Category:**
    - `missing_closing_brace`: 75.0% (3/4)
    - `missing_comma`: 50.0% (1/2)
    - `unexpected_token`: 33.3% (2/6)
    - `missing_closing_paren`: 25.0% (1/4)
    - `missing_semicolon`: 16.7% (1/6)
    - `replace_delimiter`: 12.5% (1/8)

---

## 4. Experiment 3: ONNX Export & Parity Verification (Phase 11)

- **Purpose:** Export PyTorch checkpoint to ONNX Opset 18 format with dynamic batch and sequence dimensions; verify numerical parity on CPU inference.
- **Export Command:**
  ```bash
  python -m ml.export.export_onnx --checkpoint artifacts/training/checkpoints/best_checkpoint.pt --output artifacts/onnx/repair_model.onnx --opset 18
  ```
- **Verification Command:**
  ```bash
  python -m pytest tests/python/test_onnx_export.py -v
  ```
- **Output Artifact:** `artifacts/onnx/repair_model.onnx` (and `models/repair_model.onnx`).
- **Actual Measured Numerical Parity Results:**
  - **Operation Logits Max Absolute Difference:** $7.0 \times 10^{-7}$ (Tolerance: $10^{-4}$)
  - **Token Logits Max Absolute Difference:** $1.9 \times 10^{-6}$ (Tolerance: $10^{-4}$)
  - **Position Logits Max Absolute Difference:** $2.9 \times 10^{-6}$ (Tolerance: $10^{-4}$)
  - **Argmax Prediction Parity:** **100.0% match** across all dynamic test shapes.

---

## 5. Experiment 4: End-to-End Compiler Benchmark Suite (Phase 15)

- **Purpose:** Compare recovery success, cascading error reduction, AST validity, and execution latency across 5 compiler recovery modes on the held-out test split.
- **Evaluated Modes:** `None`, `Panic`, `Heuristic`, `Neural`, `Auto`.
- **Execution Command:**
  ```bash
  python scripts/run_benchmark.py --dataset ml/dataset/generated/test.jsonl --output benchmarks/results/phase15_results.json
  ```
- **Output Artifacts:** `benchmarks/results/phase15_results.json`, `benchmarks/results/phase15_report.md`.
- **Actual Measured Benchmark Results (30 Held-Out Test Samples):**

### Quality & Recovery Performance
| Metric | None | Panic | Heuristic (Baseline) | NeuroParse (Neural) | NeuroParse (Auto) |
|---|---|---|---|---|---|
| **Parse Success Rate** | 0.0% (0/30) | 60.0% (18/30) | 70.0% (21/30) | **96.7% (29/30)** | **96.7% (29/30)** |
| **Recovery Success Rate** | 0.0% (0/30) | 86.7% (26/30) | 100.0% (30/30) | **100.0% (30/30)** | **100.0% (30/30)** |
| **AST Structural Validity** | 3.3% (1/30) | 30.0% (9/30) | 63.3% (19/30) | **53.3% (16/30)** | **53.3% (16/30)** |
| **Remaining Syntax Errors** | 35 | 13 | 14 | **2** | **2** |
| **Cascading Error Reduction vs Heuristic** | - | - | 0.0% (ref) | **-85.7%** | **-85.7%** |
| **Validator Acceptance Rate** | - | - | - | **100.0% (30/30)** | **100.0% (30/30)** |

### Latency Profiling (Intel x86_64, Windows 11)
| Component / Metric | Measured Value |
|---|---|
| **Compiler Parse Mean (Heuristic)** | 16,116.00 µs (16.12 ms) |
| **Compiler Parse Mean (Neural)** | 16,008.65 µs (16.01 ms, $-0.7\%$ overhead) |
| **ONNX CPU Cold Start Latency** | 77,706.30 µs (77.71 ms) |
| **ONNX CPU Warm Inference Mean** | **1,191.37 µs (1.19 ms)** |
| **ONNX CPU Warm Inference Median** | **1,152.85 µs (1.15 ms)** |
| **ONNX CPU Warm Inference P95** | **1,500.30 µs (1.50 ms)** |
| **Mean ONNX Model Confidence** | **0.8817** |

---

## 6. Experiment 5: Compiler Robustness & Fuzz Testing (Phase 17)

- **Purpose:** Stress test all parser, lexer, context extractor, and recovery invariants under adversarial token sequences, byte mutations, and pathological nesting.
- **Execution Command:**
  ```bash
  python scripts/run_fuzz.py --seed 42 --iterations 1000 --corpus tests/fuzz/corpus --timeout 2.0 --output benchmarks/results/phase17_fuzz_results.json
  ```
- **Output Artifact:** `benchmarks/results/phase17_fuzz_results.json`.
- **Actual Measured Fuzzing Results:**
  - **Total Iterations:** 1,000
  - **Completed Cases:** 1,000 (100%)
  - **Valid Parses:** 189
  - **Clean Diagnostic Rejections:** 811
  - **Crashes:** **0**
  - **Hangs / Timeouts:** **0**
  - **Invariant Violations:** **0**
  - **Duration:** 24.05s
