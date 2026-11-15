#pragma once

#include "Token.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cvc {

enum class AccessModifier : uint8_t {
    Default,
    Public,
    Private,
    Protected,
    Internal
};

enum EffectFlags : uint32_t {
    EFFECT_NONE      = 0,
    EFFECT_COMPUTES  = 1 << 0,
    EFFECT_READS     = 1 << 1,
    EFFECT_TRANSACTS = 1 << 2,
    EFFECT_DECIDES   = 1 << 3,
    EFFECT_SUSPENDS  = 1 << 4
};

// AST Nodes omitted for brevity in stub
struct Stmt {
    SourceLocation loc;
    virtual ~Stmt() = default;
};

} // namespace cvc
