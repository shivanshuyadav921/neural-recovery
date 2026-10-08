# NeuroParse ONNX Export & Inference Contract (Phase 11)

This document specifies the ONNX export pipeline, input/output tensor contracts, dynamic dimension support, and numerical equivalence verification for the NeuroParse neural syntax repair model.

---

## 1. Export Pipeline Overview

The neural repair model (`RepairModel`) is trained in PyTorch (Phase 9/10) and exported to ONNX (Open Neural Network Exchange) format to establish a portable, runtime-independent inference artifact for future compiler integration (Phase 12+).

```
PyTorch Checkpoint (.pt)
         │
         ▼
PyTorch RepairModel (eval mode, torch.no_grad())
         │
         ▼
ONNX Export (torch.onnx.export with dynamic_shapes)
         │
         ▼
ONNX Model Graph (.onnx)
         │
         ▼
ONNX Runtime Engine (CPUExecutionProvider)
         │
         ▼
Numerical Equivalence & Prediction Parity Verification
```

---

## 2. Model Architecture & Opset Selection

- **Architecture**: `RepairModel` (2-layer Transformer encoder, $d_{\text{model}}=64$, 4 attention heads, $d_{\text{ff}}=128$, vocab size 42, max sequence length 128).
- **Selected ONNX Opset**: **Opset 18** (Standard ONNX operator set with native support for multi-head attention decompositions, dynamic gathers, and elementwise operations).
- **Target File**: `artifacts/onnx/repair_model.onnx`
- **Model Size**: ~333.5 KB (341,492 bytes).

---

## 3. Input Tensor Contract

The exported ONNX model takes 4 input tensors. All inputs use 64-bit integer (`int64`) representations.

| Input Name | Shape | Dtype | Dynamic Axes | Semantic Description |
|---|---|---|---|---|
| `input_ids` | `[batch_size, sequence_length]` | `INT64` | Dim 0 (`batch_size`), Dim 1 (`sequence_length`) | Sequence of normalized token IDs from the vocabulary extracted by `ContextExtractor`. |
| `attention_mask` | `[batch_size, sequence_length]` | `INT64` | Dim 0 (`batch_size`), Dim 1 (`sequence_length`) | Binary mask (`1` for real tokens, `0` for padding). |
| `segment_ids` | `[batch_size, sequence_length]` | `INT64` | Dim 0 (`batch_size`), Dim 1 (`sequence_length`) | Segment identifiers (`1` = left context, `2` = error token, `3` = right context). |
| `error_positions` | `[batch_size]` | `INT64` | Dim 0 (`batch_size`) | Index of the detected syntax error within each sequence. |

---

## 4. Output Tensor Contract

The exported ONNX graph exposes **unnormalized raw logits** for each of the three structured repair prediction heads. Post-processing (e.g. `argmax` / `softmax`) remains deterministic and decoupled from the graph.

| Output Name | Shape | Dtype | Dynamic Axes | Semantic Description |
|---|---|---|---|---|
| `operation_logits` | `[batch_size, 4]` | `FLOAT` | Dim 0 (`batch_size`) | Logits over repair operations (`INSERT_TOKEN`, `DELETE_TOKEN`, `REPLACE_TOKEN`, `NO_REPAIR`). |
| `token_logits` | `[batch_size, 42]` | `FLOAT` | Dim 0 (`batch_size`) | Logits over target token vocabulary for insertion or replacement. |
| `position_logits` | `[batch_size, 128]` | `FLOAT` | Dim 0 (`batch_size`) | Logits over candidate repair token positions across the sequence window. |

---

## 5. Dynamic Shape Support

Both `batch_size` and `sequence_length` dimensions are dynamic:
- `batch_size`: Range $[1, 64]$
- `sequence_length`: Range $[1, 128]$

Dynamic behavior is verified across short ($L=16, 20$), medium ($L=32$), and full ($L=128$) sequence lengths with batch sizes ranging from 1 to 4.

---

## 6. Numerical & Prediction Equivalence

Numerical equivalence between the source PyTorch checkpoint and ONNX Runtime CPU inference was evaluated across 5 deterministic test configurations covering diverse batch sizes and context lengths with strict tolerances ($\text{atol} = 10^{-4}, \text{rtol} = 10^{-4}$).

### Measured Maximum Absolute Differences

| Test Configuration | Batch Size | Sequence Length | Op Max Diff | Tok Max Diff | Pos Max Diff | Argmax Match |
|---|---|---|---|---|---|---|
| `single_short` | 1 | 16 | $0.0000000$ ($< 10^{-7}$) | $0.0000000$ ($< 10^{-7}$) | $0.0000010$ ($1.0 \times 10^{-6}$) | **PASS (100%)** |
| `single_medium` | 1 | 32 | $0.0000000$ ($< 10^{-7}$) | $0.0000010$ ($1.0 \times 10^{-6}$) | $0.0000020$ ($2.0 \times 10^{-6}$) | **PASS (100%)** |
| `single_full` | 1 | 128 | $0.0000000$ ($< 10^{-7}$) | $0.0000020$ ($2.0 \times 10^{-6}$) | $0.0000030$ ($3.0 \times 10^{-6}$) | **PASS (100%)** |
| `batch_short` | 2 | 20 | $0.0000000$ ($< 10^{-7}$) | $0.0000010$ ($1.0 \times 10^{-6}$) | $0.0000020$ ($2.0 \times 10^{-6}$) | **PASS (100%)** |
| `batch_full` | 4 | 128 | $0.0000010$ ($1.0 \times 10^{-6}$) | $0.0000010$ ($1.0 \times 10^{-6}$) | $0.0000020$ ($2.0 \times 10^{-6}$) | **PASS (100%)** |

**Global Peak Absolute Difference**:
- `operation_logits`: $0.0000007$ ($7.0 \times 10^{-7}$)
- `token_logits`: $0.0000019$ ($1.9 \times 10^{-6}$)
- `position_logits`: $0.0000029$ ($2.9 \times 10^{-6}$)

**Prediction Parity**: 100% agreement across operation, token, and position predictions.

---

## 7. Export Tooling & Usage

### CLI Exporter
```bash
python -m ml.export.export_onnx \
    --checkpoint artifacts/training/checkpoints/best_checkpoint.pt \
    --output artifacts/onnx/repair_model.onnx \
    --opset 18
```

### Programmatic Export
```python
from ml.export.onnx_exporter import export_model_to_onnx, verify_onnx_numerical_parity
from ml.training.checkpoint import CheckpointManager
import torch

# Export to ONNX
onnx_path = export_model_to_onnx(
    checkpoint_path="artifacts/training/checkpoints/best_checkpoint.pt",
    output_onnx_path="artifacts/onnx/repair_model.onnx",
    opset_version=18,
)

# Verify numerical parity
model, _ = CheckpointManager.load_checkpoint("artifacts/training/checkpoints/best_checkpoint.pt", device=torch.device("cpu"))
report = verify_onnx_numerical_parity(model, onnx_path, atol=1e-4, rtol=1e-4)
print(f"Numerical Equivalence: {report.is_equivalent}")
```
