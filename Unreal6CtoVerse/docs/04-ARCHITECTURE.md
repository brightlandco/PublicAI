# Compiler & IDE Tooling Architecture

`cvc` is intentionally designed to be lean, fast, and embeddable—roughly **5,000 to 8,000 lines of dependency-free C++20**.

---

## 1. Compiler Pipeline

```text
  [.cv Source File]
         │
         ▼
  ┌──────────────┐
  │    Lexer     │  Zero-copy string_view tokenization with line/col spans
  └──────┬───────┘
         ▼
  ┌──────────────┐
  │    Parser    │  Recursive-descent LL(k) parser -> Typed AST
  └──────┬───────┘
         ▼
  ┌──────────────┐
  │   Semantic   │  Symbol table, mutability check, Tarjan SCC effect inference
  │  & Effects   │
  └──────┬───────┘
         ▼
  ┌──────────────┐
  │Verse Emitter │  Pretty-printed 4-space indented .verse + JSON .verse.map
  └──────────────┘
```

### Why It's Fast
* **No Preprocessor / Macro Expansion:** Eliminates C++'s biggest compile-time bottleneck.
* **Single-Pass AST + Call-Graph Resolution:** Transpiles thousands of `.cv` files in milliseconds.
* **Incremental Builds:** Compares timestamps/hashes of `.cv` inputs against `.verse` outputs; untouched modules are skipped immediately.

---

## 2. Visual Studio & LSP Integration

Rather than re-implementing a full Verse type-checker inside an IDE extension, CV uses a **thin LSP proxy**:

1. **Syntax Highlighting:** `CV.tmLanguage.json` provides immediate C++-style keyword, type, and attribute highlighting in Visual Studio 2022/2026 and VS Code.
2. **Build Integration:** `CV.Build.targets` hooks into MSBuild `BeforeBuild` / `PreBuildEvent`, running `cvc` incrementally and feeding errors into the native VS Error List.
3. **Hover / Autocomplete (LSP Proxy):**
   * User hovers over a symbol at `(Inventory.cv, line 19, col 12)`.
   * `cvc-lsp` maps `(line 19, col 12)` forward via `.verse.map` to `(Inventory.verse, line 15, col 20)`.
   * Forwards `textDocument/hover` to Epic's official `verse-lsp` process.
   * Translates the returned type signature back into CV syntax (`failable void GrantItemToPlayer(Player)`) and returns it to the editor.
