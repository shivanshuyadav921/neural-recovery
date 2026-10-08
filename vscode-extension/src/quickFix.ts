import * as vscode from "vscode";
import { ExtendedDiagnostic } from "./diagnostics";
import { SuggestedEdit } from "./types";

export class NeuroParseCodeActionProvider implements vscode.CodeActionProvider {
  public static readonly providedCodeActionKinds = [vscode.CodeActionKind.QuickFix];

  public provideCodeActions(
    document: vscode.TextDocument,
    range: vscode.Range | vscode.Selection,
    context: vscode.CodeActionContext
  ): vscode.CodeAction[] {
    const actions: vscode.CodeAction[] = [];

    for (const diagnostic of context.diagnostics) {
      const extDiag = diagnostic as ExtendedDiagnostic;
      if (!extDiag.rawDiagnostic || !extDiag.rawDiagnostic.recovery) {
        continue;
      }

      const recovery = extDiag.rawDiagnostic.recovery;
      const edit = recovery.suggested_edit;

      // 1. Quick Fix Action if suggested edit exists
      if (edit && recovery.status === "RECOVERED") {
        const isStale = (extDiag.documentVersion !== undefined && extDiag.documentVersion !== document.version);
        if (!isStale) {
          const actionTitle = this.formatActionTitle(recovery.strategy, edit);
          const action = new vscode.CodeAction(actionTitle, vscode.CodeActionKind.QuickFix);
          action.diagnostics = [diagnostic];
          action.isPreferred = true;

          const wsEdit = new vscode.WorkspaceEdit();
          const editRange = this.convertToVsCodeRange(edit);

          if (edit.operation === "insert") {
            wsEdit.insert(document.uri, editRange.start, edit.new_text);
          } else if (edit.operation === "delete") {
            wsEdit.delete(document.uri, editRange);
          } else if (edit.operation === "replace") {
            wsEdit.replace(document.uri, editRange, edit.new_text);
          }

          action.edit = wsEdit;
          actions.push(action);
        }
      }

      // 2. Explain Error Action
      const explainAction = new vscode.CodeAction(
        `NeuroParse: Explain error & recovery suggestion`,
        vscode.CodeActionKind.QuickFix
      );
      explainAction.diagnostics = [diagnostic];
      explainAction.command = {
        command: "neuroparse.explainError",
        title: "Explain Error",
        arguments: [diagnostic],
      };
      actions.push(explainAction);
    }

    return actions;
  }

  private convertToVsCodeRange(edit: SuggestedEdit): vscode.Range {
    const startLine = Math.max(0, edit.line - 1);
    const startCol = Math.max(0, edit.column - 1);
    const endLine = Math.max(0, edit.end_line - 1);
    const endCol = Math.max(0, edit.end_column - 1);

    return new vscode.Range(
      new vscode.Position(startLine, startCol),
      new vscode.Position(endLine, endCol)
    );
  }

  private formatActionTitle(strategy: string, edit: SuggestedEdit): string {
    const stratName = strategy === "neural" ? "Neural" : strategy === "heuristic" ? "Heuristic" : "Compiler";
    if (edit.operation === "insert") {
      return `NeuroParse (${stratName}): Insert '${edit.new_text}'`;
    } else if (edit.operation === "delete") {
      return `NeuroParse (${stratName}): Delete unexpected token`;
    } else if (edit.operation === "replace") {
      return `NeuroParse (${stratName}): Replace with '${edit.new_text}'`;
    }
    return `NeuroParse: Apply suggested repair`;
  }
}
