# Capability System
<p>
  <u><b>&gt; English &lt;</b></u>
  <span style="display:inline-block; width: 2rem; text-align: center;">|</span>
  <u><a href="README_CN.md">中文</a></u>
</p>

---

## ⚠️ Pre-Usage Notes

Before fully adopting the **Capability System**, it’s important to understand its runtime characteristics.  
If using an *all-in CapabilitySystem* configuration is not a reasonable design choice for your project, note that this framework operates as an **active-driven behaviour system**.  
When a large number of actors each hold multiple *tick-enabled capabilities*, the cumulative CPU cost can become significant.  
For optimal performance and scalability, it is **strongly recommended** to integrate the Capability System together with an **event-driven behaviour system**, so that time-critical or continuous updates are handled efficiently while less-frequent logic is triggered by events.

In terms of **network synchronization**, this system intentionally employs some *hack-style (trick-based)* synchronization methods.  
Capability execution is primarily driven by **Conditions**, which also represent and include the **BlockTags** used to block interactions between capabilities.  
Because of the inherent latency of network transmission, the activation and deactivation of capabilities across the network may be **delayed or deferred** relative to local states.  

Therefore, it is encouraged to use capabilities mainly as **local active-driven logic units**, and combine them with a **network-synchronized, event-push-based behaviour layer** to achieve a balanced and responsive experience.

---

## Overview
Capability System is a lightweight gameplay framework that lets you assemble actor behaviour from small, network-aware modules called *capabilities*. It is engine-version agnostic: the same core works whether your project leans on C++, Blueprints, UnrealSharp, or AngelScript. Each capability can decide where it executes (server, owning client, every client, etc.), access replicated data components, and optionally manage Enhanced Input bindings. Designers can work entirely in AngelScript or UnrealSharp while sharing the same lifecycle and networking rules as C++ counterparts.

The architecture takes cues from the capability framework Hazelight discussed at GDC: capabilities act as tiny, composable slices of logic that can be added, reordered, or removed without touching surrounding systems. This reference implementation keeps the core ideas approachable: capabilities own their own state, opt into networking, and cooperate through tags instead of hard-coded dependencies, making it easy to iterate rapidly on gameplay while keeping codebases decoupled.

## Core Types
- **`UCapabilityComponent`**: the manager that lives on an actor. It is responsible for loading capability sets, creating capability instances, ticking them each frame, and replicating state to clients. Every pawn, character, or actor that should run capabilities needs exactly one of these components.
- **`UCapabilitySet` & `UCapabilitySetCollection`**: data assets created in the editor. A set lists the capability classes (and optional data component classes) that should spawn together. Collections group several sets so you can apply a full loadout in one call. Reordering entries inside a set instantly changes execution priority without touching code.
- **`UCapability`**: the base class you extend for gameplay behaviour. It provides the lifecycle hooks (start, activation, tick, end) and handles networking based on `ExecuteSide`.
- **`UCapabilityInput`**: a convenience subclass of `UCapability` that adds Enhanced Input helpers (`OnBindActions`, `OnBindInputMappingContext`, action binding utilities). Use this whenever the capability reacts to player input.
- **`UCapabilityDataComponent`**: a replicated actor component spawned alongside a capability set to store shared runtime data (cooldowns, references, UI widgets, etc.). Access them through the owning actor, for example `GetOwner()->FindComponentByClass(Type)` in C++ or `Owner.GetComponentByClass(Type)` in AngelScript.
- **`UInputAssetManager` & `UInputAssetManagerBind`**: a subsystem plus blueprint-callable helper class. Register `UInputAction` and `UInputMappingContext` assets in Project Settings > Input Asset Manager, then call `UInputAssetManagerBind::Action` / `::IMC` in your capabilities to fetch them without manual loading.

### Ordered Execution
Capabilities run in the order they appear inside a `UCapabilitySet`. Because the order is asset-driven, you can adjust execution priority simply by rearranging entries in the set instead of hard-coding dependencies.

