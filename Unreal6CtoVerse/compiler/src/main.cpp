#include "cvc/Diagnostics.hpp"
#include "cvc/Lexer.hpp"
#include "cvc/Parser.hpp"
#include "cvc/VerseEmitter.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: cvc <input.cv> [-o <output.verse>] [--sourcemap]\n";
    return 1;
  }

  std::string inputPath = argv[1];
  std::string outputPath;
  bool sourcemap = false;
  (void)sourcemap;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc) outputPath = argv[++i];
    else if (arg == "--sourcemap") sourcemap = true;
  }

  std::ifstream in(inputPath, std::ios::binary);
  if (!in) {
    std::cerr << "cvc: error: could not open input file '" << inputPath << "'\n";
    return 1;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  std::string source = buffer.str();

  cvc::DiagnosticEngine diag;
  cvc::Lexer lexer(inputPath, source, diag);
  auto tokens = lexer.tokenize();
  if (diag.hasErrors()) { diag.flush(); return 1; }

  cvc::Parser parser(tokens, diag);
  auto ast = parser.parse();
  if (diag.hasErrors()) { diag.flush(); return 1; }
  std::cout << "cvc: parsed " << inputPath << " successfully.\n";

  if (!outputPath.empty()) {
    std::filesystem::path outPath(outputPath);
    if (outPath.has_parent_path()) std::filesystem::create_directories(outPath.parent_path());
    std::ofstream out(outputPath);
    if (!out) { std::cerr << "cvc: error: could not open output file '" << outputPath << "'\n"; return 1; }
    cvc::VerseEmitter emitter;
    emitter.emit(ast, out);
    std::cout << "cvc: generated " << outputPath << "\n";
  }
  return 0;
}
