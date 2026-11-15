# CV Language Specification (v0.1 Draft)

## 1. Design Principles
1. **Semantic Fidelity:** Every construct in CV has a 1-to-1, deterministic lowering to valid Verse code. 
2. **Familiar Surface Syntax:** Braces `{}` for blocks, semicolons `;` for statements, C#/C++20 type declarations, and standard infix operators (`&&`, `||`, `!`, `==`, `!=`).
3. **Implicit Mutability, Implicit Effects:** Variables are mutable by default (C++ style), use `const` for immutability. Verse effect tags are inferred automatically.
