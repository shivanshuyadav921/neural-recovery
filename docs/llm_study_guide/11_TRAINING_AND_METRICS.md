# Chapter 11: Model Training, Evaluation & Metrics

This chapter covers the training pipeline in **NeuroParse**, optimization hyperparameters, checkpoint management, and formal mathematical evaluation metrics.

---

## 1. Training Engine (`Trainer`)

Implemented in [`ml/training/trainer.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/training/trainer.py), the `Trainer` orchestrates reproducible training runs:

```python
class Trainer:
    def __init__(
        self,
        model: RepairModel,
        train_dataset: NeuroLangDataset,
        val_dataset: Optional[NeuroLangDataset] = None,
        output_dir: Union[str, Path] = "artifacts/training",
        learning_rate: float = 1e-3,
        weight_decay: float = 1e-4,
        epochs: int = 20,
        batch_size: int = 16,
        gradient_clip: float = 1.0,
        early_stopping_patience: int = 5,
        seed: int = 42,
    ): ...
```

### Optimization Details:
- **Optimizer:** `torch.optim.AdamW`
- **Initial Learning Rate:** $1\times 10^{-3}$ (or $3\times 10^{-3}$ with warmup)
- **Weight Decay:** $1\times 10^{-4}$ (L2 regularization)
- **Gradient Clipping:** Norm clipped to $1.0$ (`torch.nn.utils.clip_grad_norm_`)
- **Batch Size:** $16$
- **Early Stopping:** Tracks validation loss with patience of $5$ epochs

---

## 2. Checkpointing (`CheckpointManager`)

The `CheckpointManager` ([`ml/training/checkpoint.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/training/checkpoint.py)) saves model state and metadata:
- Saves `best_checkpoint.pt` whenever validation loss improves.
- Persists model weights, optimizer state, epoch count, configuration parameters, and vocabulary mapping.
- Automatically exports training curves to `training_history.json`.

---

## 3. Evaluation Metrics

Evaluated by [`ml/training/metrics.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/training/metrics.py):

### 1. Head-Level Accuracies
- **Operation Accuracy ($Acc_{\text{op}}$):**
  $$Acc_{\text{op}} = \frac{1}{N} \sum_{i=1}^N \mathbb{I}(\hat{y}_i^{\text{op}} = y_i^{\text{op}})$$
- **Token Accuracy ($Acc_{\text{tok}}$):**
  $$Acc_{\text{tok}} = \frac{1}{N} \sum_{i=1}^N \mathbb{I}(\hat{y}_i^{\text{tok}} = y_i^{\text{tok}})$$
- **Position Accuracy ($Acc_{\text{pos}}$):**
  $$Acc_{\text{pos}} = \frac{1}{N} \sum_{i=1}^N \mathbb{I}(\hat{y}_i^{\text{pos}} = y_i^{\text{pos}})$$

### 2. Exact Match Accuracy ($Acc_{\text{exact}}$)
A prediction is counted as an exact match if and only if **all three heads** predict the ground truth simultaneously:
$$Acc_{\text{exact}} = \frac{1}{N} \sum_{i=1}^N \mathbb{I}(\hat{y}_i^{\text{op}} = y_i^{\text{op}} \;\land\; \hat{y}_i^{\text{tok}} = y_i^{\text{tok}} \;\land\; \hat{y}_i^{\text{pos}} = y_i^{\text{pos}})$$

---

## 4. Benchmark & Cascading Error Metrics

Defined in [`ml/evaluation/benchmark_metrics.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/evaluation/benchmark_metrics.py):

### 1. Parse Success Rate
$$\text{ParseSuccessRate} = \frac{\text{Programs Parsed to Valid AST}}{\text{Total Test Programs}} \times 100\%$$
- **No Recovery:** $0.0\%$ ($0/30$)
- **Panic Mode:** $60.0\%$ ($18/30$)
- **Heuristic Baseline:** $70.0\%$ ($21/30$)
- **NeuroParse (Neural):** **$96.7\%$** ($29/30$)

### 2. Cascading Error Reduction ($\Delta_{\text{cascade}}$)
Measures the reduction in spurious secondary errors introduced by recovery compared to a baseline:
$$\Delta_{\text{cascade}} = \frac{E_{\text{strategy}} - E_{\text{baseline}}}{E_{\text{baseline}}} \times 100\%$$
- **NeuroParse vs. Heuristic Baseline:** **$-85.7\%$** reduction (only 2 residual errors vs. 14 in heuristic).

---

## 5. Training CLI Commands

```bash
# 1. Train the model
python -m ml.training.train --dataset ml/dataset/generated --output-dir artifacts/training --epochs 25 --lr 0.003

# 2. Evaluate checkpoint on test split
python -m ml.training.evaluate --checkpoint artifacts/training/checkpoints/best_checkpoint.pt --dataset ml/dataset/generated --split test
```
