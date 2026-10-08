#include "neuroparse/cli/cli.hpp"

#include "neuroparse/ast/ast_printer.hpp"
#include "neuroparse/lexer/lexer.hpp"
#include "neuroparse/parser/parser.hpp"
#include "neuroparse/recovery/context_extractor.hpp"
#include "neuroparse/version.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace neuroparse::cli {
namespace {

std::string unimplemented(std::string_view feature) {
  std::ostringstream out;
  out << "neuroparse: '" << feature
      << "' is not implemented yet.\n"
      << "Neural inference, context extraction, and backend code generation are scheduled for later phases.\n";
  return out.str();
}

std::string format_explanation(
    const parser::ParseResult& parse_res,
    std::string_view filename) {
  std::ostringstream out;
  out << "=== NeuroParse Recovery Report ===\n";
  out << "File: " << (filename.empty() ? "<input>" : filename) << "\n";
  out << "Total Syntax Errors: " << parse_res.metrics.total_errors << "\n";
  out << "Successfully Recovered: " << parse_res.metrics.recovered_errors << "\n";
  out << "Failed Recoveries: " << parse_res.metrics.failed_recoveries << "\n";
  out << "Cascading Errors: " << parse_res.metrics.cascading_errors << "\n";
  out << std::fixed << std::setprecision(2);
  out << "Recovery Rate: " << (parse_res.metrics.recovery_rate() * 100.0) << "%\n";
  out << "Total Recovery Latency: " << parse_res.metrics.total_latency_us << " µs\n";
  out << "Average Latency per Attempt: " << parse_res.metrics.average_latency_us() << " µs\n\n";

  if (!parse_res.errors.empty()) {
    out << "--- Diagnostics & Recovery Events ---\n";
    for (std::size_t i = 0; i < parse_res.errors.size(); ++i) {
      const auto& err = parse_res.errors[i];
      out << "[" << (i + 1) << "] " << err.format(filename) << "\n";
      if (i < parse_res.recoveries.size()) {
        const auto& rec = parse_res.recoveries[i];
        out << "    Strategy: " << recovery::to_string(rec.strategy) << "\n";
        out << "    Action: " << recovery::to_string(rec.action) << "\n";
        out << "    Status: " << (rec.success ? "RECOVERED" : "FAILED") << "\n";
        out << "    Description: " << rec.description << "\n";
        out << "    Latency: " << rec.latency_us << " µs\n";
        if (rec.is_cascading) {
          out << "    Warning: Flagged as potential cascading error.\n";
        }
        if (!rec.tokens_skipped.empty()) {
          out << "    Tokens Skipped: " << rec.tokens_skipped.size() << " (";
          for (std::size_t j = 0; j < rec.tokens_skipped.size(); ++j) {
            if (j > 0) out << ", ";
            out << "'" << rec.tokens_skipped[j].lexeme << "'";
          }
          out << ")\n";
        }
        if (!rec.tokens_inserted.empty()) {
          out << "    Tokens Inserted: " << rec.tokens_inserted.size() << " (";
          for (std::size_t j = 0; j < rec.tokens_inserted.size(); ++j) {
            if (j > 0) out << ", ";
            out << "'" << rec.tokens_inserted[j].lexeme << "'";
          }
          out << ")\n";
        }
      }
    }
  }

  return out.str();
}

std::string format_recovery_trace(
    const parser::ParseResult& parse_res,
    std::string_view filename) {
  std::ostringstream out;
  out << "=== NeuroParse Recovery Trace ===\n";
  out << "File: " << (filename.empty() ? "<input>" : filename) << "\n";
  for (std::size_t i = 0; i < parse_res.errors.size(); ++i) {
    const auto& err = parse_res.errors[i];
    out << "[STEP " << (i + 1) << "]\n";
    out << "  [ERROR] " << err.format(filename) << "\n";
    if (i < parse_res.recoveries.size()) {
      const auto& rec = parse_res.recoveries[i];
      out << "  [STRATEGY] " << recovery::to_string(rec.strategy) << "\n";
      out << "  [ACTION] " << recovery::to_string(rec.action) << "\n";
      out << "  [STATUS] " << (rec.success ? "ACCEPTED & APPLIED" : "REJECTED / FAILED") << "\n";
      out << "  [DESCRIPTION] " << rec.description << "\n";
      out << "  [LATENCY] " << rec.latency_us << " µs\n";
      out << "  [PARSER RESUME] index " << rec.original_token_index << " -> " << rec.resulting_token_index << "\n";
    }
    out << "\n";
  }
  return out.str();
}

std::string escape_json(std::string_view s) {
  std::ostringstream o;
  for (char c : s) {
    switch (c) {
      case '"': o << "\\\""; break;
      case '\\': o << "\\\\"; break;
      case '\b': o << "\\b"; break;
      case '\f': o << "\\f"; break;
      case '\n': o << "\\n"; break;
      case '\r': o << "\\r"; break;
      case '\t': o << "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) <= 0x1f) {
          o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
        } else {
          o << c;
        }
    }
  }
  return o.str();
}