## Usage Workflow
1. **Place a manager**: Add `UCapabilityComponent` to the actor that should host capabilities. Pick `ComponentMode` (`Authority` for replicated gameplay, `Local` for standalone/editor tooling) and optionally assign preset sets or collections.

    <img width="283" height="158" alt="image" src="https://github.com/user-attachments/assets/ea8571d4-175b-4c71-bff2-9f85102f94c7" />

2. **Author assets**: Create `UCapabilitySet` assets that list the capability classes to spawn together. If the group needs shared state, include the matching `UCapabilityDataComponent` classes in the same set.

    <img width="252" height="169" alt="image" src="https://github.com/user-attachments/assets/a23dc96d-570a-4a9d-bbb8-7b11eb1234ef" />
    <img width="418" height="169" alt="image" src="https://github.com/user-attachments/assets/f5a568b0-5308-4f49-8676-87e985ca42cb" />

3. **Implement capabilities**: Derive from `UCapability` or `UCapabilityInput` in C++ or AngelScript or UnrealSharp, override the lifecycle hooks you need, and set defaults such as `ExecuteSide`, `CanEverTick`, and `TickInterval`.

    <img width="515" height="89" alt="image" src="https://github.com/user-attachments/assets/9c5ce2b6-4a2c-42ae-8552-eda55c62bfed" />

4. **Combine Capabilities**: Combine Capabilities into a `CapabilitySet` to form a complex functional system, and adjust the order. The Capabilities will be Setup, checked for status, and Ticked in a top-to-bottom order. During destruction, the order will be from bottom to top.

    <img width="549" height="169" alt="image" src="https://github.com/user-attachments/assets/b233d52a-8f35-4d90-9d21-da5a29fc0462" />
    <img width="386" height="169" alt="image" src="https://github.com/user-attachments/assets/86465f42-8db7-4015-9669-66e89fbb3a1d" />

5. **Register loadouts**: At runtime call `AddCapabilitySet` or `AddCapabilitySetCollection` to apply gameplay loadouts. In `Authority` mode these calls must originate on the server; in `Local` mode any instance can add/remove sets. Or place a `CapabilitySet` or `CapabilityCollection` into a `CapabilityComponent` in the editor as a preset capability.

    <img width="544" height="173" alt="image" src="https://github.com/user-attachments/assets/f94a52cf-29d2-4a29-a033-363a0bb2e648" />

6. **Drive behaviour**: Use `BlockCapability` tags, data components, and capability activations to coordinate systems instead of hard references between gameplay classes.

## Capability – Execution Model
- Callback routing
  - Always on all sides: `StartLife()`, `EndLife()`
  - Only when `ShouldRunOnThisSide()` is true: `Setup()`, activation checks (`ShouldActive()`/`ShouldDeactivate()`), `Tick()`, `EndCapability()`
  - Transition callbacks: `OnActivated()` and `OnDeactivated()` fire only on the side that performed the state change
- Initialization sequence (per allowed side)
  1) `Setup()`
  2) Initial state check uses `ShouldDeactivate()`:
     - If `ShouldDeactivate()` returns false → the capability activates immediately (`Activate()` → `OnActivated()`).
     - If it returns true → the capability stays inactive. The framework calls `Deactivate()` for symmetry, but because it wasn’t active yet it early-outs and does not call `OnDeactivated()`.
- Runtime evaluation (per tick; only if `CanEverTick` is true and the side check passes)
  - While ACTIVE: call `ShouldDeactivate()`; if it returns true, transition to inactive (`Deactivate()` → `OnDeactivated()`).
  - While INACTIVE: call `ShouldActive()`; if it returns true, transition to active (`Activate()` → `OnActivated()`).
  - After the state update, `Tick(DeltaTime)` runs only if the capability is still active.
  - If `TickInterval > 0`, the state checks and `Tick` are evaluated on that cadence rather than every frame.
- Tick scheduling by the component
  - A capability is included in the component’s tick list only when `CanEverTick` is true and `ShouldRunOnThisSide()` is true.
  - Use `SetEnable(false)` to remove a capability from the tick list at runtime, and `SetTickInterval(seconds)` to reduce evaluation frequency.
