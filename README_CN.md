# 能力系统（Capability System）
<p>
  <u><a href="README.md">English</a></u>
  <span style="display:inline-block; width: 2rem; text-align: center;">|</span>
  <u><b>&gt; 中文 &lt;</b></u>
</p>

## 使用前请注意

---

在全面使用 **Capability System** 之前，请先了解其运行特性。  
若「全部系统逻辑均依赖 CapabilitySystem」并非合理的设计选择，需要注意此框架本质上是一个**主动驱动式的行为系统**。  
当大量 Actor 含有多个启用 Tick 的 Capability 时，CPU 开销会迅速累积，影响性能。  
为获得最佳体验，**建议将 Capability System 与事件推送式（Event-Driven）的行为系统结合使用**，让持续性逻辑与触发性逻辑分工明确，从而实现高效运行与良好的扩展性。

在**网络同步**层面，该系统包含部分技巧性（Hack-Style）的实现。  
驱动 Capability 运作的核心机制是 **Condition**，它同时代表并包含用于在不同 Capability 之间互斥的 **BlockTag**。  
由于网络延迟的存在，跨网络同步的 Capability 状态变化（如激活或停用）会出现一定的**延后**。  

因此，推荐的使用方式是：  
将 Capability 作为**本地主动驱动系统的核心逻辑单元**，并结合**网络事件推送式的行为系统**来实现跨端协同，从而获得平衡且流畅的体验。

---

## 概述（Overview）
Capability System 是一套轻量的玩法框架，让你把 Actor 的行为拆分成小而独立、可联网的模块（capabilities）再组合使用。它与引擎版本无关：无论你主要使用 C++、Blueprints、UnrealSharp 还是 AngelScript，都能复用同一套核心。每个能力都可以决定自己的执行侧（例如：服务器、拥有者客户端、所有客户端等）、访问可复制（replicated）的数据组件，并且可以选择管理 Enhanced Input 的绑定。设计师可以完全用 AngelScript 或 UnrealSharp 进行工作，同时遵循和 C++ 相同的生命周期与网络规则。

该架构参考了 Hazelight 在 GDC 分享的能力框架：能力是小而可组合的逻辑切片，可以在不影响周边系统的情况下添加、重排或移除。本参考实现保持思路简洁易上手：能力自己管理状态，可选择性参与网络，通过“标签（tags）”协作而不是硬编码依赖，从而在保持低耦合的同时快速迭代玩法。

## 核心类型（Core Types）
- `UCapabilityComponent`：挂在 Actor 上的管理器。负责加载能力集、创建能力实例、逐帧 Tick，并把状态复制到客户端。任何需要运行能力的 Pawn/Character/Actor 都应恰好挂一个该组件。
- `UCapabilitySet` 与 `UCapabilitySetCollection`：在编辑器中创建的数据资产。Set 列出需要一起生成的能力类（以及可选的数据组件类）。Collection 可以把多个 Set 打包，这样只需一次调用就能应用完整装配。在 Set 中重排条目即可改变执行优先级，无需改代码。
- `UCapability`：用于扩展玩法行为的基类。提供生命周期钩子（开始、激活、Tick、结束），并基于 `ExecuteSide` 处理网络。
- `UCapabilityInput`：`UCapability` 的便捷子类，提供 Enhanced Input 的辅助（`OnBindActions`、`OnBindInputMappingContext`、动作绑定工具）。当能力需要响应玩家输入时使用。
- `UCapabilityDataComponent`：与能力集一起生成的可复制 Actor 组件，用于存放共享的运行时数据（冷却、引用、UI 组件等）。可在拥有者上访问，例如 C++ 中 `GetOwner()->FindComponentByClass(Type)`，或 AngelScript 中 `Owner.GetComponentByClass(Type)`。
- `UInputAssetManager` 与 `UInputAssetManagerBind`：子系统 + 蓝图可调用的助手。在“Project Settings > Input Asset Manager”中注册 `UInputAction` 和 `UInputMappingContext` 资源后，就能在能力里用 `UInputAssetManagerBind::Action` / `::IMC` 直接获取，无需手动加载。

### 有序执行（Ordered Execution）
能力按照它们在 `UCapabilitySet` 中出现的顺序运行。因为顺序由资产驱动，你可以直接在资产里重排条目来调整执行优先级，而不是在代码中写死依赖关系。