std::string format_json_diagnostics(
    std::string_view filename,
    const std::vector<lexer::LexerError>& lex_errors,
    const parser::ParseResult* parse_res) {
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": \"1.0.0\",\n";
  out << "  \"file\": \"" << escape_json(filename) << "\",\n";
  bool success = lex_errors.empty() && (parse_res == nullptr || parse_res->ok());
  out << "  \"success\": " << (success ? "true" : "false") << ",\n";

  // Metrics
  out << "  \"metrics\": {\n";
  if (parse_res) {
    out << "    \"total_errors\": " << parse_res->metrics.total_errors << ",\n";
    out << "    \"recovered_errors\": " << parse_res->metrics.recovered_errors << ",\n";
    out << "    \"failed_recoveries\": " << parse_res->metrics.failed_recoveries << ",\n";
    out << "    \"cascading_errors\": " << parse_res->metrics.cascading_errors << ",\n";
    out << std::fixed << std::setprecision(4);
    out << "    \"recovery_rate\": " << parse_res->metrics.recovery_rate() << ",\n";
    out << std::fixed << std::setprecision(2);
    out << "    \"total_latency_us\": " << parse_res->metrics.total_latency_us << "\n";
  } else {
    out << "    \"total_errors\": " << lex_errors.size() << ",\n";
    out << "    \"recovered_errors\": 0,\n";
    out << "    \"failed_recoveries\": " << lex_errors.size() << ",\n";
    out << "    \"cascading_errors\": 0,\n";
    out << "    \"recovery_rate\": 0.0,\n";
    out << "    \"total_latency_us\": 0.0\n";
  }
  out << "  },\n";

  // Diagnostics list
  out << "  \"diagnostics\": [\n";
  bool first_diag = true;

  // Lexer errors
  for (const auto& err : lex_errors) {
    if (!first_diag) out << ",\n";
    first_diag = false;
    out << "    {\n";
    out << "      \"source\": \"lexer\",\n";
    out << "      \"severity\": \"error\",\n";
    out << "      \"message\": \"" << escape_json(err.message) << "\",\n";
    out << "      \"line\": " << err.line() << ",\n";
    out << "      \"column\": " << err.column() << ",\n";
    out << "      \"end_line\": " << err.line() << ",\n";
    out << "      \"end_column\": " << (err.column() + 1) << ",\n";
    out << "      \"code\": \"LEX001\"\n";
    out << "    }";
  }

  // Parser errors
  if (parse_res) {
    for (std::size_t i = 0; i < parse_res->errors.size(); ++i) {
      const auto& err = parse_res->errors[i];
      if (!first_diag) out << ",\n";
      first_diag = false;
      out << "    {\n";
      out << "      \"source\": \"parser\",\n";
      out << "      \"severity\": \"error\",\n";
      out << "      \"message\": \"" << escape_json(err.message) << "\",\n";
      out << "      \"line\": " << err.line() << ",\n";
      out << "      \"column\": " << err.column() << ",\n";
      std::size_t tok_len = err.token.lexeme.empty() ? 1 : err.token.lexeme.length();
      out << "      \"end_line\": " << err.line() << ",\n";
      out << "      \"end_column\": " << (err.column() + tok_len) << ",\n";
      out << "      \"code\": \"SYN001\",\n";
      out << "      \"construct\": \"" << escape_json(err.construct) << "\",\n";
      
      out << "      \"expected_tokens\": [";
      for (std::size_t j = 0; j < err.expected_tokens.size(); ++j) {
        if (j > 0) out << ", ";
        out << "\"" << escape_json(lexer::to_string(err.expected_tokens[j])) << "\"";
      }
      out << "]";

      // Recovery info if present
      if (i < parse_res->recoveries.size()) {
        const auto& rec = parse_res->recoveries[i];
        out << ",\n      \"recovery\": {\n";
        out << "        \"strategy\": \"" << recovery::to_string(rec.strategy) << "\",\n";
        out << "        \"action\": \"" << recovery::to_string(rec.action) << "\",\n";
        out << "        \"status\": \"" << (rec.success ? "RECOVERED" : "FAILED") << "\",\n";
        out << "        \"description\": \"" << escape_json(rec.description) << "\",\n";
        out << std::fixed << std::setprecision(2);
        out << "        \"latency_us\": " << rec.latency_us << ",\n";
        out << "        \"is_cascading\": " << (rec.is_cascading ? "true" : "false");

        // Compute suggested edit for Quick Fix
        if (rec.action == recovery::RecoveryActionType::InsertToken && !rec.tokens_inserted.empty()) {
          out << ",\n        \"suggested_edit\": {\n";
          out << "          \"operation\": \"insert\",\n";
          out << "          \"line\": " << err.line() << ",\n";
          out << "          \"column\": " << err.column() << ",\n";
          out << "          \"end_line\": " << err.line() << ",\n";
          out << "          \"end_column\": " << err.column() << ",\n";
          out << "          \"new_text\": \"" << escape_json(rec.tokens_inserted[0].lexeme) << "\"\n";
          out << "        }";
        } else if ((rec.action == recovery::RecoveryActionType::DeleteToken || rec.action == recovery::RecoveryActionType::SkipTokens) && !rec.tokens_skipped.empty()) {
          const auto& first = rec.tokens_skipped.front();
          const auto& last = rec.tokens_skipped.back();
          out << ",\n        \"suggested_edit\": {\n";
          out << "          \"operation\": \"delete\",\n";
          out << "          \"line\": " << first.line() << ",\n";
          out << "          \"column\": " << first.column() << ",\n";
          out << "          \"end_line\": " << last.line() << ",\n";
          out << "          \"end_column\": " << (last.column() + last.lexeme.length()) << ",\n";
          out << "          \"new_text\": \"\"\n";
          out << "        }";
        } else if (rec.action == recovery::RecoveryActionType::ReplaceToken && !rec.tokens_inserted.empty()) {
          out << ",\n        \"suggested_edit\": {\n";
          out << "          \"operation\": \"replace\",\n";
          out << "          \"line\": " << err.line() << ",\n";
          out << "          \"column\": " << err.column() << ",\n";
          out << "          \"end_line\": " << err.line() << ",\n";
          out << "          \"end_column\": " << (err.column() + tok_len) << ",\n";
          out << "          \"new_text\": \"" << escape_json(rec.tokens_inserted[0].lexeme) << "\"\n";
          out << "        }";
        }
        out << "\n      }";
      }
      out << "\n    }";
    }
  }

  out << "\n  ]\n";
  out << "}\n";
  return out.str();
}

