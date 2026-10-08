# Model Architecture

## 1. Model Objective

NeuroParse's neural repair model (`RepairModel`) maps localized compiler syntax context (`ErrorContext`) directly into structured repair operations (`RepairCandidate`).

It does **not** generate arbitrary text or source code tokens, but rather solves a multi-task classification problem:
$$P(\text{Operation}, \text{Token}, \text{Position} \mid \text{ErrorContext})$$

## 2. Input Representation

The model operates strictly on local parser context extracted during syntax failure:
- `tokens_before`: Window of tokens preceding the diagnostic point.
- `error_token` (`current_token`): Erroneous/unexpected token or EOF.
- `tokens_after`: Window of tokens immediately following the diagnostic point.
- `expected_tokens`: Grammar terminal expectation set computed by the parser.
- `construct`: Syntactic construct hint (e.g. `let binding`, `function definition`).

Sequence Layout:
```
[BOS] + tokens_before + [ERROR] + current_token + tokens_after + [EOS] + [PAD]...
```
Segment role IDs (`1=BEFORE, 2=CURRENT/ERROR, 3=AFTER, 0=PAD`) are added as role embeddings.

## 3. Vocabulary Design

The vocabulary (`ml/models/vocabulary.py`) consists of 42 deterministic tokens:
- Special Tokens (5): `<PAD>`, `<UNK>`, `<BOS>`, `<EOS>`, `<ERROR>`
- Keywords (9): `function`, `let`, `if`, `else`, `while`, `return`, `true`, `false`, `null`
- Delimiters (8): `(`, `)`, `{`, `}`, `;`, `,`, `[`, `]`
- Operators (15): `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `=`, `!`, `&&`, `||`
- Normalized Literal Categories (5): `IDENTIFIER`, `INTEGER`, `FLOAT`, `STRING`, `EOF`

Arbitrary identifiers and literals are normalized to their category token to prevent vocabulary explosion.

## 4. Model Architecture

The `RepairModel` is a compact Transformer encoder architecture:
```
Input IDs + Position Embeddings + Segment Embeddings
                       ↓
                   LayerNorm & Dropout
                       ↓
         Transformer Encoder (2 Layers, 4 Heads)
                       ↓
           Context Pooling (at Error Token Index)
                       ↓
      ┌────────────────┬─────────────────┬────────────────┐
      │                │                 │                │
Operation Head     Token Head      Position Head     Confidence
 (4 classes)      (42 classes)     (128 offsets)     Computation
```

## 5. Prediction Heads

1. **Operation Head (`OperationHead`)**: Multi-class classification over `{INSERT_TOKEN, DELETE_TOKEN, REPLACE_TOKEN, NO_REPAIR}`.
2. **Token Head (`TokenHead`)**: Multi-class classification predicting which vocabulary token to insert or replace.
3. **Position Head (`PositionHead`)**: Classification predicting the target sequence offset for the edit.

## 6. Confidence Representation

Confidence is calculated deterministically from model probability distributions:
$$\text{Confidence} = \max P(\text{Operation}) \times \max P(\text{Token})$$
Confidence represents internal model certainty and is used by the `RepairValidator` threshold gate. It is not an unverified guarantee of correctness.

## 7. Multi-Task Loss Formulation

$$\mathcal{L} = \lambda_{\text{op}} \mathcal{L}_{\text{op}} + \lambda_{\text{tok}} \mathcal{L}_{\text{tok}} + \lambda_{\text{pos}} \mathcal{L}_{\text{pos}}$$
- $\mathcal{L}_{\text{op}}$: CrossEntropyLoss over repair operations.
- $\mathcal{L}_{\text{tok}}$: CrossEntropyLoss over vocabulary tokens (masked for operations where tokens are not applicable).
- $\mathcal{L}_{\text{pos}}$: CrossEntropyLoss over position offsets.
- Default weights: $\lambda_{\text{op}} = 1.0, \lambda_{\text{tok}} = 1.0, \lambda_{\text{pos}} = 0.5$.

## 8. Configuration & Model Size Metrics

Configuration (`ml/models/config.json`):
- `vocab_size`: 42
- `embedding_dim`: 64
- `num_layers`: 2
- `num_heads`: 4
- `feed_forward_dim`: 256
- `max_sequence_length`: 128
- `dropout`: 0.1

### Parameter Counts & Memory Footprint
- **Total Parameters:** 123,214
- **Trainable Parameters:** 123,214
- **Approximate FP32 Memory:** 0.47 MB
- **Approximate FP16 Memory:** 0.235 MB

## 9. Why the Model is Intentionally Small
1. **Low-Latency In-Process Execution:** The model is designed to run locally inside the compiler parse loop on CPU without noticeable compilation delay.
2. **Grammar Locality:** Syntax errors are predominantly localized phenomena that do not require billion-parameter general-purpose LLMs.
3. **Zero Cloud Dependencies:** The model must run completely offline without remote network round-trips.

## 10. Training & Evaluation Note
Model training and baseline evaluation on the synthetic dataset were executed in Phase 10 (`docs/TRAINING.md`).

## 11. ONNX Export & Inference Specification
In Phase 11, the trained PyTorch `RepairModel` is exported to ONNX (Opset 18) with dynamic sequence length and dynamic batch dimensions (`artifacts/onnx/repair_model.onnx`). Full numerical equivalence and prediction parity on CPU inference with ONNX Runtime are verified in `docs/ONNX.md`.