## 使用流程（Usage Workflow）
1. 放置管理器：把 `UCapabilityComponent` 添加到需要承载能力的 Actor 上。选择 `ComponentMode`（复制玩法用 `Authority`，单机/编辑器工具用 `Local`），并可选设置默认的 Set 或 Collection。

    <img width="283" height="158" alt="image" src="https://github.com/user-attachments/assets/ea8571d4-175b-4c71-bff2-9f85102f94c7" />

2. 创建资产：新建 `UCapabilitySet`，列出要一起生成的能力类。如果这组需要共享状态，把对应的 `UCapabilityDataComponent` 也加到同一个 Set 中。

    <img width="252" height="169" alt="image" src="https://github.com/user-attachments/assets/a23dc96d-570a-4a9d-bbb8-7b11eb1234ef" />
    <img width="418" height="169" alt="image" src="https://github.com/user-attachments/assets/f5a568b0-5308-4f49-8676-87e985ca42cb" />

3. 实现能力：在 C++、AngelScript 或 UnrealSharp 中继承 `UCapability` / `UCapabilityInput`，覆写所需生命周期钩子，并设置 `ExecuteSide`、`CanEverTick`、`TickInterval` 等默认值。

    <img width="515" height="89" alt="image" src="https://github.com/user-attachments/assets/9c5ce2b6-4a2c-42ae-8552-eda55c62bfed" />

4. 组合能力：把多个能力组合到一个 `CapabilitySet` 里，构成一个较完整的功能系统，并按需调整顺序。能力会按自上而下的顺序进行 Setup、状态检查和 Tick；销毁时则按自下而上顺序处理。

    <img width="549" height="169" alt="image" src="https://github.com/user-attachments/assets/b233d52a-8f35-4d90-9d21-da5a29fc0462" />
    <img width="386" height="169" alt="image" src="https://github.com/user-attachments/assets/86465f42-8db7-4015-9669-66e89fbb3a1d" />

5. 注册装配：运行时调用 `AddCapabilitySet` 或 `AddCapabilitySetCollection` 应用玩法装配。`Authority` 模式下这些调用必须从服务器发起；`Local` 模式下本地实例即可增删 Set。也可以在编辑器中把 `CapabilitySet` 或 `CapabilityCollection` 直接配置到 `CapabilityComponent` 作为预设。

    <img width="544" height="173" alt="image" src="https://github.com/user-attachments/assets/f94a52cf-29d2-4a29-a033-363a0bb2e648" />

6. 驱动行为：用 `BlockCapability` 标签、数据组件和能力的激活/失活来协作系统，而不是在各个玩法类之间写硬引用。

## Capability 执行模型（Capability Execution Model）
- 回调路由
  - 所有侧都执行：`StartLife()`、`EndLife()`
  - 仅在 `ShouldRunOnThisSide()` 为真时执行：`Setup()`、激活检查（`ShouldActive()` / `ShouldDeactivate()`）、`Tick()`、`EndCapability()`
  - 状态切换回调：只有在触发状态改变的一侧才会调用 `OnActivated()` 和 `OnDeactivated()`
- 初始化顺序（按允许的执行侧）
  1) `Setup()`
  2) 初始状态检查使用 `ShouldDeactivate()`：
     - 如果 `ShouldDeactivate()` 返回 false → 能力会立刻激活（`Activate()` → `OnActivated()`）。
     - 如果返回 true → 能力保持未激活。框架会调用 `Deactivate()` 以保持对称，但由于此时其实并未激活，所以不会调用 `OnDeactivated()`。
- 运行期评估（按 Tick；仅当 `CanEverTick` 为真且侧向检查通过时）
  - 处于激活态（ACTIVE）：调用 `ShouldDeactivate()`；若返回 true，则转为未激活（`Deactivate()` → `OnDeactivated()`）。
  - 处于未激活态（INACTIVE）：调用 `ShouldActive()`；若返回 true，则转为激活（`Activate()` → `OnActivated()`）。
  - 状态更新完成后，只有在仍为激活态时才会调用 `Tick(DeltaTime)`。
  - 若 `TickInterval > 0`，则按该间隔评估状态检查与 `Tick`，而非每帧评估。
- 组件的 Tick 调度
  - 仅当 `CanEverTick` 为真且 `ShouldRunOnThisSide()` 为真时，能力才会加入组件的 Tick 列表。
  - 可用 `SetEnable(false)` 在运行时把能力移出 Tick 列表；用 `SetTickInterval(seconds)` 降低评估频率。
