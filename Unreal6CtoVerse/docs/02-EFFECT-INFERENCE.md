# Static Effect Inference in `cvc`

One of Verse's steepest learning curves is annotating every function with accurate effect specifiers (`<computes>`, `<reads>`, `<writes>`, `<allocates>`, `<transacts>`, `<decides>`, `<suspends>`). Getting an effect tag wrong produces cascading compiler errors across callers.

`cvc` eliminates 90% of manual effect annotations through **two-pass bottom-up call-graph analysis**.

---

## 1. Explicit vs. Inferred Specifiers

The programmer only writes **control-flow-altering** qualifiers in CV:
* `failable` -> adds `<decides>`
* `async` -> adds `<suspends>`
* `const` (on member methods) -> asserts no state mutation (`<reads>` or `<computes>`)

All memory-effect specifiers are inferred automatically from the function body and transitive callees:

| Observed Operations in Body / Callees | Inferred Verse Effect |
| :--- | :--- |
| Pure arithmetic, struct construction, no mutable reads/writes | `<computes>` |
| Reads `mut` fields, `WeakMap` lookups, or calls `<reads>` functions | `<reads>` |
| Mutates `mut` fields, mutates `WeakMap`, instantiates classes (`new`), or calls `<transacts>` functions | `<transacts>` |
| Marked `failable` | `<decides>` + (`<transacts>` if mutating, else `<reads>`/`<computes>`) |
| Marked `async` | `<suspends>` (note: Verse `<suspends>` implicitly permits state mutation without `<transacts>`) |

---

## 2. Fixed-Point Propagation Algorithm

1. **Local Effect Extraction:** Each AST function node is scanned for primitive effects:
   * `ReadMutableField` / `WeakMapLookup` -> `EFFECT_READS`
   * `WriteMutableField` / `WeakMapAssign` / `NewClassInstance` -> `EFFECT_TRANSACTS`
   * `FailStatement` / `FailableCall` -> `EFFECT_DECIDES`
   * `AwaitExpr` / `SpawnStmt` / `RaceBlock` / `ConcurrentBlock` -> `EFFECT_SUSPENDS`
2. **Call-Graph Construction:** Build a directed graph $G = (V, E)$ where $(u, v) \in E$ if function $u$ invokes function $v$.
3. **Strongly Connected Components (SCC):** Run Tarjan's SCC algorithm to group mutually recursive functions.
4. **Fixed-Point Iteration:** Propagate effects in reverse topological order until no function's effect bitmask changes:
   $$	ext{Effects}(u) \leftarrow 	ext{LocalEffects}(u) \cup igcup_{v \in 	ext{Callees}(u)} 	ext{Effects}(v)$$
5. **Consistency Validation:**
   * If a method is marked `const` but resolves `EFFECT_TRANSACTS`, emit `error CV2014: const method mutates state`.
   * If a function resolves `EFFECT_DECIDES` without being declared `failable` (and the failable call is not enclosed in an `if (...)` failure handler), emit `error CV2008: unhandled failable expression; mark function 'failable' or wrap in 'if'`.
   * If a function resolves `EFFECT_SUSPENDS` without being declared `async`, emit `error CV2009: 'await' used inside non-async function`.
