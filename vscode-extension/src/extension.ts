import * as vscode from "vscode";
import { registerCommands } from "./commands";
import { NeuroParseRunner } from "./compiler";
import { NeuroParseDiagnosticProvider } from "./diagnostics";
import { NeuroParseCodeActionProvider } from "./quickFix";

let statusBarItem: vscode.StatusBarItem;

export function activate(context: vscode.ExtensionContext): void {
  // 1. Output Channel
  const outputChannel = vscode.window.createOutputChannel("NeuroParse");
  context.subscriptions.push(outputChannel);
  outputChannel.appendLine("[NeuroParse] Extension activated.");

  // 2. Diagnostic Collection & Runner
  const diagnosticCollection = vscode.languages.createDiagnosticCollection("neurolang");
  context.subscriptions.push(diagnosticCollection);

  const runner = new NeuroParseRunner(outputChannel);
  const diagnosticProvider = new NeuroParseDiagnosticProvider(
    diagnosticCollection,
    runner,
    outputChannel
  );

  // 3. Register Code Actions for Quick Fix
  context.subscriptions.push(
    vscode.languages.registerCodeActionsProvider(
      { language: "neurolang", scheme: "file" },
      new NeuroParseCodeActionProvider(),
      {
        providedCodeActionKinds: NeuroParseCodeActionProvider.providedCodeActionKinds,
      }
    )
  );

  // 4. Register Commands
  registerCommands(context, diagnosticProvider, runner, outputChannel);

  // 5. Register Document Lifecycle Events
  diagnosticProvider.registerEvents(context);

  // 6. Status Bar Item
  statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
  statusBarItem.command = "neuroparse.setRecoveryMode";
  context.subscriptions.push(statusBarItem);
  updateStatusBar();

  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("neuroparse")) {
        updateStatusBar();
        if (vscode.window.activeTextEditor?.document.languageId === "neurolang") {
          diagnosticProvider.analyzeDocument(vscode.window.activeTextEditor.document);
        }
      }
    })
  );

  context.subscriptions.push(
    vscode.window.onDidChangeActiveTextEditor((editor) => {
      if (editor?.document.languageId === "neurolang") {
        statusBarItem.show();
        diagnosticProvider.analyzeDocument(editor.document);
      } else {
        statusBarItem.hide();
      }
    })
  );

  // Initial check for currently active editor
  if (vscode.window.activeTextEditor?.document.languageId === "neurolang") {
    statusBarItem.show();
    diagnosticProvider.analyzeDocument(vscode.window.activeTextEditor.document);
  }
}

function updateStatusBar(): void {
  const wsConfig = vscode.workspace.getConfiguration("neuroparse");
  const mode = wsConfig.get<string>("recoveryMode", "auto");
  statusBarItem.text = `$(circuit-board) NeuroParse: ${mode}`;
  statusBarItem.tooltip = `Click to change NeuroParse recovery mode (current: ${mode})`;
}

export function deactivate(): void {
  if (statusBarItem) {
    statusBarItem.dispose();
  }
}
