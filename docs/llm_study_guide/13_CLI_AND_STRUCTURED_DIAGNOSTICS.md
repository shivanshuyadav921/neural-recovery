# Chapter 13: Compiler CLI & Structured JSON Diagnostics

This chapter specifies the command-line interface of the native `neuroparse` binary, all execution flags, diagnostic formats, and the **v1.0.0 JSON schema** consumed by IDEs.

---

## 1. CLI Usage & Flags

Implemented in [`compiler/src/cli/cli.cpp`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/compiler/src/cli/cli.cpp):

```text
Usage:
  neuroparse <file.nl>                                      # Standard syntax check
  neuroparse --dump-tokens <file.nl>                        # Dump token stream
  neuroparse --dump-ast <file.nl>                           # Print typed AST tree
  neuroparse --dump-context <file.nl>                       # Extract error context window
  neuroparse --explain-recovery <file.nl>                   # Print aggregate recovery report
  neuroparse --trace-recovery <file.nl>                     # Step-by-step recovery trace
  neuroparse --recovery=<auto|neural|heuristic|panic|none>  # Select recovery strategy
  neuroparse --diagnostics=json <file.nl>                   # Structured JSON diagnostics
  neuroparse --version                                      # Compiler version
  neuroparse --help                                         # Help documentation
```

---

## 2. Human-Readable Diagnostic Reports

### Standard Terminal Diagnostics
```text
examples/syntax_errors/missing_semicolon.nl:2:1: error: expected ';'
  let y = 10;
  ^
```

### Recovery Aggregate Report (`--explain-recovery`)
```text
=== NeuroParse Recovery Report ===
File: examples/syntax_errors/missing_semicolon.nl
Total Syntax Errors: 1
Successfully Recovered: 1
Failed Recoveries: 0
Cascading Errors: 0
Recovery Rate: 100.00%
Total Recovery Latency: 1140.00 µs
Average Latency per Attempt: 1140.00 µs

--- Diagnostics & Recovery Events ---
[1] examples/syntax_errors/missing_semicolon.nl:2:1: error: expected ';'
    Strategy: neural
    Action: insert_token
    Status: RECOVERED
    Description: Inserted ';' at position 4
    Latency: 1140.00 µs
    Tokens Inserted: 1 (';')
```

### Step-by-Step Chronological Trace (`--trace-recovery`)
```text
=== NeuroParse Recovery Trace ===
File: examples/syntax_errors/missing_semicolon.nl
[STEP 1]
  [ERROR] examples/syntax_errors/missing_semicolon.nl:2:1: error: expected ';'
  [STRATEGY] neural
  [ACTION] insert_token
  [STATUS] ACCEPTED & APPLIED
  [DESCRIPTION] Inserted ';' at position 4
  [LATENCY] 1140.00 µs
  [PARSER RESUME] index 4 -> 4
```

---

## 3. Structured JSON Schema (v1.0.0)

When invoked with `--diagnostics=json`, `neuroparse` produces machine-readable JSON enabling IDEs to display inline error markers and provide automated Quick Fixes:

```json
{
  "schema_version": "1.0.0",
  "file": "examples/syntax_errors/missing_semicolon.nl",
  "success": false,
  "metrics": {
    "total_errors": 1,
    "recovered_errors": 1,
    "failed_recoveries": 0,
    "cascading_errors": 0,
    "recovery_rate": 1.0000,
    "total_latency_us": 1140.00
  },
  "diagnostics": [
    {
      "source": "parser",
      "severity": "error",
      "message": "expected ';'",
      "line": 2,
      "column": 1,
      "end_line": 2,
      "end_column": 4,
      "code": "SYN001",
      "construct": "let binding",
      "expected_tokens": [";"],
      "recovery": {
        "strategy": "neural",
        "action": "insert_token",
        "status": "RECOVERED",
        "description": "Inserted ';' at position 4",
        "latency_us": 1140.00,
        "is_cascading": false,
        "suggested_edit": {
          "operation": "insert",
          "line": 2,
          "column": 1,
          "end_line": 2,
          "end_column": 1,
          "new_text": ";"
        }
      }
    }
  ]
}
```

### Suggested Edit Operations:
- `"insert"`: Adds `new_text` at `(line, column)`.
- `"delete"`: Deletes range from `(line, column)` to `(end_line, end_column)`.
- `"replace"`: Replaces range from `(line, column)` to `(end_line, end_column)` with `new_text`.
