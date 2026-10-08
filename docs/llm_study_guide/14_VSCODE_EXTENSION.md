# Chapter 14: Visual Studio Code Extension Integration

This chapter covers the architecture of the **NeuroParse VS Code extension**, its compiler communication bridge, debounced diagnostic provider, QuickFix code actions, and configuration settings.

---

## 1. Extension Architecture

The extension ([`vscode-extension/`](file:///c:/Users/shiva/OneDrive/Desktop/neurology/vscode-extension/)) is written in TypeScript and connects editor document lifecycles to the native C++ compiler CLI.

```
VS Code Editor (Active Document: .nl)
         │
         ▼ (onType / onSave / onOpen)
[ NeuroParseDiagnosticProvider ] (Debounced: 300ms)
         │
         ▼
[ NeuroParseRunner ] ────────► Invokes: `neuroparse --diagnostics=json <temp_file.nl>`
         │
         ▼ Reads JSON stdout
[ Diagnostic Collection ] ───► Emits red squigglies (vscode.Diagnostic)
         │
         ▼ Lightbulb action requested
[ NeuroParseCodeActionProvider ] ──► Emits QuickFix: WorkspaceEdit applying suggested_edit
```

---

## 2. Subsystem Components

### 1. The Compiler Runner (`compiler.ts`)
The `NeuroParseRunner` executes the native binary as a subprocess:
- Dynamically resolves compiler binary path (from settings, build directory, or `PATH`).
- Writes in-memory buffer to temporary `.nl` file.
- Executes `neuroparse --diagnostics=json --recovery=<mode> <temp_file>`.
- Parses JSON output and handles process timeouts (default 5,000 ms).

### 2. Diagnostic Provider (`diagnostics.ts`)
- Manages `vscode.DiagnosticCollection`.
- Debounces document change events (`onDidChangeTextDocument`) by 300 ms to avoid compiler thrashing during typing.
- Converts JSON diagnostics into `vscode.Diagnostic` objects, attaching source coordinates, error codes (`SYN001`, `LEX001`), and recovery metadata.

### 3. QuickFix Code Actions (`quickFix.ts`)
The `NeuroParseCodeActionProvider` inspects diagnostics for attached `recovery.suggested_edit` metadata:
- If a repair was found (`status == "RECOVERED"`), generates a `vscode.CodeAction` with `CodeActionKind.QuickFix`:
  - `"insert"`: Applies `wsEdit.insert(uri, range.start, new_text)`.
  - `"delete"`: Applies `wsEdit.delete(uri, range)`.
  - `"replace"`: Applies `wsEdit.replace(uri, range, new_text)`.
- Flags the action as preferred (`action.isPreferred = true`), allowing single-keystroke fixing (`Ctrl+.` or `Cmd+.`).
- Adds an **"Explain error & recovery suggestion"** action to display model confidence and latency.

### 4. Interactive Commands (`commands.ts`)
- `neuroparse.analyze`: Manually triggers compiler analysis on current file.
- `neuroparse.setRecoveryMode`: Opens a quick pick menu to switch between `auto`, `neural`, `heuristic`, `panic`, and `none`.
- `neuroparse.explainError`: Displays a modal dialog with full recovery explanation.

---

## 3. Extension Settings (`package.json`)

| Setting Key | Type | Default | Description |
|---|---|---|---|
| `neuroparse.compilerPath` | `string` | `""` | Path to `neuroparse` binary (auto-detects `./build/` if empty). |
| `neuroparse.recoveryMode` | `string` | `"auto"` | Strategy: `auto`, `neural`, `heuristic`, `panic`, `none`. |
| `neuroparse.runMode` | `string` | `"onType"` | When to run: `onType`, `onSave`, `manual`. |
| `neuroparse.debounceDelay` | `number` | `300` | Typing debounce delay in milliseconds. |
| `neuroparse.timeoutMs` | `number` | `5000` | Subprocess timeout in milliseconds. |
| `neuroparse.traceRecovery`| `boolean`| `false` | Enables `--trace-recovery` in output logs. |

---

## 4. Building & Running Extension Tests

```bash
cd vscode-extension
npm install
npm run compile
npm test
```
