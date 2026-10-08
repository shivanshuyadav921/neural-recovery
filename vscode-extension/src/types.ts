/**
 * Type definitions matching the authoritative NeuroParse compiler JSON diagnostics contract.
 */

export interface SuggestedEdit {
  operation: "insert" | "delete" | "replace";
  line: number;
  column: number;
  end_line: number;
  end_column: number;
  new_text: string;
}

export interface RecoveryInfo {
  strategy: "neural" | "heuristic" | "panic" | "none" | "unknown";
  action: "insert_token" | "delete_token" | "replace_token" | "skip_tokens" | "synchronize_statement" | "none" | "unknown";
  status: "RECOVERED" | "FAILED";
  description: string;
  latency_us: number;
  is_cascading: boolean;
  suggested_edit?: SuggestedEdit;
}

export interface CompilerDiagnostic {
  source: "parser" | "lexer" | "cli";
  severity: "error" | "warning" | "info" | "hint";
  message: string;
  line: number;
  column: number;
  end_line: number;
  end_column: number;
  code?: string;
  construct?: string;
  expected_tokens?: string[];
  recovery?: RecoveryInfo;
}

export interface CompilerMetrics {
  total_errors: number;
  recovered_errors: number;
  failed_recoveries: number;
  cascading_errors: number;
  recovery_rate: number;
  total_latency_us: number;
}

export interface CompilerOutput {
  schema_version: string;
  file: string;
  success: boolean;
  metrics: CompilerMetrics;
  diagnostics: CompilerDiagnostic[];
}

export interface ExtensionConfig {
  compilerPath: string;
  recoveryMode: "auto" | "neural" | "heuristic" | "panic" | "none";
  runMode: "onType" | "onSave" | "manual";
  debounceDelay: number;
  timeoutMs: number;
  traceRecovery: boolean;
}