bool read_source_input(const std::string& input_path, std::string& source, std::string& error_message) {
  if (input_path == "-") {
    source = std::string((std::istreambuf_iterator<char>(std::cin)),
                         std::istreambuf_iterator<char>());
    return true;
  }
  std::ifstream file(input_path, std::ios::binary);
  if (!file.is_open()) {
    error_message = "could not open file: " + input_path;
    return false;
  }
  source = std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  return true;
}

}  // namespace

std::string help_text() {
  std::ostringstream out;
  out << "neuroparse " << version_string() << "\n"
      << "Neural error recovery inside a compiler parsing pipeline.\n\n"
      << "Usage:\n"
      << "  neuroparse <file.nl | ->\n"
      << "  neuroparse check <file.nl | ->\n"
      << "  neuroparse --explain <file.nl | ->\n"
      << "  neuroparse --explain-recovery <file.nl | ->\n"
      << "  neuroparse --trace-recovery <file.nl | ->\n"
      << "  neuroparse --recovery=<panic|heuristic|neural|auto|none> <file.nl | ->\n"
      << "  neuroparse --recovery-mode=<panic|heuristic|neural|auto|none> <file.nl | ->\n"
      << "  neuroparse --dump-tokens <file.nl | ->\n"
      << "  neuroparse --dump-ast <file.nl | ->\n"
      << "  neuroparse --dump-context <file.nl | ->\n"
      << "  neuroparse --diagnostics=json <file.nl | ->\n"
      << "  neuroparse --version\n"
      << "  neuroparse --help\n";
  return out.str();
}

