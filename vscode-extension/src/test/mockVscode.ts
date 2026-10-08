/**
 * Lightweight mock for the `vscode` module to enable fast, standalone unit testing.
 */

class Position {
  constructor(public readonly line: number, public readonly character: number) {}
}

class Range {
  public readonly start: Position;
  public readonly end: Position;

  constructor(start: Position, end: Position);
  constructor(startLine: number, startCharacter: number, endLine: number, endCharacter: number);
  constructor(
    startOrStartLine: Position | number,
    endOrStartCharacter: Position | number,
    endLine?: number,
    endCharacter?: number
  ) {
    if (typeof startOrStartLine === "number") {
      this.start = new Position(startOrStartLine, endOrStartCharacter as number);
      this.end = new Position(endLine!, endCharacter!);
    } else {
      this.start = startOrStartLine;
      this.end = endOrStartCharacter as Position;
    }
  }

  public contains(pos: Position): boolean {
    if (pos.line < this.start.line || pos.line > this.end.line) return false;
    if (pos.line === this.start.line && pos.character < this.start.character) return false;
    if (pos.line === this.end.line && pos.character > this.end.character) return false;
    return true;
  }
}

enum DiagnosticSeverity {
  Error = 0,
  Warning = 1,
  Information = 2,
  Hint = 3,
}

class Diagnostic {
  public source?: string;
  public code?: string | number;
  constructor(
    public range: Range,
    public message: string,
    public severity: DiagnosticSeverity = DiagnosticSeverity.Error
  ) {}
}

const CodeActionKind = {
  QuickFix: "quickfix",
};

const CodeActionTriggerKind = {
  Invoke: 1,
  Automatic: 2,
};

class CodeAction {
  public diagnostics?: Diagnostic[];
  public isPreferred?: boolean;
  public edit?: WorkspaceEdit;
  public command?: any;

  constructor(public title: string, public kind?: any) {}
}

class WorkspaceEdit {
  public entries: Array<{ uri: Uri; edit: any }> = [];

  public insert(uri: Uri, position: Position, newText: string): void {
    this.entries.push({ uri, edit: { type: "insert", position, newText } });
  }

  public delete(uri: Uri, range: Range): void {
    this.entries.push({ uri, edit: { type: "delete", range } });
  }

  public replace(uri: Uri, range: Range, newText: string): void {
    this.entries.push({ uri, edit: { type: "replace", range, newText } });
  }
}

class Uri {
  private constructor(public readonly fsPath: string, public readonly scheme: string = "file") {}

  public static file(fsPath: string): Uri {
    return new Uri(fsPath, "file");
  }

  public toString(): string {
    return `file://${this.fsPath}`;
  }
}

const mockVscode = {
  Position,
  Range,
  Diagnostic,
  DiagnosticSeverity,
  CodeAction,
  CodeActionKind,
  CodeActionTriggerKind,
  WorkspaceEdit,
  Uri,
  workspace: {
    workspaceFolders: [
      {
        uri: Uri.file(process.cwd()),
        name: "workspace",
        index: 0,
      },
    ],
    getConfiguration: (section?: string) => ({
      get: (key: string, defaultValue?: any) => defaultValue,
      update: async () => {},
    }),
    onDidOpenTextDocument: () => ({ dispose: () => {} }),
    onDidChangeTextDocument: () => ({ dispose: () => {} }),
    onDidSaveTextDocument: () => ({ dispose: () => {} }),
    onDidCloseTextDocument: () => ({ dispose: () => {} }),
    onDidChangeConfiguration: () => ({ dispose: () => {} }),
  },
  window: {
    activeTextEditor: undefined,
    createOutputChannel: (name: string) => ({
      name,
      append: () => {},
      appendLine: () => {},
      clear: () => {},
      show: () => {},
      hide: () => {},
      dispose: () => {},
      replace: () => {},
    }),
    showInformationMessage: async (msg: string) => msg,
    showQuickPick: async (items: any[]) => items[0],
    createStatusBarItem: () => ({
      text: "",
      tooltip: "",
      command: "",
      show: () => {},
      hide: () => {},
      dispose: () => {},
    }),
    onDidChangeActiveTextEditor: () => ({ dispose: () => {} }),
  },
  languages: {
    createDiagnosticCollection: (name?: string) => ({
      name,
      set: () => {},
      delete: () => {},
      clear: () => {},
      get: () => undefined,
      has: () => false,
      dispose: () => {},
      forEach: () => {},
      [Symbol.iterator]: () => [][Symbol.iterator](),
    }),
    registerCodeActionsProvider: () => ({ dispose: () => {} }),
  },
  commands: {
    registerCommand: () => ({ dispose: () => {} }),
  },
  StatusBarAlignment: {
    Left: 1,
    Right: 2,
  },
};

export default mockVscode;
