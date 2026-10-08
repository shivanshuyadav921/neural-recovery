# Chapter 10: Dataset Generation & Corruption Pipeline

This chapter covers the synthetic dataset generation pipeline in **NeuroParse**, the 6 syntax corruption strategies, the JSONL schema, data splitting, and the zero-data-leakage verification protocol.

---

## 1. Generation Pipeline Architecture

Because real-world codebases rarely contain vast, aligned pairs of erroneous code and exact single-token compiler repairs, NeuroParse uses a **high-fidelity synthetic corruption generator** ([`ml/dataset/generator.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/dataset/generator.py)).

```
[ Valid NeuroLang Programs ] (examples/valid/*.nl)
             │
             ▼
      [ Lexical Scan ] ────────► Token Stream
             │
             ▼
   [ Corruption Strategy ] ────► Select candidate token & apply mutation
             │
             ▼
    [ Compiler Oracle ] ───────► Parse corrupted code via C++ compiler CLI
             │                   - Verify syntax failure occurs
             │                   - Capture compiler error & context
             ▼
     [ Dataset Sample ] ───────► JSONL Record (corrupted_source, repair, error, context)
             │
             ▼
    [ Splitter (80/10/10) ] ───► train.jsonl (240), val.jsonl (30), test.jsonl (30)
```

---

## 2. The 6 Corruption Strategies

Implemented in [`ml/corruption/corruptions.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/corruption/corruptions.py), each strategy inherits from `CorruptionStrategy` and produces an exact inverse `RepairLabel`:

| Strategy Name | Corruption Mechanism | Target Repair Operation | Example Repair |
|---|---|---|---|
| **`missing_semicolon`** | Removes statement terminating `;` | `INSERT_TOKEN` | `token=";", pos=k` |
| **`missing_closing_paren`**| Removes closing `)` in if/while/call | `INSERT_TOKEN` | `token=")", pos=k` |
| **`missing_closing_brace`**| Removes block closing `}` | `INSERT_TOKEN` | `token="}", pos=k` |
| **`missing_comma`** | Removes `,` in argument/parameter list | `INSERT_TOKEN` | `token=",", pos=k` |
| **`replace_delimiter`** | Replaces `)` or `}` with mismatched `]` | `REPLACE_TOKEN` | `token=")", pos=k` |
| **`unexpected_token`** | Injects extraneous duplicate delimiter (`;;`) | `DELETE_TOKEN` | `token=";", pos=k+1` |

---

## 3. The `DatasetSample` JSONL Schema

Defined in [`ml/dataset/schema.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/dataset/schema.py), every sample contains the complete end-to-end trace:

```json
{
  "sample_id": "sample_0042",
  "source_file": "examples/valid/binary_search.nl",
  "original_source": "function search(arr, target) { let left = 0; ... }",
  "corrupted_source": "function search(arr, target) { let left = 0 ... }",
  "corruption_type": "missing_semicolon",
  "repair": {
    "operation": "INSERT_TOKEN",
    "token": ";",
    "position": 12
  },
  "error": {
    "token_index": 12,
    "line": 1,
    "column": 45,
    "message": "expected ';'",
    "expected_tokens": [";"],
    "construct": "let binding"
  },
  "context": {
    "tokens_before": [...],
    "current_token": { "type": "KwLet", "lexeme": "let", "line": 2, "column": 3 },
    "tokens_after": [...],
    "expected_tokens": [";"],
    "construct": "let binding"
  }
}
```

---

## 4. Dataset Splits & Stratification

The dataset generator partitions generated records using deterministic hashing (`seed=42`):

- **Train Split (80%):** $240$ samples used for gradient updates.
- **Validation Split (10%):** $30$ samples used for early stopping and hyperparameter tuning.
- **Test Split (10%):** $30$ held-out samples used exclusively for benchmark reporting.

---

## 5. Zero Data Leakage Guarantee

To ensure empirical validity, NeuroParse strictly validates the test set:

1. **Source File Isolation:** Validated by `ml/dataset/validate.py`. The held-out test split is verified to have zero program overlap with the training set.
2. **Hash Collisions:** AST and token stream SHA-256 hashes of the test set are checked against train and validation sets.
3. **Audited Test Suite:** All 30 test cases are audited and tracked in [`scripts/audit_30_samples.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/scripts/audit_30_samples.py), confirming:
   - 0 duplicate prompts.
   - 0 test samples present in training checkpoints.
   - 100% compiler oracle verification (every corrupted sample triggers a real compiler syntax error).
