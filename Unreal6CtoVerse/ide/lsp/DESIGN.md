# CV Language Server Proxy (`cvc-lsp`)

Instead of duplicating Epic's Verse type-checker, `cvc-lsp` acts as a lightweight **source-map translating proxy** sitting between Visual Studio / VS Code and `verse-lsp`:

1. **On Edit (`textDocument/didChange`):** Runs in-memory `cvc` transpilation in < 5ms, updates the `.verse` buffer in `verse-lsp`, and publishes any Stage 1 `CVxxxx` diagnostics immediately.
2. **On Verse Diagnostics (`textDocument/publishDiagnostics`):** Intercepts diagnostics emitted by `verse-lsp` on the backing `.verse` buffer, translates `(genLine, genCol)` -> `(srcLine, srcCol)` via the in-memory `SourceMap`, and forwards them to the client on the `.cv` URI.
3. **On Hover / Completion (`textDocument/hover`, `textDocument/completion`):** Maps the cursor position forward to `.verse`, queries `verse-lsp`, formats the returned Verse type signature into CV syntax, and responds to the IDE.
