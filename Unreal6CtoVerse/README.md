# CV (`.cv`) — C++ & C# Syntax for Unreal Engine 6 Verse

> **Write deterministic, transactional gameplay code in the syntax you already know. Compile cleanly to human-readable `.verse` with zero runtime overhead.**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target: Unreal Engine 6 / Verse](https://img.shields.io/badge/Target-UE6%20Verse-purple.svg)]()
[![Compiler: C++20](https://img.shields.io/badge/Compiler-C%2B%2B20-00599C.svg)]()

---

## Why CV?

Unreal Engine 6 introduces software transactional memory, failure-as-control-flow, and structured concurrency to gameplay programming through **Verse** and the **Scene Graph**. Those *runtime semantics* are a major leap forward for deterministic simulation and multiplayer state safety.

However, Verse's *surface syntax* (`:=`, `set`, `[]` failable invocations, `<decides><transacts>` effect tags, and significant whitespace) creates unnecessary friction for veteran C++ and C# engine programmers.

**CV separates Verse's runtime semantics from its surface syntax:**

* **Zero Runtime Overhead:** `cvc` is a pure ahead-of-time source-to-source transpiler (`file.cv` -> `file.verse`). There is no VM, no wrapper runtime, and no FFI marshaling penalty.
* **100% Bidirectional Interop:** Emitted `.verse` code is clean, formatted, and directly callable from native Verse modules—and native Verse APIs are directly callable from `.cv`.
* **Automatic Effect Inference:** Write `failable` or `async`; `cvc` infers `<computes>`, `<reads>`, `<transacts>`, `<decides>`, and `<suspends>` across the call graph.
* **First-Class Visual Studio Integration:** Drop-in MSBuild `.targets` hooks and JSON source maps let you compile on `Ctrl+Shift+B` and double-click errors directly in the Visual Studio Error List.

---

## Side-by-Side Comparison: CV vs. Emitted Verse

### 1. Transactional Rollbacks, Failure Contexts & `weak_map`

In Verse, mutating state inside a failable function automatically rolls back if a subsequent check fails. CV expresses this with clean C++/C# control flow while preserving exact Verse transactional semantics:

<table>
<tr>
<th><code>Inventory.cv</code> (What You Write)</th>
<th><code>Inventory.verse</code> (What <code>cvc</code> Emits)</th>
</tr>
<tr>
<td>

```cpp
namespace Game::Economy;

public class Wallet {
    private mut int Gold = 100;
    private mut int Gems = 5;

    public int GetGold() const {
        return Gold;
    }

    // 'failable' + state mutation infers <decides><transacts>
    // If GrantItemToPlayer() fails, Gold and Gems roll back.
    public failable void PurchaseItem(
        Player buyer, int goldCost, int gemCost)
    {
        if (Gold < goldCost || Gems < gemCost) fail;

        Gold -= goldCost;
        Gems -= gemCost;

        // Trailing '?' propagates failure & rolls back
        GrantItemToPlayer(buyer)?;
    }
}

private WeakMap<Player, Wallet> PlayerWallets;

public Wallet GetOrCreateWallet(Player p) {
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
                Gold >= GoldCost and Gems >= GemCost
                set Gold -= GoldCost
                set Gems -= GemCost
                GrantItemToPlayer[Buyer]

        var<private> PlayerWallets : weak_map(player, wallet) = map{}

        GetOrCreateWallet<public>(P : player)<transacts> : wallet =
            if (Existing := PlayerWallets[P]):
                return Existing
            Created := wallet{}
            if (set PlayerWallets[P] = Created) {}
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

public class CaptureZone : Component {
    [[editable]] public float CaptureTime = 10.0;
    [[editable]] public TriggerDevice ZoneTrigger;

    public async void RunCaptureLoop() {
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

    public void Start() {
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

## Repository Structure

```text
cvc/
├── README.md                        # Project overview, side-by-side examples & quickstart
├── LICENSE                          # MIT License
├── CMakeLists.txt                   # C++20 build configuration for cvc
├── docs/
│   ├── 01-LANGUAGE-SPEC.md          # Formal syntax & semantic mapping rules
│   ├── 02-EFFECT-INFERENCE.md       # Static call-graph effect inference rules
│   ├── 03-DIAGNOSTICS-SOURCEMAPS.md # Two-stage error translation & JSON source maps
│   └── 04-ARCHITECTURE.md           # Compiler pipeline & LSP proxy design
├── compiler/
│   ├── include/cvc/                 # Token, AST, Symbol/Effect, Emitter & SourceMap headers
│   └── src/                         # Reference C++20 implementation (Lexer, Parser, CLI)
├── ide/
│   ├── visual-studio/
│   │   ├── CV.Build.targets         # MSBuild incremental transpilation hook
│   │   ├── CV.tmLanguage.json       # TextMate syntax grammar (VS 2022/2026 & VS Code)
│   │   └── ProblemMatcher.json      # Error List regex integration
│   └── lsp/
│       └── DESIGN.md                # Thin LSP proxy architecture over Verse Language Server
└── examples/
    ├── 01_BasicsAndMutability/      # hello_world.cv & expected hello_world.verse
    ├── 02_TransactionalCombat/      # Inventory.cv & expected Inventory.verse
    ├── 03_StructuredConcurrency/    # CapturePoint.cv & expected CapturePoint.verse
    └── 04_SceneGraphComponent/      # FlightThruster.cv & expected FlightThruster.verse
```

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
