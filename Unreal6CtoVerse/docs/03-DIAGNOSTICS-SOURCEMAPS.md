# Two-Stage Diagnostics & Source Maps

For a transpiled language to succeed in production game development, **a developer must never be forced to debug generated `.verse` files**.

`cvc` guarantees this via a two-stage diagnostic architecture and bidirectional JSON source maps.

---

## Stage 1: Front-End Semantic Guardrails (`cvc`)

`cvc` catches invalid C++ idioms, mutability violations, and unhandled failure/async contexts *before* emitting `.verse`. Diagnostics are formatted using standard MSVC/Clang conventions so Visual Studio and VS Code parse them natively:

```text
C:\Project\Source\Inventory.cv(18,9): error CV2008: Unhandled failable call 'GrantItemToPlayer(...)'. Append '?' to propagate failure (requires 'failable' function) or wrap in 'if (...)'.
```

### Diagnostic Code Ranges
* `CV1000–CV1999`: Lexical and syntax errors (including helpful hints when raw pointers `*` or `throw` are encountered).
* `CV2000–CV2999`: Type checking, mutability (`mut`), and effect inference errors.
* `CV3000–CV3999`: UE6 Scene Graph & interop validation warnings.

---

## Stage 2: Downstream Verse Compiler Error Translation

When Unreal Engine's Verse compiler (`versec` / UBT) compiles the generated `.verse` files, external API type mismatches or Verse-specific constraints may still surface.

`cvc` emits a `.verse.map` file alongside every generated `.verse` file:

```json
{
  "version": 1,
  "sourceFile": "Source/Inventory.cv",
  "generatedFile": "Intermediate/Verse/Inventory.verse",
  "mappings": [
    { "genLine": 12, "genCol": 17, "srcLine": 14, "srcCol": 9, "symbol": "PurchaseItem" },
    { "genLine": 13, "genCol": 17, "srcLine": 16, "srcCol": 9, "symbol": "Gold" },
    { "genLine": 15, "genCol": 17, "srcLine": 19, "srcCol": 9, "symbol": "GrantItemToPlayer" }
  ]
}
```

### Build Wrapper (`cvc --translate-errors`)
In `CV.Build.targets`, the Verse build output is piped through `cvc --translate-errors`:
1. Regex matches Verse compiler diagnostic lines: `^(?<file>.*\.verse)\((?<line>\d+),(?<col>\d+)\):\s*(?<sev>error|warning)\s*(?<code>\w+):\s*(?<msg>.*)$`.
2. Looks up `(file, line, col)` in the corresponding `.verse.map` using binary search on `genLine`.
3. Rewrites the file path, line, and column back to `Inventory.cv(19,9)` and prints the translated diagnostic to `stderr` so Visual Studio jumps straight to the `.cv` source line.