- Blocking
  - Per frame, for each capability in the component’s tick list, the component checks `BlockInfo` against the capability’s `Tags`.
    - If blocked: that frame’s update is skipped (no activation/deactivation evaluation, no Tick). If the capability is currently active it is deactivated (`Deactivate()` → `OnDeactivated()` on that side).
    - If not blocked: the capability performs its normal frame update: evaluate activation/deactivation, and if it remains active, call `Tick` (respecting `TickInterval`).
  - Because capabilities are processed sequentially, a call to `BlockCapability`/`UnBlockCapability` made by an earlier capability in the same frame takes effect immediately for later capabilities in that tick. Already-processed capabilities in that frame are unaffected until the next tick.
- Teardown
  - On removal/shutdown, an active capability is first deactivated (triggering `OnDeactivated()` on the side that was running it).
  - Then `EndCapability()` runs on allowed sides, and `EndLife()` always runs on every side to guarantee final cleanup.

## Execution Sides
Set the execution mode with `default ExecuteSide` inside your capability.  

| Mode | Runs On | Typical Use |
| ---- | ------- | ----------- |
| `Always` | Every instance, including dedicated servers | Timers, replicated state machines |
| `AuthorityOnly` | Server only | Inventory updates, authoritative gameplay rules |
| `LocalControlledOnly` | Locally controlled pawn/controller | Input sampling, local prediction |
| `AuthorityAndLocalControlled` | Server + owning client | Client prediction with server reconciliation |
| `OwnerLocalControlledOnly` | Only the locally controlled owner (supports nested ownership) | Gadgets attached to player-owned actors |
| `AllClients` | Every non-dedicated instance (clients, standalone, listen servers) | Visual/audio feedback, pure UI |

`UCapabilityComponent` can run in two modes: `Authority` (default) where the server owns capability creation and replicates the sub-objects to clients, and `Local` where the owning instance builds everything locally (useful for standalone tools, previews, or controller-side widgets). Pick the mode per component instance in the details panel.

<details>
<summary>

### Capability – Lifecycle (AngelScript)

</summary>
AngelScript exposes the same hooks as the C++ classes, but the day-to-day pattern is slightly different: you subclass `UCapability` and rely on the lifecycle callbacks below instead of overriding an actor-level `BeginPlay()`. The template lists every overridable function in the order they may run.

```c++
class UMyCapability : UCapability
{
    default ExecuteSide = ECapabilityExecuteSide::Always;
    default CanEverTick = true;
    default TickInterval = 0.f; // 0 = evaluate every frame; >0 = evaluate on that cadence

    // Runs once on every machine (server and all clients), independent of ExecuteSide.
    UFUNCTION(BlueprintOverride)
    void StartLife() {}

    // Runs only on sides where ShouldRunOnThisSide() == true. Cache components/data here.
    UFUNCTION(BlueprintOverride)
    void Setup() {}

    // While INACTIVE (runtime): return true to request activation.
    // Note: initial activation decision is made right after Setup based on ShouldDeactivate().
    UFUNCTION(BlueprintOverride)
    bool ShouldActive() { return true; }

    // While ACTIVE (runtime): return true to request deactivation.
    // Note: immediately after Setup, if ShouldDeactivate() returns false the capability activates at once; if true it stays inactive.
    UFUNCTION(BlueprintOverride)
    bool ShouldDeactivate() { return false; }

    // Fired on the side that performed Activate().
    UFUNCTION(BlueprintOverride)
    void OnActivated() {}

    // Fired on the side that performed Deactivate(). Not called during initial Setup unless the capability was already active.
    UFUNCTION(BlueprintOverride)
    void OnDeactivated() {}

    // Called only if the capability is active and allowed to run on this side; respects TickInterval.
    UFUNCTION(BlueprintOverride)
    void Tick(float DeltaTime) {}

    // Teardown on allowed sides (e.g., set removal). Runs before EndLife().
    UFUNCTION(BlueprintOverride)
    void EndCapability() {}

    // Final cleanup on all sides, even those that never ran Setup/ Tick due to ExecuteSide.
    UFUNCTION(BlueprintOverride)
    void EndLife() {}
}
```
</details>

