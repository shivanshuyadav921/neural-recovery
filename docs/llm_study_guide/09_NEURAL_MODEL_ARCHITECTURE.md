# Chapter 9: Neural Model Architecture & Multi-Task Heads

This chapter details the neural architecture of **`RepairModel`**, its embedding representations, 2-layer Transformer encoder, 3 multi-task prediction heads, and multi-task loss formulation.

---

## 1. Model Overview & Design Philosophy

The neural repair model ([`ml/models/repair_model.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/models/repair_model.py)) is engineered to run **offline, in-process, and on CPU** with sub-2-millisecond latency.

### Specifications:
- **Total Parameters:** $123,214$ parameters ($123\text{k}$)
- **ONNX Model Size:** $341.5\text{ KB}$
- **Architecture:** Pre-LN Bidirectional Transformer Encoder with Multi-Task Prediction Heads
- **Inference Runtime:** ONNX Runtime CPU Execution Provider (zero GPU requirement)

---

## 2. Model Configuration (`ModelConfig`)

Defined in [`ml/models/config.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/models/config.py):

```python
@dataclass
class ModelConfig:
    vocab_size: int = 42
    embedding_dim: int = 64
    num_layers: int = 2
    num_heads: int = 4
    feed_forward_dim: int = 256
    dropout: float = 0.1
    max_sequence_length: int = 128
    num_operations: int = 4            # INSERT_TOKEN, DELETE_TOKEN, REPLACE_TOKEN, NO_REPAIR
    num_token_types: int = 42          # Vocabulary size
    learning_rate: float = 1e-3
    loss_weights: Dict[str, float] = field(
        default_factory=lambda: {
            "operation": 1.0,
            "token": 1.0,
            "position": 0.5,
        }
    )
```

---

## 3. Embedding Pipeline

The model encodes input tokens using three distinct embedding layers summed together:

```
input_ids      ──► TokenEmbedding(42, 64)       ──┐
positions      ──► PositionEmbedding(128, 64)    ──┼──► (+) ──► LayerNorm ──► Dropout
segment_ids    ──► SegmentEmbedding(4, 64)       ──┘
```

1. **Token Embeddings (`token_embeddings`):** Maps integer token IDs to 64-dimensional vectors. Padding token (`<PAD>`) is index 0.
2. **Positional Embeddings (`position_embeddings`):** Learns absolute positional encodings up to sequence length 128.
3. **Segment Role Embeddings (`segment_embeddings`):** Encodes the role of each token relative to the error location:
   - `0`: `PAD`
   - `1`: `BEFORE` (tokens preceding error)
   - `2`: `CURRENT/ERROR` (the token that triggered the syntax error)
   - `3`: `AFTER` (lookahead tokens)

---

## 4. Transformer Encoder Backbone

The core sequence representation is computed using a 2-layer Pre-LayerNorm Transformer:

```python
encoder_layer = nn.TransformerEncoderLayer(
    d_model=64,
    nhead=4,
    dim_feedforward=256,
    dropout=0.1,
    activation="gelu",
    batch_first=True,
    norm_first=True,    # Pre-LN architecture for training stability
)
self.transformer = nn.TransformerEncoder(encoder_layer, num_layers=2)
```

### Context Vector Extraction:
To form a single dense context vector $c \in \mathbb{R}^{64}$ for classification:
- If `error_positions` is provided: Gathers the hidden state vector directly at the error position index $k$:
  $$c = h_k$$
- If `error_positions` is omitted: Performs masked mean-pooling across all non-padded tokens.

---

## 5. Multi-Task Prediction Heads

The context vector $c$ is projected simultaneously into 3 task-specific prediction heads defined in [`ml/models/heads.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/models/heads.py):

```
                   ┌──► OperationHead ──► [batch, 4]   (INSERT, DELETE, REPLACE, NO_REPAIR)
Context Vector (c) ┼──► TokenHead     ──► [batch, 42]  (Vocabulary Token ID)
                   └──► PositionHead  ──► [batch, 128] (Relative Sequence Position)
```

Each head is constructed as a two-layer non-linear MLP with GELU activation and dropout:
$$\text{Head}(c) = W_2 \cdot \text{GELU}(\text{Dropout}(W_1 \cdot c + b_1)) + b_2$$

### Confidence Metric:
Model confidence is computed as the joint probability of the predicted operation and predicted token:
$$\text{confidence} = \max(P(\text{op})) \times \max(P(\text{tok}))$$

---

## 6. Multi-Task Loss Formulation

Training optimizes the joint loss function $\mathcal{L}_{\text{total}}$:

$$\mathcal{L}_{\text{total}} = \lambda_{\text{op}} \mathcal{L}_{\text{op}} + \lambda_{\text{tok}} \mathcal{L}_{\text{tok}} + \lambda_{\text{pos}} \mathcal{L}_{\text{pos}}$$

Where:
- $\lambda_{\text{op}} = 1.0$ (CrossEntropy over 4 operation classes)
- $\lambda_{\text{tok}} = 1.0$ (CrossEntropy over 42 token types, ignoring `<PAD>`)
- $\lambda_{\text{pos}} = 0.5$ (CrossEntropy over 128 sequence positions)
