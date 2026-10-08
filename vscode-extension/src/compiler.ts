import * as cp from "child_process";
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import * as vscode from "vscode";
import { CompilerOutput, ExtensionConfig } from "./types";

export class NeuroParseRunner {
  private outputChannel: vscode.OutputChannel;

  constructor(outputChannel: vscode.OutputChannel) {
    this.outputChannel = outputChannel;
  }

  /**
   * Resolves the NeuroParse compiler executable path.
   */
  public resolveExecutablePath(configPath: string): string {
    if (configPath && configPath.trim().length > 0) {
      if (path.isAbsolute(configPath)) {
        if (fs.existsSync(configPath)) {
          return configPath;
        }
      } else {
        // Resolve relative to workspace folders
        const workspaceFolders = vscode.workspace.workspaceFolders;
        if (workspaceFolders) {
          for (const folder of workspaceFolders) {
            const candidate = path.join(folder.uri.fsPath, configPath);
            if (fs.existsSync(candidate)) {
              return candidate;
            }
          }
        }
      }
    }

    // Auto-detect in common project locations
    const exeName = process.platform === "win32" ? "neuroparse.exe" : "neuroparse";
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (workspaceFolders) {
      for (const folder of workspaceFolders) {
        const candidates = [
          path.join(folder.uri.fsPath, "build", exeName),
          path.join(folder.uri.fsPath, "..", "build", exeName),
          path.join(folder.uri.fsPath, "bin", exeName),
        ];
        for (const candidate of candidates) {
          if (fs.existsSync(candidate)) {
            return candidate;
          }
        }
      }
    }

    // Default to system PATH
    return exeName;
  }

  /**
   * Executes the NeuroParse compiler on a document or source string.
   */
  public async runCompiler(
    sourceText: string,
    filePath: string,
    config: ExtensionConfig
  ): Promise<CompilerOutput> {
    const exe = this.resolveExecutablePath(config.compilerPath);
    let tempFilePath: string | null = null;
    let targetPath = filePath;

    // If file is untitled or has unsaved changes, write to temp file
    if (!filePath || filePath.startsWith("untitled:") || sourceText !== undefined) {
      const tempDir = os.tmpdir();
      tempFilePath = path.join(tempDir, `neuroparse_temp_${Date.now()}_${Math.random().toString(36).substring(2, 8)}.nl`);
      fs.writeFileSync(tempFilePath, sourceText, "utf8");
      targetPath = tempFilePath;
    }

    const args: string[] = [
      "--diagnostics=json",
      `--recovery=${config.recoveryMode}`,
      targetPath,
    ];

    if (config.traceRecovery) {
      this.outputChannel.appendLine(`[NeuroParse] Invoking: ${exe} ${args.join(" ")}`);
    }

    try {
      const { stdout, stderr, exitCode } = await this.execProcess(exe, args, config.timeoutMs);

      if (config.traceRecovery && stderr) {
        this.outputChannel.appendLine(`[NeuroParse stderr] ${stderr}`);
      }

      if (!stdout || stdout.trim().length === 0) {
        throw new Error(`Compiler returned empty output. Exit code: ${exitCode}. Stderr: ${stderr}`);
      }

      const parsed: CompilerOutput = JSON.parse(stdout);
      return parsed;
    } catch (err: any) {
      this.outputChannel.appendLine(`[NeuroParse Error] ${err.message || String(err)}`);
      throw err;
    } finally {
      if (tempFilePath && fs.existsSync(tempFilePath)) {
        try {
          fs.unlinkSync(tempFilePath);
        } catch (_) {}
      }
    }
  }

  private execProcess(
    exe: string,
    args: string[],
    timeoutMs: number
  ): Promise<{ stdout: string; stderr: string; exitCode: number | null }> {
    return new Promise((resolve, reject) => {
      let stdout = "";
      let stderr = "";
      let isTimedOut = false;

      const proc = cp.spawn(exe, args, {
        windowsHide: true,
      });

      const timer = setTimeout(() => {
        isTimedOut = true;
        proc.kill();
        reject(new Error(`NeuroParse compiler timed out after ${timeoutMs} ms`));
      }, timeoutMs);

      proc.stdout.on("data", (data) => {
        stdout += data.toString("utf8");
      });

      proc.stderr.on("data", (data) => {
        stderr += data.toString("utf8");
      });

      proc.on("error", (err) => {
        clearTimeout(timer);
        reject(new Error(`Failed to spawn NeuroParse executable '${exe}': ${err.message}`));
      });

      proc.on("close", (code) => {
        clearTimeout(timer);
        if (!isTimedOut) {
          resolve({ stdout, stderr, exitCode: code });
        }
      });
    });
  }
}