<details>
<summary>

### Capability – Lifecycle (UnrealSharp C#)

</summary>

```csharp
// C# (UnrealSharp) example for a base capability
// Configure where and how often this capability runs on each machine.
[UClass]
public class UMyCapability : UCapability
{
    public UMyCapability()
    {
        // Where to run (see ExecuteSide table above). Example: only on locally controlled pawns/PCs.
        executeSide = ECapabilityExecuteSide.LocalControlledOnly;
        // Whether this capability can be evaluated every frame (or on an interval) on the allowed side.
        CanEverTick = true;
        // Per-frame (0) or interval-driven (>0) evaluation of state machine + Tick.
        tickInterval = 0.0f;
    }

    // Always runs on every machine (server + all clients), regardless of ExecuteSide.
    public override void StartLife() { }

    // Runs only where ShouldRunOnThisSide() == true. Cache components/data for this side here.
    public override void Setup() { }

    // While INACTIVE at runtime: return true to request activation.
    // Note: immediately after Setup, the initial state uses ShouldDeactivate() instead.
    public override bool ShouldActive() => true;

    // While ACTIVE at runtime: return true to request deactivation.
    // Note: after Setup, if ShouldDeactivate() returns false the capability activates immediately; if true, it stays inactive.
    public override bool ShouldDeactivate() => false;

    // Fired on the side that performed Activate(). Use to kick off side-local effects.
    public override void OnActivated() { }

    // Fired on the side that performed Deactivate(). Not called during initial bootstrap if the capability never became active.
    public override void OnDeactivated() { }

    // Called only if the capability is ACTIVE and allowed to run on this side; respects TickInterval.
    public override void Tick(float deltaTime) { }

    // Teardown on allowed sides (e.g., capability set removal). Runs before EndLife().
    public override void EndCapability() { }

    // Final cleanup on all sides, even on sides that never ran Setup/Tick due to ExecuteSide.
    public override void EndLife() { }
}
```
</details>


## CapabilityInput – Controller-aware Input Capabilities
`UCapabilityInput` builds on `UCapability` to handle input-specific plumbing:
- Receives `OnGetControllerAndInputComponent(APlayerController, UEnhancedInputComponent)` when a local controller attaches
- Exposes `OnControllerAttach` / `OnControllerDeattach` to react to possession changes
- Provides `OnBindActions` and `OnBindInputMappingContext` to declare Enhanced Input bindings
- Offers `UseInput()` / `StopUseInput()` to toggle mapping contexts at runtime without destroying the capability
- Requires your pawns/controllers to forward control changes to the capability component so input caps can bind correctly:
  - Call `UCapabilityComponent::OnControllerChanged(APlayerController*, UEnhancedInputComponent*)` when setting up input (`SetupPlayerInputComponent` / controller `SetupInputComponent`).
  - Call `UCapabilityComponent::OnControllerRemoved()` on unpossess/detach.
  - See `ACapabilityCharacter` and `ACapabilityController` for reference implementations.

<details>
<summary>

### CapabilityInput Example (AngelScript)

</summary>
The example below wires Enhanced Input to control forward/back/strafe/ascend/descend using the character and its camera.

