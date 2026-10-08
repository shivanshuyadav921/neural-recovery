import * as assert from "assert";
import * as vscode from "vscode";
import { NeuroParseRunner } from "../../compiler";

suite("Compiler Runner Unit Tests", () => {
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

  const runner = new NeuroParseRunner(dummyOutputChannel);

  test("Resolves executable path with auto-detection fallback", () => {
    const resolved = runner.resolveExecutablePath("");
    assert.ok(resolved.length > 0);
    assert.ok(resolved.includes("neuroparse"));
  });

  test("Handles explicit absolute path", () => {
    const custom = "C:\\Tools\\neuroparse.exe";
    const resolved = runner.resolveExecutablePath(custom);
    // If not found, falls back gracefully
    assert.ok(resolved.length > 0);
  });
});
