#include "cvc/Diagnostics.hpp"
#include "cvc/Lexer.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: cvc <input.cv> [-o <output.verse>] [--sourcemap]\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::ifstream in(inputPath);
    if (!in) {
        std::cerr << "cvc: error: could not open input file '" << inputPath << "'\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();

    cvc::DiagnosticEngine diag;
    cvc::Lexer lexer(inputPath, buffer.str(), diag);
    auto tokens = lexer.tokenize();

    if (diag.hasErrors()) {
        diag.flush();
        return 1;
    }

    std::cout << "cvc: tokenized " << tokens.size() << " tokens from " << inputPath << "\n";
    return 0;
}