- 阻塞（Blocking）
  - 每帧，对于组件 Tick 列表中的每个能力，组件会根据该能力的 `Tags` 检查 `BlockInfo`：
    - 若被阻塞：本帧更新跳过（不进行激活/失活评估，也不 Tick）。若该能力当前是激活态，会在本侧被失活（`Deactivate()` → `OnDeactivated()`）。
    - 若未被阻塞：按正常流程进行本帧更新：评估激活/失活；若保持激活则进行 `Tick`（遵循 `TickInterval`）。
  - 因为能力按顺序处理，如果同一帧中较早的能力调用了 `BlockCapability`/`UnBlockCapability`，那么其效果会立即作用于之后的能力；已经处理过的能力要到下一帧才受影响。
- 拆卸（Teardown）
  - 在移除/关闭时，若能力处于激活态，会先进行失活（在运行该能力的一侧触发 `OnDeactivated()`）。
  - 然后在允许的侧调用 `EndCapability()`，最后在所有侧调用 `EndLife()` 以确保最终清理。

## 执行侧（Execution Sides）
在能力类中通过 `default ExecuteSide` 指定执行模式。

| 模式 | 运行于 | 典型用途 |
| ---- | ------ | -------- |
| `Always` | 所有实例（含专用服务器） | 计时器、复制状态机 |
| `AuthorityOnly` | 仅服务器 | 背包/库存更新、权威玩法规则 |
| `LocalControlledOnly` | 本地控制的 Pawn/Controller | 输入采样、本地预测 |
| `AuthorityAndLocalControlled` | 服务器 + 拥有者客户端 | 客户端预测 + 服务器回滚校正 |
| `OwnerLocalControlledOnly` | 仅本地控制的拥有者（支持嵌套所有权） | 附着在玩家拥有 Actor 上的装置/道具 |
| `AllClients` | 所有非专用实例（客户端、单机、监听服） | 视觉/音频反馈、纯 UI |

`UCapabilityComponent` 有两种模式：`Authority`（默认）由服务器创建能力并复制子对象到客户端；`Local` 在拥有者实例本地构建所有内容（适合单机工具、预览或控制器侧 UI）。可在 Details 面板为每个组件实例单独选择模式。

<details>
<summary>

### Capability — 生命周期（AngelScript）

</summary>
AngelScript 暴露与 C++ 相同的钩子，但日常用法稍有不同：继承 `UCapability`，使用下面这些生命周期回调，而不是覆写 Actor 级的 `BeginPlay()`。下面的模板按可能的执行顺序列出所有可覆写函数。

```c++
class UMyCapability : UCapability
{
    default ExecuteSide = ECapabilityExecuteSide::Always;
    default CanEverTick = true;
    default TickInterval = 0.f; // 0 = 每帧评估；>0 = 按该间隔评估

    // 无论 ExecuteSide 如何，都会在每台机器（服务器与所有客户端）运行一次。
    UFUNCTION(BlueprintOverride)
    void StartLife() {}

    // 仅在 ShouldRunOnThisSide() == true 的侧运行。在这里缓存组件/数据。
    UFUNCTION(BlueprintOverride)
    void Setup() {}

    // 运行期处于未激活态时：返回 true 以请求激活。
    // 注意：初始激活判断在 Setup 之后基于 ShouldDeactivate() 完成。
    UFUNCTION(BlueprintOverride)
    bool ShouldActive() { return true; }

    // 运行期处于激活态时：返回 true 以请求失活。
    // 注意：在 Setup 之后，若 ShouldDeactivate() 返回 false，将立刻激活；若返回 true，则保持未激活。
    UFUNCTION(BlueprintOverride)
    bool ShouldDeactivate() { return false; }

    // 在触发 Activate() 的那一侧被调用。
    UFUNCTION(BlueprintOverride)
    void OnActivated() {}

    // 在触发 Deactivate() 的那一侧被调用。若在初始引导阶段从未激活过，则不会调用。
    UFUNCTION(BlueprintOverride)
    void OnDeactivated() {}

    // 仅当能力为激活态且被允许在此侧运行时调用；遵循 TickInterval。
    UFUNCTION(BlueprintOverride)
    void Tick(float DeltaTime) {}

    // 在允许的侧进行拆卸（如从能力集中移除）。先于 EndLife() 运行。
    UFUNCTION(BlueprintOverride)
    void EndCapability() {}

    // 在所有侧进行最终清理，即便由于 ExecuteSide 原因从未在该侧运行过 Setup/Tick。
    UFUNCTION(BlueprintOverride)
    void EndLife() {}
}
```
</details>

<details>
<summary>

### Capability — 生命周期（UnrealSharp C#）

</summary>

