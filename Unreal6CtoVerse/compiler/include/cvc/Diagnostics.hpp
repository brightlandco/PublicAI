#pragma once

#include "Token.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace cvc {

enum class Severity {
  Warning,
  Error
};

struct Diagnostic {
  std::string file;
  SourceLocation loc;
  Severity severity;
  std::string code;
  std::string message;
};

class DiagnosticEngine {
public:
  void report(std::string file, SourceLocation loc, Severity sev, std::string code, std::string msg) {
    if (sev == Severity::Error) {
      hasErrors_ = true;
    }
    diagnostics_.push_back(Diagnostic{std::move(file), loc, sev, std::move(code), std::move(msg)});
  }

  bool hasErrors() const { return hasErrors_; }

  void flush(std::ostream& out = std::cerr) const {
    for (const auto& d : diagnostics_) {
      out << d.file << "(" << d.loc.line << "," << d.loc.column << "): "
        << (d.severity == Severity::Error ? "error " : "warning ")
        << d.code << ": " << d.message << "\n";
    }
  }

private:
  std::vector<Diagnostic> diagnostics_;
  bool hasErrors_ = false;
};

} // namespace cvc