Options parse_args(const std::vector<std::string_view>& args) {
  Options options;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    if (arg == "--help" || arg == "-h") {
      options.command = Command::Help;
    } else if (arg == "--version" || arg == "-V") {
      options.command = Command::Version;
    } else if (arg == "check") {
      options.command = Command::Check;
    } else if (arg == "--explain" || arg == "--explain-recovery") {
      options.explain_recovery = true;
      options.command = Command::Explain;
    } else if (arg == "--trace-recovery") {
      options.trace_recovery = true;
    } else if (arg == "--dump-tokens") {
      options.command = Command::DumpTokens;
    } else if (arg == "--dump-ast") {
      options.command = Command::DumpAst;
    } else if (arg == "--dump-context") {
      options.command = Command::DumpContext;
    } else if (arg == "--show-repair") {
      options.command = Command::ShowRepair;
    } else if (arg == "--benchmark") {
      options.command = Command::Benchmark;
      if (i + 1 < args.size()) {
        options.benchmark_dir = std::string(args[++i]);
      }
    } else if (arg == "--json" || arg == "--diagnostics=json" || arg == "--output-format=json") {
      options.output_json = true;
    } else if (arg.rfind("--recovery=", 0) == 0 || arg.rfind("--recovery-mode=", 0) == 0) {
      std::string_view value;
      if (arg.rfind("--recovery=", 0) == 0) {
        value = arg.substr(11);
      } else {
        value = arg.substr(16);
      }
      if (value == "panic") {
        options.recovery = RecoveryMode::Panic;
      } else if (value == "heuristic") {
        options.recovery = RecoveryMode::Heuristic;
      } else if (value == "neural") {
        options.recovery = RecoveryMode::Neural;
      } else if (value == "none") {
        options.recovery = RecoveryMode::None;
      } else {
        options.recovery = RecoveryMode::Auto;
      }
    } else if (arg == "--recovery" || arg == "--recovery-mode") {
      if (i + 1 < args.size()) {
        const std::string_view value = args[++i];
        if (value == "panic") {
          options.recovery = RecoveryMode::Panic;
        } else if (value == "heuristic") {
          options.recovery = RecoveryMode::Heuristic;
        } else if (value == "neural") {
          options.recovery = RecoveryMode::Neural;
        } else if (value == "none") {
          options.recovery = RecoveryMode::None;
        } else {
          options.recovery = RecoveryMode::Auto;
        }
      }
    } else if (!arg.empty() && (arg == "-" || arg.front() != '-')) {
      options.input_path = std::string(arg);
    }
  }
  return options;
}

