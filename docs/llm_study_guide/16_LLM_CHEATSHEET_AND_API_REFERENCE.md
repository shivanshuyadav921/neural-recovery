# Chapter 16: LLM Quick Reference & API Cheat Sheet

This chapter is a compact, high-density reference sheet designed for LLMs and developers to instantly look up symbols, file locations, extension recipes, and key architectural invariants.

---

## 1. Symbol & Subsystem Lookup Table

| Symbol Name | Kind | Header File | Source File | Description |
|---|---|---|---|---|
| `Token` | struct | [`lexer/token.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/lexer/token.hpp) | [`lexer/token.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/lexer/token.hpp) | Token record (`type`, `lexeme`, `location`, `length`). |
| `TokenType` | enum | [`lexer/token.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/lexer/token.hpp) | [`lexer/token.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/lexer/token.hpp) | 35 lexical token types. |
| `Lexer` | class | [`lexer/lexer.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/lexer/lexer.hpp) | [`lexer/lexer.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/lexer/lexer.cpp) | Scans text into tokens; tracks source coordinates. |
| `ASTNode` | class | [`ast/ast.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/ast/ast.hpp) | [`ast/ast.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/ast/ast.hpp) | Abstract base class for all typed AST nodes. |
| `ASTVisitor` | class | [`ast/ast_visitor.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/ast/ast_visitor.hpp) | - | Visitor pattern interface for AST traversal. |
| `ASTPrinter` | class | [`ast/ast_printer.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/ast/ast_printer.hpp) | [`ast/ast_printer.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/ast/ast_printer.cpp) | Serializes AST into ASCII tree hierarchy. |
| `Parser` | class | [`parser/parser.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/parser/parser.hpp) | [`parser/parser.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/parser/parser.cpp) | Recursive descent + Pratt expression parser. |
| `ParseResult` | struct | [`parser/parser.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/parser/parser.hpp) | [`parser/parser.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/parser/parser.hpp) | Output of parser (`program`, `errors`, `recoveries`). |
| `ContextExtractor` | class | [`recovery/context_extractor.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/context_extractor.hpp) | [`recovery/context_extractor.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/recovery/context_extractor.cpp) | Slices $[-50, +10]$ local token window. |
| `ErrorContext` | struct | [`recovery/error_context.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/error_context.hpp) | [`recovery/error_context.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/recovery/error_context.cpp) | Context data passed to neural predictor. |
| `RepairPrediction`| struct | [`recovery/repair_candidate.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/repair_candidate.hpp) | - | Predicted repair (`operation`, `token`, `pos`, `conf`). |
| `RepairValidator` | class | [`recovery/repair_validator.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/repair_validator.hpp) | [`recovery/repair_validator.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/recovery/repair_validator.cpp) | 6-stage non-mutating deterministic verification gate. |
| `RepairApplier` | class | [`recovery/repair_applier.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/repair_applier.hpp) | [`recovery/repair_applier.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/recovery/repair_applier.cpp) | Atomically patches token stream & updates cursor. |
| `ErrorRecoveryController`| class | [`recovery/error_recovery_controller.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/error_recovery_controller.hpp) | [`recovery/error_recovery_controller.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/recovery/error_recovery_controller.cpp) | Orchestrator managing strategy dispatch and loops. |
| `PanicModeRecovery`| class | [`recovery/panic_mode.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/panic_mode.hpp) | [`recovery/panic_mode.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/recovery/panic_mode.cpp) | Skips tokens to `;` or statement starter. |
| `HeuristicRecovery`| class | [`recovery/heuristic_recovery.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/recovery/heuristic_recovery.hpp) | [`recovery/heuristic_recovery.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/recovery/heuristic_recovery.cpp) | Rule-based insertion of `;`, `)`, `}`, `,`. |
| `RepairModel` | PyTorch Module | [`ml/models/repair_model.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/models/repair_model.py) | [`ml/models/repair_model.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/models/repair_model.py) | 2-layer Transformer + 3 multi-task heads ($123\text{k}$ params). |
| `Trainer` | Python Class | [`ml/training/trainer.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/training/trainer.py) | [`ml/training/trainer.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/training/trainer.py) | Reproducible PyTorch training loop & evaluator. |
| `ONNXExportWrapper`| PyTorch Module | [`ml/export/onnx_exporter.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/export/onnx_exporter.py) | [`ml/export/onnx_exporter.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/export/onnx_exporter.py) | Formats model logits for Opset 18 ONNX export. |

---

## 2. Common Extension Recipes

### Recipe 1: Adding a New Token Type
1. Add enumerator to `enum class TokenType` in [`token.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/lexer/token.hpp).
2. Update `token_category()` and `to_string(TokenType)` in [`token.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/lexer/token.hpp).
3. Add scanning pattern in `Lexer::tokenize()` in [`lexer.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/lexer/lexer.cpp).
4. Update Python vocabulary in [`ml/models/vocabulary.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/models/vocabulary.py).

### Recipe 2: Adding a New AST Node
1. Declare node class inheriting from `Statement` or `Expression` in [`ast.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/ast/ast.hpp).
2. Add `ASTNodeKind` enumerator in [`ast.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/ast/ast.hpp).
3. Add `visit(*this)` methods in `ASTVisitor` and `ConstASTVisitor` in [`ast_visitor.hpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/include/neuroparse/ast/ast_visitor.hpp).
4. Implement formatting in `ASTPrinter::visit(...)` in [`ast_printer.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/ast/ast_printer.cpp).
5. Add recursive descent parsing rule in [`parser.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/parser/parser.cpp).

### Recipe 3: Adding a Synthetic Corruption Operator
1. Create a class inheriting from `CorruptionStrategy` in [`ml/corruption/corruptions.py`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/ml/corruption/corruptions.py).
2. Implement `find_candidates()`, `apply()`, and `apply_repair()`.
3. Register the new strategy in `CORRUPTION_REGISTRY`.
4. Re-run dataset generation: `python -m ml.dataset.generate --input-dir examples/valid --output-dir ml/dataset/generated --seed 42`.

---

## 3. The 7 Golden Invariants for LLMs

When modifying or analyzing NeuroParse, **never violate these rules**:

1. **AI Proposes, Compiler Validates:** The neural network never has authority to modify tokens or AST directly. Any candidate must be checked by `RepairValidator`.
2. **Never Delete EOF:** The terminating `TokenType::Eof` token must never be deleted, replaced, or skipped.
3. **Monotonic Offsets:** Token byte offsets must be strictly non-decreasing.
4. **Single-Ownership AST:** All subtrees are owned through `std::unique_ptr<T>`. Never duplicate pointers or use manual `delete`.
5. **Progress Guarantees:** Recovery attempts at the exact same token index are strictly capped at 3; total attempts per compilation unit are capped at 50.
6. **Graceful Fallback:** If ONNX runtime fails or model confidence is low, fall back to heuristic/panic recovery without throwing an uncaught exception.
7. **Zero Leakage:** Never train or evaluate on overlapping samples between train and test datasets.
