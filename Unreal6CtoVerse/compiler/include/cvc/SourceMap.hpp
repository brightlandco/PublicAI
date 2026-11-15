#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cvc {
class SourceMap {
public:
    void add(uint32_t genLine, uint32_t genCol, uint32_t srcLine, uint32_t srcCol, std::string symbol = "");
    std::string toJson(const std::string& sourceFile, const std::string& generatedFile) const;
};
} // namespace cvc
