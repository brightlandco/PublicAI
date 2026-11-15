# Two-Stage Diagnostics & Source Maps

For a transpiled language to succeed in production game development, **a developer must never be forced to debug generated `.verse` files**.

`cvc` guarantees this via a two-stage diagnostic architecture and bidirectional JSON source maps.

---

## Stage 1: Front-End Semantic Guardrails (`cvc`)

`cvc` catches invalid C++ idioms, mutability violations, and unhandled failure/async contexts *before* emitting `.verse`. Diagnostics are formatted using standard MSVC/Clang conventions so Visual Studio and VS Code parse them natively:

```text
C:\Project\Source\Inventory.cv(18,9): error CV2008: Unhandled failable call 'GrantItemToPlayer(...)'. Append '?' to propagate failure (requires 'failable' function) or wrap in 'if (...)'.
```
