import * as assert from "assert";
import * as vscode from "vscode";
import { NeuroParseCodeActionProvider } from "../../quickFix";
import { ExtendedDiagnostic } from "../../diagnostics";

suite("QuickFix CodeActionProvider Tests", () => {
  const provider = new NeuroParseCodeActionProvider();

  test("Generates QuickFix when compiler reports suggested_edit", () => {
    const diag: ExtendedDiagnostic = new vscode.Diagnostic(
      new vscode.Range(new vscode.Position(5, 0), new vscode.Position(5, 1)),
      "Expected ';'",
      vscode.DiagnosticSeverity.Error
    );
    diag.rawDiagnostic = {
      source: "parser",
      severity: "error",
      message: "Expected ';'",
      line: 6,
      column: 1,
      end_line: 6,
      end_column: 1,
      recovery: {
        strategy: "neural",
        action: "insert_token",
        status: "RECOVERED",
        description: "Validated neural repair: INSERT_TOKEN token=';'",
        latency_us: 120.0,
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
    };
    diag.documentVersion = 2;

    const mockDoc = {
      version: 2,
      uri: vscode.Uri.file("test.nl"),
    } as unknown as vscode.TextDocument;

    const range = new vscode.Range(new vscode.Position(5, 0), new vscode.Position(5, 1));
    const context: vscode.CodeActionContext = {
      diagnostics: [diag],
      triggerKind: vscode.CodeActionTriggerKind.Invoke,
      only: undefined,
    };

    const actions = provider.provideCodeActions(mockDoc, range, context);
    assert.strictEqual(actions.length, 2); // 1 QuickFix edit + 1 Explain action

    const fixAction = actions[0];
    assert.ok(fixAction.title.includes("Insert ';'"));
    assert.strictEqual(fixAction.isPreferred, true);
    assert.ok(fixAction.edit !== undefined);
  });

  test("Protects against stale document edits when version changed", () => {
    const diag: ExtendedDiagnostic = new vscode.Diagnostic(
      new vscode.Range(new vscode.Position(5, 0), new vscode.Position(5, 1)),
      "Expected ';'",
      vscode.DiagnosticSeverity.Error
    );
    diag.rawDiagnostic = {
      source: "parser",
      severity: "error",
      message: "Expected ';'",
      line: 6,
      column: 1,
      end_line: 6,
      end_column: 1,
      recovery: {
        strategy: "neural",
        action: "insert_token",
        status: "RECOVERED",
        description: "Validated repair",
        latency_us: 100,
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
    };
    diag.documentVersion = 2; // Diagnostic was computed on version 2

    const currentDoc = {
      version: 3, // Document was modified to version 3!
      uri: vscode.Uri.file("test.nl"),
    } as unknown as vscode.TextDocument;

    const range = new vscode.Range(new vscode.Position(5, 0), new vscode.Position(5, 1));
    const context: vscode.CodeActionContext = {
      diagnostics: [diag],
      triggerKind: vscode.CodeActionTriggerKind.Invoke,
      only: undefined,
    };

    const actions = provider.provideCodeActions(currentDoc, range, context);
    // Stale edit action should be suppressed, only explain action remains
    assert.strictEqual(actions.length, 1);
    assert.ok(actions[0].title.includes("Explain error"));
  });
});
