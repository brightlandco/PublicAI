# CV (`.cv`) — C++ & C# Syntax for Unreal Engine 6 Verse

> **Write deterministic, transactional gameplay code in the syntax you already know. Compile cleanly to human-readable `.verse` with zero runtime overhead.**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target: Unreal Engine 6 / Verse](https://img.shields.io/badge/Target-UE6%20Verse-purple.svg)]()
[![Compiler: C++20](https://img.shields.io/badge/Compiler-C%2B%2B20-00599C.svg)]()

---

## ⚠️ Project Status: Initial Prototype

**CV is currently in the proof-of-concept / prototype stage.** 

### What it does exactly (v0.1):
* **Lexical & AST Parsing:** Implements a dependency-free, C++20 hand-rolled Lexer and Recursive-Descent Parser that converts `.cv` source into an Abstract Syntax Tree (AST).
* **Structural Lowering:** Accurately maps C++ idioms to Verse equivalents (e.g., modules, classes, structs, methods, fields) using C++-style block access (`public:`, `private:`).
* **State Mutation:** Translates C++ variables (mutable by default, `const` for immutable) and assignment operators (`-=`, `+=`, `=`) into Verse's strict `var` and `set` keywords.
* **Failure Contexts:** Translates `failable` functions, trailing `?` propagation, `fail;` statements, and C++-style `if (auto val = ...)` bindings into Verse's `false?` and `if (Val := ...):` failure blocks.
* **Deterministic Emitter:** Generates cleanly formatted, 2-space indented `.verse` code natively from the AST.

### Future Direction for Development
To evolve from a working prototype into a full, production-ready language and toolchain, the following milestones are needed:
1. **Semantic Analysis & Type Checking:** Implement a full multi-file symbol table to enforce type safety, validate method calls, and reject invalid C++ idioms *before* emission.
2. **Call-Graph Effect Inference:** Complete the Tarjan SCC fixed-point pass to automatically infer `<reads>`, `<transacts>`, and `<computes>` without the user having to write them.
3. **Comprehensions & Arrays:** Expand the AST to support Verse's powerful `for` comprehensions, parametric types, and dynamic array (`[]t`) operations.
4. **Source Maps (`.verse.map`):** Wire the parser's positional tokens to generate JSON source maps, ensuring downstream Epic compiler errors point directly back to the original `.cv` lines/columns.
5. **LSP Proxy (`cvc-lsp`):** Build the lightweight Language Server Protocol wrapper to provide real-time VS Code / Visual Studio autocomplete and hover support by delegating to Epic's `verse-lsp`.

---

## Why CV?

Unreal Engine 6 introduces software transactional memory, failure-as-control-flow, and structured concurrency to gameplay programming through **Verse** and the **Scene Graph**. Those *runtime semantics* are a major leap forward for deterministic simulation and multiplayer state safety.

However, Verse's *surface syntax* (`:=`, `set`, `[]` failable invocations, `<decides><transacts>` effect tags, and significant whitespace) creates unnecessary friction for veteran C++ and C# engine programmers.

**CV separates Verse's runtime semantics from its surface syntax:**

* **Zero Runtime Overhead:** `cvc` is a pure ahead-of-time source-to-source transpiler (`file.cv` -> `file.verse`). There is no VM, no wrapper runtime, and no FFI marshaling penalty.
* **100% Bidirectional Interop:** Emitted `.verse` code is clean, formatted, and directly callable from native Verse modules—and native Verse APIs are directly callable from `.cv`.
* **Automatic Effect Inference:** Write `failable` or `async`; `cvc` infers `<computes>`, `<reads>`, `<transacts>`, `<decides>`, and `<suspends>` across the call graph (planned).
* **First-Class Visual Studio Integration:** Drop-in MSBuild `.targets` hooks and JSON source maps let you compile on `Ctrl+Shift+B` and double-click errors directly in the Visual Studio Error List.

---

## Side-by-Side Comparison: CV vs. Emitted Verse

### 1. Transactional Rollbacks, Failure Contexts & `weak_map`

In Verse, mutating state inside a failable function automatically rolls back if a subsequent check fails. CV expresses this with clean C++ block-scoped access and control flow while preserving exact Verse transactional semantics:

<table>
<tr>
<th><code>Inventory.cv</code> (What You Write)</th>
<th><code>Inventory.verse</code> (What <code>cvc</code> Emits)</th>
</tr>
<tr>
<td>

```cpp
namespace Game::Economy;

class Wallet {
  private:
    int Gold = 100;
    int Gems = 5;

  public:
    int GetGold() const {
      return Gold;
    }

    // 'failable' + state mutation infers <decides><transacts>
    // If GrantItemToPlayer() fails, Gold and Gems roll back.
    failable void PurchaseItem(
      Player buyer, int goldCost, int gemCost)
    {
      if (Gold < goldCost || Gems < gemCost) fail;

      Gold -= goldCost;
      Gems -= gemCost;

      // Trailing '?' propagates failure & rolls back
      GrantItemToPlayer(buyer)?;
    }
}

WeakMap<Player, Wallet> PlayerWallets;

Wallet GetOrCreateWallet(Player p) {
  if (auto existing = PlayerWallets[p]) {
    return existing;
  }
  auto created = new Wallet();
  PlayerWallets[p] = created;
  return created;
}
```

</td>
<td>

```verse
Game<public> := module:
  Economy<public> := module:
    wallet<public> := class:
      var<private> Gold : int = 100
      var<private> Gems : int = 5

      GetGold<public>()<reads> : int =
        return Gold

      PurchaseItem<public>(
        Buyer : player, GoldCost : int, GemCost : int
      )<decides><transacts> : void =
        if (Gold < GoldCost or Gems < GemCost):
          false?
        set Gold -= GoldCost
        set Gems -= GemCost
        GrantItemToPlayer[Buyer]

    var<public> PlayerWallets : weak_map(player, wallet) = map{}

    GetOrCreateWallet<public>(P : player)<transacts> : wallet =
      if (Existing := PlayerWallets[P]):
        return Existing
      var Created : any = wallet{}
      set PlayerWallets[P] = Created
      return Created
```

</td>
</tr>
</table>

---

### 2. Structured Concurrency (`async`, `await`, `race`, `spawn`)

<table>
<tr>
<th><code>CapturePoint.cv</code> (What You Write)</th>
<th><code>CapturePoint.verse</code> (What <code>cvc</code> Emits)</th>
</tr>
<tr>
<td>

```cpp
namespace Game::Modes;

class CaptureZone : Component {
  public:
    [[editable]] float CaptureTime = 10.0;
    [[editable]] TriggerDevice ZoneTrigger;

    async void RunCaptureLoop() {
      loop {
        Player agent = await ZoneTrigger.AgentEntersEvent.Await();

        race {
          {
            await Sleep(CaptureTime);
            OnZoneCaptured(agent);
          }
          {
            await ZoneTrigger.AgentExitsEvent.Await();
            OnCaptureCancelled();
          }
        }
      }
    }

    void Start() {
      spawn RunCaptureLoop();
    }
}
```

</td>
<td>

```verse
Game<public> := module:
  Modes<public> := module:
    capture_zone<public> := class(component):
      @editable
      CaptureTime<public> : float = 10.0

      @editable
      ZoneTrigger<public> : trigger_device = trigger_device{}

      RunCaptureLoop<public>()<suspends> : void =
        loop:
          Agent : player = ZoneTrigger.AgentEntersEvent.Await()
          race:
            block:
              Sleep(CaptureTime)
              OnZoneCaptured(Agent)
            block:
              ZoneTrigger.AgentExitsEvent.Await()
              OnCaptureCancelled()

      Start<public>() : void =
        spawn { RunCaptureLoop() }
```

</td>
</tr>
</table>

---

## Quickstart (Building & Running `cvc`)

### Prerequisites
* C++20 compiler (MSVC 2022/2026, Clang 16+, or GCC 13+)
* CMake 3.20+

### Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Transpile a `.cv` File

**On macOS/Linux:**
```bash
./build/cvc examples/02_TransactionalCombat/Inventory.cv -o out/Inventory.verse --sourcemap
```

**On Windows:**
```cmd
.\build\Release\cvc.exe examples\02_TransactionalCombat\Inventory.cv -o out\Inventory.verse --sourcemap
```

### Visual Studio Project Integration
Import `ide/visual-studio/CV.Build.targets` into your `.vcxproj` or UE6 project file:
```xml
<Import Project="$(SolutionDir)Tools\cvc\ide\visual-studio\CV.Build.targets" />
```
Pressing **Build (`Ctrl+Shift+B`)** automatically transpiles modified `.cv` files into your Verse source directory before Unreal Build Tool / Verse compilation runs, routing any compiler diagnostics directly into the Visual Studio **Error List**.