```c++
class UFlyingInputCapability : UCapabilityInput
{
    default ExecuteSide = ECapabilityExecuteSide::LocalControlledOnly;
    default CanEverTick = false; // We use Input event, so we can disable Capability part.

    ACharacter CachedCharacter;

    // Execute on Server and all Clients
    UFUNCTION(BlueprintOverride)
    void StartLife() 
    {
        CachedCharacter = Cast<ACharacter>(Owner);
    }

    // Execute on LocalControlledOnly (Because ExecuteSide == ECapabilityExecuteSide::LocalControlledOnly)
    UFUNCTION(BlueprintOverride)
    void Setup()
    {
        /* Due to initialization latency issues in Unreal Networking, we avoid setting the movement mode in StartLife. 
        This is because when the server executes StartLife, 
        the client might not have even received the Capability component data yet.
        Therefore, the most stable approach is for the client to call an RPC to the server during its Setup. */
        StartFlyingMode(); // RPC
    }

    UFUNCTION(Server)
    void StartFlyingMode() 
    {
        UCharacterMovementComponent::Get(Owner).SetMovementMode(EMovementMode::MOVE_Flying);
    }

    UFUNCTION(BlueprintOverride)
    void OnBindActions() 
    {
        BindAction(UInputAssetManagerBind::Action(n"IA_RobotMove"), ETriggerEvent::Triggered, this, n"Move");
        BindAction(UInputAssetManagerBind::Action(n"IA_RobotUpDown"), ETriggerEvent::Triggered, this, n"UpDown");
        BindAction(UInputAssetManagerBind::Action(n"IA_RobotLook"), ETriggerEvent::Triggered, this, n"Look");
    }

    UFUNCTION(BlueprintOverride)
    void OnBindInputMappingContext()
    {
        BindInputMappingContext(UInputAssetManagerBind::IMC(n"IMC_Robot_Fly"));
    }

    UFUNCTION()
    void Move(FInputActionValue Value) 
    {
        FVector2D v = Value.GetAxis2D();
        FVector forward = CachedCharacter.GetActorForwardVector();
        FVector right = CachedCharacter.GetActorRightVector();
        CachedCharacter.AddMovementInput(right * v.X);
        CachedCharacter.AddMovementInput(forward * v.Y);
    }

    UFUNCTION()
    void Look(FInputActionValue Value) 
    {
        FVector2D v = Value.GetAxis2D();
        CachedCharacter.AddControllerYawInput(v.X);
        CachedCharacter.AddControllerPitchInput(v.Y);
    }

    UFUNCTION()
    void UpDown(FInputActionValue Value) 
    {
        float v = Value.GetAxis1D();
        FVector UpVec = CachedCharacter.GetActorUpVector();
        CachedCharacter.AddMovementInput(UpVec * v);
    }
}
```
</details>

`UInputAssetManagerBind` is designed as an ergonomic helper: once you register your `UInputAction` and `UInputMappingContext` assets in **Project Settings -> Input Asset Manager**, you can reference them with `UInputAssetManagerBind::Action(Name)` or `::IMC(Name)` directly from code without manual loading.

<details>
<summary>

### CapabilityInput Example (UnrealSharp C#)

</summary>

```csharp
// C# (UnrealSharp) input capability: binds actions and an IMC
[UClass]
public class UFlyingInputCapability : UCapabilityInput
{
   private ACharacter? CachedCharacter { get; set; }
    
    public UFlyingInputCapability()
    {
        executeSide = ECapabilityExecuteSide.LocalControlledOnly;
        CanEverTick = false; // We use Input event, so we can disable Capability part.
        tickInterval = 0.0f;
    }
    
    // Execute on Server and all Clients
    public override void StartLife()
    {
        CachedCharacter = Owner as ACharacter;
        CachedCharacter?.CharacterMovement.SetMovementMode(EMovementMode.MOVE_Flying);
    }

    // Execute on LocalControlledOnly (Because ExecuteSide == ECapabilityExecuteSide.LocalControlledOnly)
    public override void Setup()
    {
        /* Due to initialization latency issues in Unreal Networking, we avoid setting the movement mode in StartLife. 
        This is because when the server executes StartLife, 
        the client might not have even received the Capability component data yet.
        Therefore, the most stable approach is for the client to call an RPC to the server during its Setup. */
        StartFlyingMode(); // RPC
    }

    [UFunction(FunctionFlags.RunOnServer | FunctionFlags.Reliable)]
    private void StartFlyingMode()
    {
        CachedCharacter?.CharacterMovement.SetMovementMode(EMovementMode.MOVE_Flying);
    }
    
    protected override void OnBindActions()
    {
        BindAction(UInputAssetManagerBind.Action("IA_Look"), ETriggerEvent.Triggered, this, "Look");
        BindAction(UInputAssetManagerBind.Action("IA_Move"), ETriggerEvent.Triggered, this, "Move");
        BindAction(UInputAssetManagerBind.Action("IA_UpDown"), ETriggerEvent.Triggered, this, "UpDown");
    }

    protected override void OnBindInputMappingContext()
    {
        BindInputMappingContext(UInputAssetManagerBind.IMC("IMC_Moving"));
    }

    [UFunction]
    public void Look(FInputActionValue value)
    {
        if (CachedCharacter is null) return;
        var offset = value.GetAxis2D();
        CachedCharacter.AddControllerYawInput((float)offset.X);
        CachedCharacter.AddControllerPitchInput(-(float)offset.Y);
    }

    [UFunction]
    public void UpDown(FInputActionValue value)
    {
        if (CachedCharacter is null) return;
        var offset = value.GetAxis1D();
        var up = CachedCharacter.ActorUpVector;
        CachedCharacter.AddMovementInput(offset * up);
    }

    [UFunction]
    public void Move(FInputActionValue value)
    {
        if (CachedCharacter is null) return;
        var offset = value.GetAxis2D();
        var forward = CachedCharacter.ActorForwardVector;
        var right = CachedCharacter.ActorRightVector;
        CachedCharacter.AddMovementInput(forward * offset.Y);
        CachedCharacter.AddMovementInput(right * offset.X);
    }
}
```
</details>

