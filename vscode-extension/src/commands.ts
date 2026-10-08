import * as cp from "child_process";
import * as vscode from "vscode";
import { ExtendedDiagnostic, NeuroParseDiagnosticProvider } from "./diagnostics";
import { NeuroParseRunner } from "./compiler";

export function registerCommands(
  context: vscode.ExtensionContext,
  diagnosticProvider: NeuroParseDiagnosticProvider,
  runner: NeuroParseRunner,
  outputChannel: vscode.OutputChannel
): void {
  // Command: Analyze Current File
  context.subscriptions.push(
    vscode.commands.registerCommand("neuroparse.analyzeFile", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor || editor.document.languageId !== "neurolang") {
        vscode.window.showInformationMessage("No active NeuroLang (*.nl) file to analyze.");
        return;
      }
      outputChannel.appendLine(`[NeuroParse] Manually analyzing: ${editor.document.fileName}`);
      await diagnosticProvider.analyzeDocument(editor.document);
      vscode.window.showInformationMessage("NeuroParse analysis complete.");
    })
  );

  // Command: Explain Syntax Error
  context.subscriptions.push(
    vscode.commands.registerCommand("neuroparse.explainError", async (diagnostic?: vscode.Diagnostic) => {
      const editor = vscode.window.activeTextEditor;
      if (!editor) {
        return;
      }

      let targetDiag: ExtendedDiagnostic | undefined;

      if (diagnostic && "rawDiagnostic" in diagnostic) {
        targetDiag = diagnostic as ExtendedDiagnostic;
      } else {
        // Find diagnostic under current cursor position
        const cursor = editor.selection.active;
        const diags = diagnosticProvider.getDiagnosticsForUri(editor.document.uri);
        targetDiag = diags.find((d) => d.range.contains(cursor)) || diags[0];
      }

      if (!targetDiag || !targetDiag.rawDiagnostic) {
        vscode.window.showInformationMessage("No syntax error found at the current position.");
        return;
      }

      const raw = targetDiag.rawDiagnostic;
      const lines: string[] = [];
      lines.push(`### NeuroParse Syntax Error Report`);
      lines.push(`**Diagnostic:** ${raw.message}`);
      lines.push(`**Location:** Line ${raw.line}, Column ${raw.column}`);
      if (raw.construct) {
        lines.push(`**Construct Context:** \`${raw.construct}\``);
      }
      if (raw.expected_tokens && raw.expected_tokens.length > 0) {
        lines.push(`**Expected Terminals:** ${raw.expected_tokens.map((t) => `'${t}'`).join(", ")}`);
      }

      if (raw.recovery) {
        const rec = raw.recovery;
        lines.push(`\n#### Recovery & Proposal Details`);
        lines.push(`- **Strategy:** ${rec.strategy.toUpperCase()}`);
        lines.push(`- **Action:** ${rec.action}`);
        lines.push(`- **Status:** ${rec.status}`);
        lines.push(`- **Description:** ${rec.description}`);
        lines.push(`- **Latency:** ${rec.latency_us} µs`);
        if (rec.is_cascading) {
          lines.push(`- **Warning:** Flagged as potential cascading error.`);
        }
      }

      const reportText = lines.join("\n");
      outputChannel.appendLine("\n" + reportText + "\n");
      outputChannel.show(true);

      const msg = raw.recovery
        ? `[${raw.recovery.strategy.toUpperCase()}]: ${raw.recovery.description}`
        : raw.message;
      vscode.window.showInformationMessage(`NeuroParse: ${msg}`, "View Output Channel").then((selection) => {
        if (selection === "View Output Channel") {
          outputChannel.show(true);
        }
      });
    })
  );

  // Command: View Recovery Trace
  context.subscriptions.push(
    vscode.commands.registerCommand("neuroparse.traceRecovery", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor || editor.document.languageId !== "neurolang") {
        vscode.window.showInformationMessage("No active NeuroLang (*.nl) file.");
        return;
      }

      const config = diagnosticProvider.getConfig();
      const exe = runner.resolveExecutablePath(config.compilerPath);

      outputChannel.appendLine(`\n=== Running Recovery Trace for ${editor.document.fileName} ===`);
      outputChannel.show(true);

      const args = ["--trace-recovery", `--recovery=${config.recoveryMode}`, editor.document.fileName];

      cp.execFile(exe, args, (err, stdout, stderr) => {
        if (stdout) {
          outputChannel.appendLine(stdout);
        }
        if (stderr) {
          outputChannel.appendLine(stderr);
        }
        if (err && !stdout && !stderr) {
          outputChannel.appendLine(`[Error] Execution failed: ${err.message}`);
        }
      });
    })
  );

  // Command: Change Recovery Mode
  context.subscriptions.push(
    vscode.commands.registerCommand("neuroparse.setRecoveryMode", async () => {
      const modes: Array<"auto" | "neural" | "heuristic" | "panic" | "none"> = [
        "auto",
        "neural",
        "heuristic",
        "panic",
        "none",
      ];
      const selected = await vscode.window.showQuickPick(modes, {
        placeHolder: "Select NeuroParse Error Recovery Strategy",
      });

      if (selected) {
        const wsConfig = vscode.workspace.getConfiguration("neuroparse");
        await wsConfig.update("recoveryMode", selected, vscode.ConfigurationTarget.Global);
        vscode.window.showInformationMessage(`NeuroParse recovery mode set to: ${selected}`);

        const editor = vscode.window.activeTextEditor;
        if (editor && editor.document.languageId === "neurolang") {
          diagnosticProvider.analyzeDocument(editor.document);
        }
      }
    })
  );

  // Command: Show Output Channel
  context.subscriptions.push(
    vscode.commands.registerCommand("neuroparse.showOutput", () => {
      outputChannel.show(true);
    })
  );
}
