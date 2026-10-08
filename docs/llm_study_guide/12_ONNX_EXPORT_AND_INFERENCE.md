# Chapter 12: ONNX Export, Numerical Parity & C++ Inference

This chapter details the ONNX export pipeline, dynamic shape configuration, numerical parity verification, and C++ inference engine integration.

---

## 1. Why ONNX for Compiler Error Recovery?

1. **Native Performance:** Python runtime latency (> 100 ms) is unacceptable in an interactive parse loop. ONNX Runtime CPU execution runs in $\sim 1.19\text{ ms}$.
2. **Zero Python Runtime Dependency:** Compilers can be compiled as standalone native binaries (`neuroparse.exe`) without requiring Python, PyTorch, or virtual environments at runtime.
3. **Cross-Platform Portability:** ONNX models run uniformly across Linux, macOS, and Windows.

---

## 2. The `ONNXExportWrapper`

PyTorch's default `forward()` returns a dataclass (`RepairModelOutput`). ONNX export requires a flat tuple of raw output tensors. The wrapper is defined in [`ml/export/onnx_exporter.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/export/onnx_exporter.py):

```python
class ONNXExportWrapper(nn.Module):
    def __init__(self, model: RepairModel):
        super().__init__()
        self.model = model

    def forward(
        self,
        input_ids: torch.Tensor,
        attention_mask: torch.Tensor,
        segment_ids: torch.Tensor,
        error_positions: torch.Tensor,
    ) -> Tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
        output = self.model(
            input_ids=input_ids,
            attention_mask=attention_mask,
            segment_ids=segment_ids,
            error_positions=error_positions,
        )
        return output.operation_logits, output.token_logits, output.position_logits
```

---

## 3. Dynamic Axes & Export Specifications

The export command targets **ONNX Opset 18**:

```python
dynamic_axes = {
    "input_ids": {0: "batch_size", 1: "sequence_length"},
    "attention_mask": {0: "batch_size", 1: "sequence_length"},
    "segment_ids": {0: "batch_size", 1: "sequence_length"},
    "error_positions": {0: "batch_size"},
    "operation_logits": {0: "batch_size"},
    "token_logits": {0: "batch_size"},
    "position_logits": {0: "batch_size"},
}

torch.onnx.export(
    wrapper,
    dummy_inputs,
    "models/repair_model.onnx",
    opset_version=18,
    input_names=["input_ids", "attention_mask", "segment_ids", "error_positions"],
    output_names=["operation_logits", "token_logits", "position_logits"],
    dynamic_axes=dynamic_axes,
)
```

---

## 4. Numerical Parity Verification

Before deployment, `ml/export/onnx_exporter.py` verifies numerical equivalence between native PyTorch evaluation and ONNX Runtime evaluation:

- **Tolerances:** Maximum absolute difference $\le 1 \times 10^{-5}$ across all logit outputs.
- **Tested Test Cases:**
  1. Batch size 1, sequence length 16
  2. Batch size 1, sequence length 64
  3. Batch size 1, sequence length 128
  4. Batch size 4, sequence length 32
  5. Batch size 8, sequence length 64
  6. Empty/minimal sequence (length 2)
- **Result:** $100\%$ prediction argmax agreement; max abs diff $< 3.2 \times 10^{-6}$.

---

## 5. C++ Inference Engine Interface

In the C++ compiler ([`compiler/include/neuroparse/recovery/neural_recovery.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/neural_recovery.hpp)), inference is abstracted via `NeuralInferenceEngine`:

```cpp
class NeuralInferenceEngine {
public:
  virtual ~NeuralInferenceEngine() = default;
  [[nodiscard]] virtual bool is_ready() const noexcept = 0;
  [[nodiscard]] virtual std::optional<RepairPrediction> predict(const ErrorContext& context) = 0;
  [[nodiscard]] virtual std::string_view engine_name() const noexcept = 0;
};
```

### Implementations:
1. **`RuleAssistedInferenceEngine`:** Built-in candidate generator using high-accuracy syntactic priors for zero-dependency standalone execution.
2. **`MockInferenceEngine`:** Injected during unit tests to test edge cases (validator rejections, corrupted predictions, confidence gating).
3. **`ONNXInferenceEngine`:** Loads `models/repair_model.onnx` via the ONNX Runtime C++ API and executes CPU tensor evaluation.
