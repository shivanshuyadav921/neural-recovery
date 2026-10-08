import * as assert from "assert";
import * as fs from "fs";
import * as path from "path";
import * as vscode from "vscode";
import { NeuroParseRunner } from "../../compiler";
import { NeuroParseDiagnosticProvider } from "../../diagnostics";
import { ExtensionConfig } from "../../types";

suite("End-to-End Compiler Integration Tests", () => {
  const dummyOutputChannel: vscode.OutputChannel = {
    name: "Test",
    append: () => {},
    appendLine: () => {},
    clear: () => {},
    show: () => {},
    hide: () => {},
    dispose: () => {},
    replace: () => {},
  };

  const dummyCollection: vscode.DiagnosticCollection = {
    name: "test",
    set: () => {},
    delete: () => {},
    clear: () => {},
    forEach: () => {},
    get: () => undefined,
    has: () => false,
    dispose: () => {},
    [Symbol.iterator]: () => [][Symbol.iterator](),
  };

  const runner = new NeuroParseRunner(dummyOutputChannel);
  const provider = new NeuroParseDiagnosticProvider(dummyCollection, runner, dummyOutputChannel);

  test("End-to-End: Real C++ compiler invocation, JSON parsing, and diagnostic conversion", async () => {
    const exeName = process.platform === "win32" ? "neuroparse.exe" : "neuroparse";
    const exePath = path.resolve(__dirname, "../../../../build", exeName);

    if (!fs.existsSync(exePath)) {
      console.log(`Skipping E2E test; binary not found at: ${exePath}`);
      return;
    }

    const config: ExtensionConfig = {
      compilerPath: exePath,
      recoveryMode: "neural",
      runMode: "onType",
      debounceDelay: 300,
      timeoutMs: 5000,
      traceRecovery: false,
    };

    const brokenSource = "function test(a, b) {\n    let c = a + b\n    return c;\n}\n";
    const output = await runner.runCompiler(brokenSource, "test.nl", config);

    assert.strictEqual(output.schema_version, "1.0.0");
    assert.strictEqual(output.success, false);
    assert.ok(output.diagnostics.length >= 1);

    const diag = output.diagnostics[0];
    assert.strictEqual(diag.source, "parser");
    assert.strictEqual(diag.severity, "error");
    assert.strictEqual(diag.line, 3);
    assert.ok(diag.message.includes("Expected ';'"));
    assert.ok(diag.recovery !== undefined);
    assert.strictEqual(diag.recovery.strategy, "neural");
    assert.strictEqual(diag.recovery.action, "insert_token");
    assert.strictEqual(diag.recovery.status, "RECOVERED");
    assert.ok(diag.recovery.suggested_edit !== undefined);
    assert.strictEqual(diag.recovery.suggested_edit.operation, "insert");
    assert.strictEqual(diag.recovery.suggested_edit.new_text, ";");

    const mockDoc = {
      version: 1,
      uri: vscode.Uri.file("test.nl"),
      languageId: "neurolang",
      getText: () => brokenSource,
    } as unknown as vscode.TextDocument;

    const vsDiags = provider.convertDiagnostics(output, mockDoc, 1);
    assert.strictEqual(vsDiags.length, 1);
    assert.strictEqual(vsDiags[0].range.start.line, 2); // 0-indexed line 3 -> 2
    assert.strictEqual(vsDiags[0].severity, vscode.DiagnosticSeverity.Error);
  });
});
