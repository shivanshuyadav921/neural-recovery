#include "neuroparse/recovery/error_context.hpp"

#include <sstream>

namespace neuroparse::recovery {

std::string ErrorContext::format(std::string_view filename) const {
  std::ostringstream ss;
  if (!filename.empty()) {
    ss << filename << ":";
  }
  ss << error_location.line << ":" << error_location.column << ": Error Context";
  if (!diagnostic_message.empty()) {
    ss << " (" << diagnostic_message << ")";
  }
  ss << "\n";

  ss << "  Token index: " << error_token_index << " of " << total_tokens
     << " (Window: [" << window_start_index << ", " << window_end_index << "))\n";

  if (!construct.empty()) {
    ss << "  Construct: " << construct << "\n";
  }

  ss << "\n  Tokens before (" << tokens_before.size() << "):\n";
  if (tokens_before.empty()) {
    ss << "    <none>\n";
  } else {
    for (const auto& tok : tokens_before) {
      ss << "    [" << tok.line() << ":" << tok.column() << "] "
         << lexer::to_string(tok.type) << " \"" << tok.lexeme << "\"\n";
    }
  }

  ss << "\n  Current error token:\n";
  ss << "    [" << error_token.line() << ":" << error_token.column() << "] "
     << lexer::to_string(error_token.type) << " \"" << error_token.lexeme << "\"\n";

  ss << "\n  Tokens after (" << tokens_after.size() << "):\n";
  if (tokens_after.empty()) {
    ss << "    <none>\n";
  } else {
    for (const auto& tok : tokens_after) {
      ss << "    [" << tok.line() << ":" << tok.column() << "] "
         << lexer::to_string(tok.type) << " \"" << tok.lexeme << "\"\n";
    }
  }

  if (!expected_tokens.empty()) {
    ss << "\n  Expected terminals:\n    ";
    for (std::size_t i = 0; i < expected_tokens.size(); ++i) {
      if (i > 0) ss << ", ";
      ss << "'" << lexer::to_string(expected_tokens[i]) << "'";
    }
    ss << "\n";
  }

  return ss.str();
}

}  // namespace neuroparse::recovery