```csharp
// C#（UnrealSharp）示例：基础能力
// 配置该能力在每台机器上运行的位置与频率。
[UClass]
public class UMyCapability : UCapability
{
    public UMyCapability()
    {
        // 运行侧（见上表 ExecuteSide）。示例：仅在本地控制的 Pawn/PC 上运行。
        executeSide = ECapabilityExecuteSide.LocalControlledOnly;
        // 是否允许在被允许的侧上每帧（或按间隔）进行评估。
        CanEverTick = true;
        // 每帧（0）或按间隔（>0）评估状态机 + Tick。
        tickInterval = 0.0f;
    }

    // 无论 ExecuteSide 如何，都会在每台机器（服务器 + 所有客户端）运行。
    public override void StartLife() { }

    // 仅在 ShouldRunOnThisSide() == true 的地方运行。在这里缓存该侧的组件/数据。
    public override void Setup() { }

    // 运行期处于未激活态：返回 true 以请求激活。
    // 注意：在 Setup 之后，初始状态使用 ShouldDeactivate() 进行判断。
    public override bool ShouldActive() => true;

    // 运行期处于激活态：返回 true 以请求失活。
    // 注意：在 Setup 之后，若 ShouldDeactivate() 返回 false 则立即激活；若为 true 则保持未激活。
    public override bool ShouldDeactivate() => false;

    // 在触发 Activate() 的那一侧调用。可用于启动仅限本侧的效果。
    public override void OnActivated() { }

    // 在触发 Deactivate() 的那一侧调用。若在引导阶段从未激活，则不会被调用。
    public override void OnDeactivated() { }

    // 仅当能力为激活态且允许在此侧运行时调用；遵循 TickInterval。
    public override void Tick(float deltaTime) { }

    // 在允许的侧进行拆卸（如从能力集中移除）。先于 EndLife() 运行。
    public override void EndCapability() { }

    // 在所有侧进行最终清理，即便由于 ExecuteSide 原因该侧从未运行过 Setup/Tick。
    public override void EndLife() { }
}
```
</details>

## CapabilityInput — 感知控制器的输入型能力
`UCapabilityInput` 在 `UCapability` 的基础上，处理与输入相关的流程：
- 当本地控制器挂接时，接收 `OnGetControllerAndInputComponent(APlayerController, UEnhancedInputComponent)`
- 暴露 `OnControllerAttach` / `OnControllerDeattach` 以响应控制权变更
- 提供 `OnBindActions` 与 `OnBindInputMappingContext` 用于声明 Enhanced Input 绑定
- 提供 `UseInput()` / `StopUseInput()`，可在运行时切换映射上下文而无需销毁能力
- 需要 Pawn/Controller 将控制器变更转发给能力组件，以便输入型能力能正确绑定：
  - 在设置输入时（`SetupPlayerInputComponent` / 控制器 `SetupInputComponent`）调用 `UCapabilityComponent::OnControllerChanged(APlayerController*, UEnhancedInputComponent*)`。
  - 在解除控制/分离时调用 `UCapabilityComponent::OnControllerRemoved()`。
  - 可参考 `ACapabilityCharacter` 与 `ACapabilityController` 的实现。

<details>
<summary>

### CapabilityInput 示例（AngelScript）

</summary>
下面的示例把 Enhanced Input 绑定到角色及其相机上，以控制前进/后退、平移与升降。

```c++
class UFlyingInputCapability : UCapabilityInput
{
    default ExecuteSide = ECapabilityExecuteSide::LocalControlledOnly;
    default CanEverTick = false; // 我们使用输入事件，因此可以关闭能力侧的 Tick。

    ACharacter CachedCharacter;

    // 在服务器与所有客户端上执行
    UFUNCTION(BlueprintOverride)
    void StartLife() 
    {
        CachedCharacter = Cast<ACharacter>(Owner);
    }

    // 在本地控制侧执行（因为 ExecuteSide == LocalControlledOnly）
    UFUNCTION(BlueprintOverride)
    void Setup()
    {
        /* 由于虚幻网络的初始化延迟问题，不建议在 StartLife 中设置移动模式。
        因为当服务器执行 StartLife 时，
        客户端可能还未收到 Capability 组件的数据。
        因此，最稳妥的方式是：由客户端在其 Setup 中调用 RPC 通知服务器。*/
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

`UInputAssetManagerBind` 旨在提升易用性：当你在 **Project Settings -> Input Asset Manager** 注册了 `UInputAction` 与 `UInputMappingContext` 资源后，就可以在代码中通过 `UInputAssetManagerBind::Action(Name)` 或 `::IMC(Name)` 直接引用，无需手动加载。

<details>
<summary>

### CapabilityInput 示例（UnrealSharp C#）

</summary>

```csharp
// C#（UnrealSharp）输入型能力：绑定动作与 IMC
[UClass]
public class UFlyingInputCapability : UCapabilityInput
{
   private ACharacter? CachedCharacter { get; set; }
    