## Managing Capability Sets
1. Create a `UCapabilitySet` asset and add your capability classes along with any required `UCapabilityDataComponent` classes. Order matters: the list determines the execution sequence.
2. Optionally group multiple sets inside a `UCapabilitySetCollection` to apply an entire loadout at once.
3. Add `UCapabilityComponent` to your actor and assign default sets/collections in the details panel, or call `AddCapabilitySet` / `RemoveCapabilitySet` at runtime.

Because execution order is asset-driven, you can reprioritize behaviour simply by reshuffling entries in the capability set. No code changes are required to coordinate dependencies between abilities.

## Data Components & Shared State
- When a set lists `UCapabilityDataComponent` classes, the capability component automatically spawns them on the owning actor, wires them to the shared `UCapabilityMetaHead`, and destroys them again when the set is removed.
- Data components replicate (push-model) alongside their sibling capabilities, so any replicated properties you add are visible to clients as soon as the capability set syncs.
- Retrieve the shared component manually: in AngelScript call `Owner.GetComponentByClass(SomeDataComponentClass)`; in C++ call `GetOwner()->FindComponentByClass(SomeDataComponentClass)` or cache the pointer during `Setup`.
- Use data components for cross-capability state such as cooldowns, target references, or UI widgets that several capabilities need to touch.


## Networking Notes
- In `Authority` mode, add and remove capability sets from the server. The component registers each capability, its meta head, and every data component as replicated sub-objects so owners receive identical lifecycles.
- Clients defer capability bootstrap until required actor components exist, preventing race conditions when data components are still initializing.
- `AllClients` executes on every non-dedicated instance (including listen servers), while `OwnerLocalControlledOnly` walks up the ownership chain so gadget actors attached to a player still respect local control.
- `Local` mode keeps all work on the current instance, which is ideal for editor utilities, standalone previews, or controller-specific UI logic.

## Debugging & Tips
- `GetCapabilityComponentStates` returns a snapshot of all capabilities hosted by a component (name, active state, execute side).
- `GetString()` prints a tree of capability sets and instances for quick in-game inspection.
- Use `stat Capability` to monitor total and ticking capability counts.
- Prefer `SetCanEverTick(false)` for event-driven capabilities; re-enable ticking only when necessary.
- Block mutually exclusive abilities with `BlockCapability(Tag, Source)` / `UnBlockCapability` instead of spreading tag checks across code.
- Enable the `CapabilitySystemLog` category for runtime diagnostics; the component already emits warnings when assets fail to load or when replication preconditions are not met.

Capability System emphasises predictable lifecycle hooks, explicit network routing, and data-driven ordering, so you can extend gameplay behaviour safely and iteratively using script languages such as AngelScript or UnrealSharp.
