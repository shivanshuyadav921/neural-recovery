# NeuroParse VS Code Extension

**NeuroParse** is an intelligent IDE client for the NeuroParse research compiler featuring AI-assisted syntax error recovery.

The extension integrates VS Code directly with the native C++ NeuroParse compiler frontend. The TypeScript extension acts purely as a presentation and orchestration layer; the native compiler remains strictly authoritative for parsing, validation, and repair proposal decisions.

---

## Architecture

```
VS Code Editor
   │
   ├── TextMate Grammar (Syntax Highlighting for *.nl)
   ├── Language Configuration (Brackets, Comments, Indentation)
   │
   ▼
TypeScript Extension (`vscode-extension/`)
   │
   │  [Spawns process with `--diagnostics=json`]
   ▼
Native C++ NeuroParse Compiler (`neuroparse.exe` / `neuroparse`)
   │
   ├── Lexer & Pratt Parser
   ├── ContextExtractor (Extracts token window surrounding error)
   ├── NeuralInferenceEngine (ONNX Runtime / Embedded model)
   ├── RepairValidator (Deterministic grammar & safety verification)
   └── RepairApplier (Structured token stream transformation)
   │
   │  [Emits structured JSON contract on stdout]
   ▼
VS Code Diagnostic Collection & QuickFix CodeActions
```

---

## Features

1. **NeuroLang Language Recognition (`*.nl`):**
   - Automatically associates `.nl` files with the NeuroLang language ID.
   - Configures bracket matching, auto-closing delimiter pairs, line comments (`//`), and block comments (`/* ... */`).

2. **TextMate Syntax Highlighting:**
   - Full semantic token classification matching the official NeuroLang lexical grammar (keywords, declarations, numbers, strings, operators, function calls).

3. **Compiler Diagnostics & Error Highlights:**
   - Real-time, debounced compiler invocations on type or on save.
   - Precise 0-indexed squiggly error ranges matching the C++ compiler's exact token locations.

4. **Neural Repair Suggestions & Explanations:**
   - Explains neural repair proposals directly in diagnostic tooltips and via the `NeuroParse: Explain Syntax Error` command.
   - Displays model strategy, action, confidence, repair description, and validation outcome.

5. **Interactive Quick Fix (`Ctrl+.` / `Cmd+.`):**
   - Safely exposes compiler-validated repairs as one-click VS Code Quick Fixes (e.g. `NeuroParse (Neural): Insert ';'`).
   - Includes **stale document protection**: edits are rejected if the file was modified after diagnostic calculation.

6. **Recovery Trace Inspection:**
   - Command `NeuroParse: View Recovery Trace for Current File` runs the compiler with `--trace-recovery` and streams step-by-step recovery logs to the output channel.

---

## Configuration Settings

Configure these options in VS Code Settings (`settings.json`):

| Setting | Default | Description |
|---|---|---|
| `neuroparse.compilerPath` | `""` (auto-detect) | Absolute path or workspace-relative path to `neuroparse` executable. |
| `neuroparse.recoveryMode` | `"auto"` | Error recovery strategy (`auto`, `neural`, `heuristic`, `panic`, `none`). |
| `neuroparse.runMode` | `"onType"` | When to run analysis (`onType`, `onSave`, `manual`). |
| `neuroparse.debounceDelay` | `300` | Milliseconds delay after typing before triggering compilation. |
| `neuroparse.timeoutMs` | `5000` | Process execution timeout in milliseconds. |
| `neuroparse.traceRecovery` | `false` | Enable verbose recovery trace logging in the output channel. |

---

## Contributed Commands

- `neuroparse.analyzeFile`: **NeuroParse: Analyze Current File**
- `neuroparse.explainError`: **NeuroParse: Explain Syntax Error Under Cursor**
- `neuroparse.traceRecovery`: **NeuroParse: View Recovery Trace for Current File**
- `neuroparse.setRecoveryMode`: **NeuroParse: Change Recovery Mode**
- `neuroparse.showOutput`: **NeuroParse: Show Output Channel**

---

## Compiler ↔ Extension JSON Schema Contract

The extension communicates with the compiler using the `--diagnostics=json` flag:

```json
{
  "schema_version": "1.0.0",
  "file": "example.nl",
  "success": false,
  "metrics": {
    "total_errors": 1,
    "recovered_errors": 1,
    "failed_recoveries": 0,
    "cascading_errors": 0,
    "recovery_rate": 1.0,
    "total_latency_us": 124.0
  },
  "diagnostics": [
    {
      "source": "parser",
      "severity": "error",
      "message": "Expected ';' after return statement.",
      "line": 6,
      "column": 1,
      "end_line": 6,
      "end_column": 2,
      "code": "SYN001",
      "construct": "return statement",
      "expected_tokens": [";"],
      "recovery": {
        "strategy": "neural",
        "action": "insert_token",
        "status": "RECOVERED",
        "description": "Validated neural repair applied: INSERT_TOKEN token=';' pos=12 conf=0.95",
        "latency_us": 124.0,
        "is_cascading": false,
        "suggested_edit": {
          "operation": "insert",
          "line": 6,
          "column": 1,
          "end_line": 6,
          "end_column": 1,
          "new_text": ";"
        }
      }
    }
  ]
}
```

---

## Building and Testing

```bash
cd vscode-extension

# Install dependencies
npm install

# Compile TypeScript
npm run compile

# Run extension unit tests
npm test
```