    public UFlyingInputCapability()
    {
        executeSide = ECapabilityExecuteSide.LocalControlledOnly;
        CanEverTick = false; // 我们使用输入事件，因此可以关闭能力侧的 Tick。
        tickInterval = 0.0f;
    }
    
    // 在服务器与所有客户端上执行
    public override void StartLife()
    {
        CachedCharacter = Owner as ACharacter;
        CachedCharacter?.CharacterMovement.SetMovementMode(EMovementMode.MOVE_Flying);
    }

    // 在本地控制侧执行（因为 ExecuteSide == LocalControlledOnly）
    public override void Setup()
    {
        /* 由于虚幻网络的初始化延迟问题，不建议在 StartLife 中设置移动模式。
        因为当服务器执行 StartLife 时，
        客户端可能还未收到 Capability 组件的数据。
        因此，最稳妥的方式是：由客户端在其 Setup 中调用 RPC 通知服务器。*/
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

## 管理能力集（Managing Capability Sets）
1. 创建 `UCapabilitySet` 资产，并加入需要的能力类与 `UCapabilityDataComponent` 类。顺序很重要：列表顺序决定执行顺序。
2. 可选：把多个 Set 组合到 `UCapabilitySetCollection`，一次性应用完整装配。
3. 给 Actor 添加 `UCapabilityComponent`，并在 Details 面板设置默认的 Set/Collection，或在运行时调用 `AddCapabilitySet` / `RemoveCapabilitySet`。

由于执行顺序由资产驱动，只需在能力集中重排条目即可改变优先级。无需改代码，也无需在能力之间写硬依赖。

## 数据组件与共享状态（Data Components & Shared State）
- 当 Set 声明了 `UCapabilityDataComponent`，能力组件会在拥有者 Actor 上自动生成它们，把它们挂到共享的 `UCapabilityMetaHead` 下，并在移除该 Set 时销毁。
- 数据组件与其同级能力一同复制（推送模型），因此你添加的可复制属性会在能力集同步时立刻对客户端可见。
- 手动获取共享组件：在 AngelScript 使用 `Owner.GetComponentByClass(SomeDataComponentClass)`；在 C++ 中使用 `GetOwner()->FindComponentByClass(SomeDataComponentClass)`，或在 `Setup` 期间缓存指针。
- 用数据组件承载跨能力共享的状态，例如冷却、目标引用，或多个能力需要访问的 UI。

## 网络说明（Networking Notes）
- 在 `Authority` 模式下，应从服务器增删能力集。组件会把每个能力、其 meta head，以及所有数据组件注册为可复制的子对象，确保拥有者在各端具有一致的生命周期。
- 客户端会延迟能力的引导，直到所需 Actor 组件就绪，避免在数据组件仍在初始化时产生竞争条件。
- `AllClients` 会在所有非专用实例（包括监听服）执行；`OwnerLocalControlledOnly` 会沿所有权链向上查找，使附着在玩家上的装置 Actor 也能遵循本地控制。
- `Local` 模式把所有工作保留在当前实例，适合编辑器工具、单机预览或控制器侧 UI 逻辑。

## 调试与技巧（Debugging & Tips）
- `GetCapabilityComponentStates` 返回该组件承载的全部能力快照（名称、激活状态、执行侧）。
- `GetString()` 打印能力集与实例的树状结构，便于游戏内快速查看。
- 使用 `stat Capability` 监控能力总数与正在 Tick 的能力数量。
- 对事件驱动的能力优先关闭 `SetCanEverTick(false)`；仅在需要时再开启 Tick。
- 用 `BlockCapability(Tag, Source)` / `UnBlockCapability` 屏蔽互斥能力，避免在代码中到处写标签判断。
- 启用 `CapabilitySystemLog` 日志类别获取运行期诊断；当资产加载失败或复制前置条件不满足时，组件会输出告警。

Capability System 强调可预测的生命周期钩子、明确的网络路由、数据驱动的顺序。你可以用 AngelScript 或 UnrealSharp 等脚本语言，安全而迭代地扩展玩法行为。

