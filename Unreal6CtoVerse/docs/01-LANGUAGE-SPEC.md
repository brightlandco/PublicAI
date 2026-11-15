# CV Language Specification (v0.1 Draft)

## 1. Design Principles

1. **Semantic Fidelity:** Every construct in CV has a 1-to-1, deterministic lowering to valid Verse code. CV never emulates C++ features (such as raw pointer arithmetic or manual heap allocation) that violate Verse's transactional memory model.
2. **Familiar Surface Syntax:** Braces `{}` for blocks, semicolons `;` for statements, C#/C++20 type declarations, and standard infix operators (`&&`, `||`, `!`, `==`, `!=`).
3. **Implicit Mutability, Implicit Effects:** Variables are mutable by default (C++ style), use `const` for immutability, matching Verse's `var`/`set` model, while Verse effect tags (`<reads>`, `<transacts>`, `<computes>`) are inferred automatically.

---

## 2. Lexical & Naming Conventions

* **Comments:** `// single-line` and `/* multi-line */` translate to `#` and `<# ... #>` in Verse.
* **Identifiers:** Preserved verbatim for variables, functions, and fields. Type names written in `PascalCase` in CV automatically normalize to Verse's standard `snake_case` convention when emitting type definitions (configurable via `--preserve-casing`), while external UE6/Verse API identifiers retain exact symbol bindings.
* **Namespaces:** File-scoped `namespace A::B::C;` or block-scoped `namespace A::B { ... }` lowers to nested Verse `module:` declarations with `<public>` visibility.

---

## 3. Type System Mapping

| CV Type | Verse Type | Semantics |
| :--- | :--- | :--- |
| `void` | `void` | Unit / no return value |
| `bool` | `logic` | Boolean (`true` / `false`) |
| `int` | `int` | Arbitrary-precision signed integer |
| `float` | `float` | IEEE 754 64-bit floating-point |
| `string` | `string` | Immutable UTF-8 string |
| `T?` | `?t` | Option type (`null` -> `false`, value -> `option{v}`) |
| `T[]` | `[]t` | Dynamic immutable/copy-on-write array |
| `Map<K, V>` | `[k]v` | Associative map |
| `WeakMap<K, V>` | `weak_map(k, v)` | Session/module-scoped weak reference map |
| `struct S` | `s := struct` | Pure value type (passed by value, structural equality) |
| `class C` | `c := class` | Reference type (managed lifetime, single inheritance) |
| `interface I` | `i := interface` | Abstract interface (multiple inheritance supported) |
| `enum E` | `e := enum` | Closed enumeration |

### Prohibited C++ Constructs (Compile-Time Error)
* Raw pointers (`T*`), address-of (`&x`), and pointer dereference (`*ptr`) — use `class` reference semantics or `T?` optionals.
* Unbounded `while` loops without `suspends` or bounded termination guarantees — use `loop { ... break; }`.
* `try / catch / throw` exceptions — use `failable` functions, `fail;`, and `if (auto x = ...)` failure contexts.
