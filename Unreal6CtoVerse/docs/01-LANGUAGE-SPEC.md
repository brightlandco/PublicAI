# CV Language Specification (v0.1 Draft)

## 1. Design Principles

1. **Semantic Fidelity:** Every construct in CV has a 1-to-1, deterministic lowering to valid Verse code. CV never emulates C++ features (such as raw pointer arithmetic or manual heap allocation) that violate Verse's transactional memory model.
2. **Familiar Surface Syntax:** Braces `{}` for blocks, semicolons `;` for statements, C#/C++20 type declarations, and standard infix operators (`&&`, `||`, `!`, `==`, `!=`).
3. **Explicit Mutability, Implicit Effects:** Mutability is opt-in (`mut`), matching Verse's `var`/`set` model, while Verse effect tags (`<reads>`, `<transacts>`, `<computes>`) are inferred automatically.

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

---

## 4. Variables & Mutability

In Verse, bindings are immutable by default (`Name : type = val`), and mutable bindings require `var` at declaration and `set` on every assignment.

```cpp
// CV
const float Gravity = 980.0;        // -> Gravity : float = 980.0
float speed = 100.0;                // -> Speed : float = 100.0 (immutable)
mut float currentHealth = 100.0;    // -> var CurrentHealth : float = 100.0
currentHealth -= 25.0;              // -> set CurrentHealth -= 25.0
```

Compound assignments (`+=`, `-=`, `*=`, `/=`) on `mut` variables automatically emit `set Var op= Expr`.

---

## 5. Failure-as-Control-Flow (`failable`, `?`, `fail`)

Verse replaces exceptions with **failure contexts** (`<decides>`). Any expression that can fail must execute inside a failure context (either another `<decides>` function or the condition of an `if` / `for` expression).

### Declaring Failable Functions
```cpp
public failable int ParsePositive(int value) {
    if (value <= 0) fail;
    return value;
}
```
Emits:
```verse
ParsePositive<public>(Value : int)<decides><computes> : int =
    Value > 0
    return Value
```

### Calling Failable Functions
1. **Propagating Failure (`?` suffix):** Valid only inside a `failable` function or failure condition. Lowers to Verse's bracket call syntax `Func[Args]`.
   ```cpp
   int x = ParsePositive(input)?; // -> X : int = ParsePositive[Input]
   ```
2. **Handling Failure (`if` binding):** Valid inside any function (including non-failable `void` or `async` functions). Lowers to Verse's `if (X := Func[Args]):`.
   ```cpp
   if (int x = ParsePositive(input)) {
       UseValue(x);
   } else {
       HandleInvalid();
   }
   ```

---

## 6. Structured Concurrency (`async`, `await`, `concurrent`, `race`, `spawn`)

| CV Construct | Verse Lowering | Notes |
| :--- | :--- | :--- |
| `async ReturnType Fn(...)` | `Fn(...)<suspends> : return_type` | Marks coroutine function |
| `await Expr;` | `Expr` | Explicitly documents suspension points in CV |
| `spawn Fn();` | `spawn { Fn() }` | Launches concurrent task without awaiting |
| `concurrent { A(); B(); }` | `sync: A() B()` | Runs branches concurrently; completes when all finish |
| `race { A(); B(); }` | `race: A() B()` | Runs branches concurrently; cancels losers when first finishes |
| `rush { A(); B(); }` | `rush: A() B()` | Completes when first finishes; losers continue running |

---

## 7. Access Specifiers & UE6 Attributes

* `public` -> `<public>`
* `private` -> `<private>`
* `protected` -> `<protected>`
* `internal` -> `<internal>`
* `[[editable]]` -> `@editable`
* `[[override]]` -> `override` modifier on method declaration
* `[[abstract]]` -> `<abstract>` class/method specifier