CliResult run(const Options& options) {
  CliResult result;
  switch (options.command) {
    case Command::Help:
      result.stdout_text = help_text();
      return result;
    case Command::Version:
      result.stdout_text = std::string(version_string()) + "\n";
      return result;
    case Command::DumpTokens: {
      if (options.input_path.empty()) {
        result.exit_code = 1;
        result.stderr_text = "neuroparse: error: no input file specified for --dump-tokens\n";
        return result;
      }
      std::string source;
      std::string err_msg;
      if (!read_source_input(options.input_path, source, err_msg)) {
        result.exit_code = 1;
        result.stderr_text = "neuroparse: error: " + err_msg + "\n";
        return result;
      }
      neuroparse::lexer::Lexer lexer(source);
      auto lex_res = lexer.tokenize();

      std::ostringstream out;
      for (const auto& tok : lex_res.tokens) {
        out << "[" << tok.line() << ":" << tok.column() << "] "
            << neuroparse::lexer::to_category_string(tok.category()) << " "
            << neuroparse::lexer::to_string(tok.type) << " "
            << "\"" << tok.lexeme << "\"\n";
      }
      result.stdout_text = out.str();

      if (lex_res.has_errors()) {
        std::ostringstream err;
        for (const auto& e : lex_res.errors) {
          err << e.format(options.input_path) << "\n";
        }
        result.stderr_text = err.str();
        result.exit_code = 1;
      } else {
        result.exit_code = 0;
      }
      return result;
    }
    case Command::Compile:
    case Command::Check:
    case Command::Explain: {
      if (options.input_path.empty()) {
        result.exit_code = 1;
        if (options.output_json) {
          result.stdout_text = "{\n  \"schema_version\": \"1.0.0\",\n  \"file\": \"\",\n  \"success\": false,\n  \"metrics\": {\"total_errors\": 1, \"recovered_errors\": 0, \"failed_recoveries\": 1, \"cascading_errors\": 0, \"recovery_rate\": 0.0, \"total_latency_us\": 0.0},\n  \"diagnostics\": [{\"source\": \"cli\", \"severity\": \"error\", \"message\": \"no input file specified\", \"line\": 1, \"column\": 1, \"end_line\": 1, \"end_column\": 1, \"code\": \"CLI001\"}]\n}\n";
        } else {
          result.stderr_text = "neuroparse: error: no input file specified\n";
        }
        return result;
      }
      std::string source;
      std::string err_msg;
      if (!read_source_input(options.input_path, source, err_msg)) {
        result.exit_code = 1;
        if (options.output_json) {
          result.stdout_text = "{\n  \"schema_version\": \"1.0.0\",\n  \"file\": \"" + escape_json(options.input_path) + "\",\n  \"success\": false,\n  \"metrics\": {\"total_errors\": 1, \"recovered_errors\": 0, \"failed_recoveries\": 1, \"cascading_errors\": 0, \"recovery_rate\": 0.0, \"total_latency_us\": 0.0},\n  \"diagnostics\": [{\"source\": \"cli\", \"severity\": \"error\", \"message\": \"" + escape_json(err_msg) + "\", \"line\": 1, \"column\": 1, \"end_line\": 1, \"end_column\": 1, \"code\": \"CLI002\"}]\n}\n";
        } else {
          result.stderr_text = "neuroparse: error: " + err_msg + "\n";
        }
        return result;
      }
      neuroparse::lexer::Lexer lexer(source);
      auto lex_res = lexer.tokenize();

      if (lex_res.has_errors()) {
        if (options.output_json) {
          result.stdout_text = format_json_diagnostics(options.input_path, lex_res.errors, nullptr);
          result.exit_code = 1;
          return result;
        }
        std::ostringstream err;
        for (const auto& e : lex_res.errors) {
          err << e.format(options.input_path) << "\n";
        }
        result.stderr_text = err.str();
        result.exit_code = 1;
        return result;
      }

      neuroparse::parser::Parser parser(std::move(lex_res.tokens), options.recovery);
      auto parse_res = parser.parse();

      if (options.output_json) {
        result.stdout_text = format_json_diagnostics(options.input_path, {}, &parse_res);
        result.exit_code = parse_res.has_errors() ? 1 : 0;
        return result;
      }

      if (options.trace_recovery) {
        result.stdout_text = format_recovery_trace(parse_res, options.input_path);
      } else if (options.explain_recovery || options.command == Command::Explain) {
        result.stdout_text = format_explanation(parse_res, options.input_path);
      }

      if (parse_res.has_errors()) {
        std::ostringstream err;
        for (const auto& e : parse_res.errors) {
          err << e.format(options.input_path) << "\n";
        }
        result.stderr_text = err.str();
        result.exit_code = 1;
        return result;
      }

      result.exit_code = 0;
      return result;
    }
    case Command::DumpAst: {
      if (options.input_path.empty()) {
        result.exit_code = 1;
        result.stderr_text = "neuroparse: error: no input file specified for --dump-ast\n";
        return result;
      }
      std::string source;
      std::string err_msg;
      if (!read_source_input(options.input_path, source, err_msg)) {
        result.exit_code = 1;
        result.stderr_text = "neuroparse: error: " + err_msg + "\n";
        return result;
      }
      neuroparse::lexer::Lexer lexer(source);
      auto lex_res = lexer.tokenize();

      if (lex_res.has_errors()) {
        std::ostringstream err;
        for (const auto& e : lex_res.errors) {
          err << e.format(options.input_path) << "\n";
        }
        result.stderr_text = err.str();
        result.exit_code = 1;
        return result;
      }

      neuroparse::parser::Parser parser(std::move(lex_res.tokens), options.recovery);
      auto parse_res = parser.parse();

      if (parse_res.program) {
        result.stdout_text = neuroparse::ast::dump_ast(*parse_res.program);
      }

      if (parse_res.has_errors()) {
        std::ostringstream err;
        for (const auto& e : parse_res.errors) {
          err << e.format(options.input_path) << "\n";
        }
        result.stderr_text = err.str();
        result.exit_code = 1;
        return result;
      }

      result.exit_code = 0;
      return result;
    }
    case Command::DumpContext: {
      if (options.input_path.empty()) {
        result.exit_code = 1;
        result.stderr_text = "neuroparse: error: no input file specified for --dump-context\n";
        return result;
      }
      std::string source;
      std::string err_msg;
      if (!read_source_input(options.input_path, source, err_msg)) {
        result.exit_code = 1;
        result.stderr_text = "neuroparse: error: " + err_msg + "\n";
        return result;
      }
      neuroparse::lexer::Lexer lexer(source);
      auto lex_res = lexer.tokenize();

      if (lex_res.has_errors()) {
        std::ostringstream err;
        for (const auto& e : lex_res.errors) {
          err << e.format(options.input_path) << "\n";
        }
        result.stderr_text = err.str();
        result.exit_code = 1;
        return result;
      }

      neuroparse::parser::Parser parser(lex_res.tokens, options.recovery);
      auto parse_res = parser.parse();

      if (parse_res.errors.empty()) {
        result.stdout_text = "neuroparse: no syntax errors in '" + options.input_path + "'.\n";
        result.exit_code = 0;
        return result;
      }

      neuroparse::recovery::ContextExtractor extractor;
      auto contexts = extractor.extract_all(lex_res.tokens, parse_res.errors);

      std::ostringstream out;
      for (std::size_t i = 0; i < contexts.size(); ++i) {
        if (i > 0) out << "\n----------------------------------------\n\n";
        out << contexts[i].format(options.input_path);
      }
      result.stdout_text = out.str();
      result.exit_code = 1;
      return result;
    }
    case Command::ShowRepair:
    case Command::Benchmark:
      result.exit_code = 2;
      result.stderr_text = unimplemented("neural inspection and benchmarks");
      return result;
  }
  result.exit_code = 2;
  result.stderr_text = unimplemented("unknown command");
  return result;
}

}  // namespace neuroparse::cli
