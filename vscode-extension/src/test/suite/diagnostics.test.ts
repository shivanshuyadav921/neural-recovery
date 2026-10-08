import * as assert from "assert";
import * as vscode from "vscode";
import { NeuroParseDiagnosticProvider } from "../../diagnostics";
import { NeuroParseRunner } from "../../compiler";
import { CompilerOutput } from "../../types";

suite("Diagnostics Conversion & Provider Tests", () => {
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

  test("Converts 1-indexed compiler diagnostic to 0-indexed VS Code range", () => {
    const mockOutput: CompilerOutput = {
      schema_version: "1.0.0",
      file: "test.nl",
      success: false,
      metrics: {
        total_errors: 1,
        recovered_errors: 1,
        failed_recoveries: 0,
        cascading_errors: 0,
        recovery_rate: 1.0,
        total_latency_us: 150.0,
      },
      diagnostics: [
        {
          source: "parser",
          severity: "error",
          message: "Expected ';' after return statement.",
          line: 6,
          column: 1,
          end_line: 6,
          end_column: 2,
          code: "SYN001",
          construct: "return statement",
          expected_tokens: [";"],
          recovery: {
            strategy: "neural",
            action: "insert_token",
            status: "RECOVERED",
            description: "Validated neural repair applied: INSERT_TOKEN token=';' pos=12 conf=0.95",
            latency_us: 150.0,
            is_cascading: false,
            suggested_edit: {
              operation: "insert",
              line: 6,
              column: 1,
              end_line: 6,
              end_column: 1,
              new_text: ";",
            },
          },
        },
      ],
    };

    const mockDoc = {
      version: 1,
      uri: vscode.Uri.file("test.nl"),
      languageId: "neurolang",
      getText: () => "return 42",
    } as unknown as vscode.TextDocument;

    const diags = provider.convertDiagnostics(mockOutput, mockDoc, 1);
    assert.strictEqual(diags.length, 1);
    const d = diags[0];

    // Check 0-indexed position
    assert.strictEqual(d.range.start.line, 5);
    assert.strictEqual(d.range.start.character, 0);
    assert.strictEqual(d.range.end.line, 5);
    assert.strictEqual(d.range.end.character, 1);
    assert.strictEqual(d.severity, vscode.DiagnosticSeverity.Error);
    assert.strictEqual(d.source, "NeuroParse");
    assert.ok(d.message.includes("Expected ';'"));
    assert.ok(d.message.includes("NeuroParse neural suggestion"));
    assert.strictEqual(d.rawDiagnostic?.recovery?.strategy, "neural");
    assert.strictEqual(d.documentVersion, 1);
  });

  test("Maps warning and info severities correctly", () => {
    const mockOutput: CompilerOutput = {
      schema_version: "1.0.0",
      file: "test.nl",
      success: false,
      metrics: {
        total_errors: 2,
        recovered_errors: 0,
        failed_recoveries: 2,
        cascading_errors: 0,
        recovery_rate: 0.0,
        total_latency_us: 0.0,
      },
      diagnostics: [
        {
          source: "parser",
          severity: "warning",
          message: "Unused variable",
          line: 2,
          column: 5,
          end_line: 2,
          end_column: 10,
        },
        {
          source: "parser",
          severity: "info",
          message: "Optimization hint",
          line: 3,
          column: 1,
          end_line: 3,
          end_column: 5,
        },
      ],
    };

    const mockDoc = { version: 1 } as unknown as vscode.TextDocument;
    const diags = provider.convertDiagnostics(mockOutput, mockDoc, 1);
    assert.strictEqual(diags.length, 2);
    assert.strictEqual(diags[0].severity, vscode.DiagnosticSeverity.Warning);
    assert.strictEqual(diags[1].severity, vscode.DiagnosticSeverity.Information);
  });
});
