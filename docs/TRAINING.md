# Model Training & Evaluation Pipeline

## 1. Overview

Phase 10 implements the offline training and evaluation pipeline for NeuroParse's PyTorch `RepairModel` using the synthetic datasets generated in Phase 8.

The pipeline establishes measurable baseline learning dynamics on localized compiler syntax context (`ErrorContext`).

> [!IMPORTANT]
> The model remains an offline research component in Phase 10. No native compiler integration or ONNX runtime embedding is active yet.

## 2. Dataset Pipeline & Leakage Prevention

- Datasets are loaded directly from `ml/dataset/generated/` (`train.jsonl`, `validation.jsonl`, `test.jsonl`).
- Split partitions are preserved strictly from Phase 8.
- Samples are grouped by source file (`source_file`), guaranteeing zero data leakage between train, validation, and test splits.

### Dataset Distribution (176 Total Samples)
- **Train split**: 110 samples
- **Validation split**: 36 samples
- **Test split**: 30 samples

## 3. Training Architecture & Loss

### Optimizer & Regularization
- **Optimizer:** AdamW (`lr = 0.003`, `weight_decay = 1e-4`)
- **Gradient Clipping:** Maximum gradient norm $\le 1.0$
- **Early Stopping:** Configurable patience (default: 8 epochs on validation exact repair accuracy)
- **Device Support:** CPU-first execution, automatic CUDA acceleration if available.

### Multi-Task Loss Formulation
$$\mathcal{L} = \lambda_{\text{op}} \mathcal{L}_{\text{op}} + \lambda_{\text{tok}} \mathcal{L}_{\text{tok}} + \lambda_{\text{pos}} \mathcal{L}_{\text{pos}}$$
- $\mathcal{L}_{\text{op}}$: CrossEntropyLoss over repair operations (`INSERT_TOKEN`, `DELETE_TOKEN`, `REPLACE_TOKEN`, `NO_REPAIR`).
- $\mathcal{L}_{\text{tok}}$: CrossEntropyLoss over vocabulary tokens (masked for `DELETE_TOKEN` / `NO_REPAIR`).
- $\mathcal{L}_{\text{pos}}$: CrossEntropyLoss over sequence offsets.

## 4. Checkpointing

Checkpoints are saved to `artifacts/training/checkpoints/`:
- `best_checkpoint.pt`: Checkpoint achieving the highest validation exact repair accuracy.
- `latest_checkpoint.pt`: Checkpoint from the most recent training epoch.
- Checkpoints store `model_state_dict`, `optimizer_state_dict`, `epoch`, `best_metric`, `config`, and `seed`.

## 5. Evaluation Metrics

1. **Operation Accuracy & F1:** Per-class precision, recall, F1, macro F1, and weighted F1 over repair operations.
2. **Token Accuracy:** Accuracy on predicting the correct vocabulary token (evaluated on samples where token prediction is applicable: `INSERT_TOKEN`, `REPLACE_TOKEN`).
3. **Position Accuracy:** Accuracy on predicting the correct local sequence offset.
4. **Exact Structured Repair Accuracy:** Percentage of samples where the complete tuple `(operation, token, position)` matches the ground truth.
5. **Invalid Prediction Rate:** Percentage of candidate predictions that violate grammar/structural validity.
6. **Confidence Metrics:** Average model softmax confidence across correct vs. incorrect predictions.

## 6. Actual Experimental Results (Phase 10 Run)

### Training Dynamics (21 Epochs, Early Stopping)
- **Initial Training Loss:** `6.7565`
- **Final Training Loss:** `1.1364`
- **Best Validation Exact Repair Accuracy:** `19.44%` (Epoch 13)

### Final Evaluation on Unseen Test Split (30 Samples)
- **Exact Structured Repair Accuracy:** `30.00%`
- **Operation Accuracy:** `50.00%` (Macro F1: `0.2883`)
- **Token Accuracy:** `41.67%`
- **Position Accuracy:** `86.67%`
- **Invalid Prediction Rate:** `0.00%` (100% of predictions were syntactically valid operations)
- **Mean Confidence (Correct):** `0.5200` vs. **Mean Confidence (Wrong):** `0.4343`

### Performance by Corruption Category (Test Split)
| Corruption Type | Sample Count ($N$) | Operation Accuracy | Token Accuracy | Exact Repair Accuracy |
| :--- | :--- | :--- | :--- | :--- |
| `missing_closing_brace` | 4 | 100.0% | 75.0% | **75.0%** |
| `missing_comma` | 2 | 100.0% | 50.0% | **50.0%** |
| `unexpected_token` | 6 | 33.3% | 100.0% | **33.3%** |
| `missing_closing_paren` | 4 | 25.0% | 50.0% | **25.0%** |
| `missing_semicolon` | 6 | 83.3% | 33.3% | **16.7%** |
| `replace_delimiter` | 8 | 12.5% | 25.0% | **12.5%** |

## 7. CLI Usage

```powershell
# Train model with early stopping and automatic test evaluation
python -m ml.training.train --dataset ml/dataset/generated --output-dir artifacts/training --epochs 25 --batch-size 16 --lr 0.003 --seed 42

# Evaluate trained checkpoint on test split
python -m ml.training.evaluate --checkpoint artifacts/training/checkpoints/best_checkpoint.pt --dataset ml/dataset/generated --split test --show-errors 10
```
