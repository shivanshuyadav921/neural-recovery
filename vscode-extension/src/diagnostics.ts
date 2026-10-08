import * as vscode from "vscode";
import { NeuroParseRunner } from "./compiler";
import { CompilerDiagnostic, CompilerOutput, ExtensionConfig } from "./types";

export interface ExtendedDiagnostic extends vscode.Diagnostic {
  rawDiagnostic?: CompilerDiagnostic;
  documentVersion?: number;
}

export class NeuroParseDiagnosticProvider {
  private diagnosticCollection: vscode.DiagnosticCollection;
  private runner: NeuroParseRunner;
  private outputChannel: vscode.OutputChannel;
  private debounceTimers: Map<string, NodeJS.Timeout> = new Map();
  private lastDiagnostics: Map<string, ExtendedDiagnostic[]> = new Map();

  constructor(
    diagnosticCollection: vscode.DiagnosticCollection,
    runner: NeuroParseRunner,
    outputChannel: vscode.OutputChannel
  ) {
    this.diagnosticCollection = diagnosticCollection;
    this.runner = runner;
    this.outputChannel = outputChannel;
  }

  public getConfig(): ExtensionConfig {
    const wsConfig = vscode.workspace.getConfiguration("neuroparse");
    return {
      compilerPath: wsConfig.get<string>("compilerPath", ""),
      recoveryMode: wsConfig.get<"auto" | "neural" | "heuristic" | "panic" | "none">("recoveryMode", "auto"),
      runMode: wsConfig.get<"onType" | "onSave" | "manual">("runMode", "onType"),
      debounceDelay: wsConfig.get<number>("debounceDelay", 300),
      timeoutMs: wsConfig.get<number>("timeoutMs", 5000),
      traceRecovery: wsConfig.get<boolean>("traceRecovery", false),
    };
  }

  public async analyzeDocument(document: vscode.TextDocument): Promise<void> {
    if (document.languageId !== "neurolang") {
      return;
    }

    const config = this.getConfig();
    const docVersion = document.version;
    const uri = document.uri;

    try {
      const output: CompilerOutput = await this.runner.runCompiler(
        document.getText(),
        document.uri.fsPath,
        config
      );

      // Verify that the document version hasn't changed during analysis
      if (document.version !== docVersion) {
        return;
      }

      const diagnostics: ExtendedDiagnostic[] = this.convertDiagnostics(output, document, docVersion);
      this.diagnosticCollection.set(uri, diagnostics);
      this.lastDiagnostics.set(uri.toString(), diagnostics);

      if (config.traceRecovery) {
        this.outputChannel.appendLine(
          `[NeuroParse] ${pathBasename(uri.fsPath)}: ${diagnostics.length} diagnostic(s) reported.`
        );
      }
    } catch (err: any) {
      if (config.traceRecovery) {
        this.outputChannel.appendLine(`[NeuroParse Analysis Error] ${err.message || String(err)}`);
      }
    }
  }

  public convertDiagnostics(
    output: CompilerOutput,
    document: vscode.TextDocument,
    docVersion: number
  ): ExtendedDiagnostic[] {
    const result: ExtendedDiagnostic[] = [];

    for (const cd of output.diagnostics) {
      // 1-indexed to 0-indexed line and column
      const startLine = Math.max(0, cd.line - 1);
      const startCol = Math.max(0, cd.column - 1);
      let endLine = Math.max(0, cd.end_line - 1);
      let endCol = Math.max(0, cd.end_column - 1);

      // Ensure valid range bounds
      if (endLine < startLine || (endLine === startLine && endCol < startCol)) {
        endLine = startLine;
        endCol = startCol + 1;
      }

      const range = new vscode.Range(
        new vscode.Position(startLine, startCol),
        new vscode.Position(endLine, endCol)
      );

      let severity = vscode.DiagnosticSeverity.Error;
      if (cd.severity === "warning") {
        severity = vscode.DiagnosticSeverity.Warning;
      } else if (cd.severity === "info") {
        severity = vscode.DiagnosticSeverity.Information;
      } else if (cd.severity === "hint") {
        severity = vscode.DiagnosticSeverity.Hint;
      }

      let message = cd.message;
      if (cd.recovery && cd.recovery.status === "RECOVERED") {
        message += `\n[NeuroParse ${cd.recovery.strategy} suggestion]: ${cd.recovery.description}`;
      }

      const diag: ExtendedDiagnostic = new vscode.Diagnostic(range, message, severity);
      diag.source = "NeuroParse";
      diag.code = cd.code || "SYN001";
      diag.rawDiagnostic = cd;
      diag.documentVersion = docVersion;

      result.push(diag);
    }

    return result;
  }

  public getDiagnosticsForUri(uri: vscode.Uri): ExtendedDiagnostic[] {
    return this.lastDiagnostics.get(uri.toString()) || [];
  }

  public clear(uri: vscode.Uri): void {
    this.diagnosticCollection.delete(uri);
    this.lastDiagnostics.delete(uri.toString());
  }

  public registerEvents(context: vscode.ExtensionContext): void {
    // On open
    context.subscriptions.push(
      vscode.workspace.onDidOpenTextDocument((doc) => {
        if (doc.languageId === "neurolang") {
          this.analyzeDocument(doc);
        }
      })
    );

    // On change (debounced)
    context.subscriptions.push(
      vscode.workspace.onDidChangeTextDocument((event) => {
        const doc = event.document;
        if (doc.languageId !== "neurolang") {
          return;
        }

        const config = this.getConfig();
        if (config.runMode !== "onType") {
          return;
        }

        const key = doc.uri.toString();
        const existingTimer = this.debounceTimers.get(key);
        if (existingTimer) {
          clearTimeout(existingTimer);
        }

        const timer = setTimeout(() => {
          this.analyzeDocument(doc);
        }, config.debounceDelay);

        this.debounceTimers.set(key, timer);
      })
    );

    // On save
    context.subscriptions.push(
      vscode.workspace.onDidSaveTextDocument((doc) => {
        if (doc.languageId !== "neurolang") {
          return;
        }
        const config = this.getConfig();
        if (config.runMode === "onSave" || config.runMode === "onType") {
          this.analyzeDocument(doc);
        }
      })
    );

    // On close
    context.subscriptions.push(
      vscode.workspace.onDidCloseTextDocument((doc) => {
        this.clear(doc.uri);
      })
    );
  }
}

function pathBasename(p: string): string {
  const parts = p.split(/[\\/]/);
  return parts[parts.length - 1] || p;
}
