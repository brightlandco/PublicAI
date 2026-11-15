# Static Effect Inference in `cvc`

One of Verse's steepest learning curves is annotating every function with accurate effect specifiers (`<computes>`, `<reads>`, `<writes>`, `<allocates>`, `<transacts>`, `<decides>`, `<suspends>`). Getting an effect tag wrong produces cascading compiler errors across callers.

`cvc` eliminates 90% of manual effect annotations through **two-pass bottom-up call-graph analysis**.

---

## 1. Explicit vs. Inferred Specifiers

The programmer only writes **control-flow-altering** qualifiers in CV:
* `failable` -> adds `<decides>`
* `async` -> adds `<suspends>`
* `const` (on member methods) -> asserts no state mutation (`<reads>` or `<computes>`)

All memory-effect specifiers are inferred automatically from the function body and transitive callees.
