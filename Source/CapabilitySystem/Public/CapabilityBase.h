// Copyright ysion(LZY). All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CapabilityCommon.h"
#include "CapabilityDataComponent.h"
#include "CapabilityBase.generated.h"

class UCapabilityMetaHead;
class UCapabilityComponent;

DECLARE_DWORD_COUNTER_STAT(TEXT("Capability Count"), STAT_CapabilityCount, STATGROUP_Capability);

UENUM(BlueprintType)
enum class ECapabilityExecuteSide : uint8 {
    Always UMETA(DisplayName = "Always Run"),
    AuthorityOnly UMETA(DisplayName = "Authority Only"),
    AllClients UMETA(DisplayName = "All Clients"),
    LocalControlledOnly UMETA(DisplayName = "Local Controlled Only"),
    AuthorityAndLocalControlled UMETA(DisplayName = "Authority And Local Controlled"),
    OwnerLocalControlledOnly UMETA(DisplayName = "Owner Local Controlled Only"),
};

UCLASS(Abstract, NotBlueprintable)
class CAPABILITYSYSTEM_API UCapabilityBase : public UObject {
    GENERATED_BODY()

    friend class UCapabilityComponent;

    bool bHasBegunPlay = false;
    bool bHasEndedPlay = false;
    bool bHasPreEndedPlay = false;

protected:

    UPROPERTY(Replicated)
    TWeakObjectPtr<UCapabilityComponent> TargetCapabilityComponent = nullptr;

    UPROPERTY(Replicated)
    TObjectPtr<UCapabilityMetaHead> TargetMetaHead = nullptr;

    UPROPERTY(Replicated)
    int16 IndexInSet = 0;
    
    UPROPERTY(BlueprintReadOnly)
    bool bCanEverTick = true;

    UPROPERTY(BlueprintReadOnly)
    float tickInterval = 0.0f;

    bool bIsTickEnabled = false;
    
    float tickTimeSum = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    ECapabilityExecuteSide executeSide = ECapabilityExecuteSide::Always;

    bool bIsCapabilityActive = false;

public:
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TArray<FName> Tags;
    
    UCapabilityBase(const FObjectInitializer& ObjectInitializer);

    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable)
    AActor* GetOwner() const;

    template <typename T>
    T* GetOwner() const;

    UFUNCTION(BlueprintCallable)
    UCapabilityComponent* GetCapabilityComponent() const;

    UFUNCTION(BlueprintCallable)
    FString GetString();

    /**
      * Configure the network execution mode of this ability.  
      * Should be called during construction.
      * 
      * @param Mode The execution mode that determines how this ability runs in networked environments.
      * 
      * Usage Guide:
      * 
      * [Always] - Executes unconditionally (including on dedicated servers)  
      *   Use cases: Logic or state updates that must stay consistent across all instances.  
      *   Examples: State machine updates, cooldown calculations, buff timers, debug logs  
      *   Note: Runs on dedicated servers as well, so not suitable for purely visual effects.
      * 
      * [AuthorityOnly] - Executes only on the server (authority).  
      *   Use cases: Core gameplay logic, state changes, or any operations that require anti-cheat protection.  
      *   Examples: Damage calculation, item spawning, game rule validation, score tracking  
      *   Note: Results will be automatically replicated to clients.
      * 
      * [LocalControlledOnly] - Executes only on locally controlled characters.  
      *   Use cases: Player input handling, local prediction, or effects that only impact the local player.  
      *   Examples: Movement input, camera control, local UI interaction, skill previsualization  
      *   Note: For AI-controlled characters, runs on the server; for player-controlled characters, runs on their owning client.
      *
      * [AuthorityAndLocalControlled] - Executes on both the authority and locally controlled instances.  
      *   Use cases: Operations like line tracing where local prediction is needed but the authoritative result comes from the server.
      * 
      * [AllClients] - Executes on all clients (excluding dedicated servers).  
      *   Use cases: Purely cosmetic effects, visual/audio feedback, or other non-gameplay logic.  
      *   Examples: Particle effects, sound playback, animations, environmental effects, UI updates  
      *   Note: Does not run on dedicated servers, suitable for purely client-side visuals.
      * 
      * Quick Selection Guidelines:  
      * - Does it affect game state? → Choose AuthorityOnly  
      * - Is it handling player input? → Choose LocalControlledOnly  
      * - Is it purely visual/audio? → Choose AllClients  
      * - Needs synchronized timers or states across all ends? → Choose Always  
      * - Not sure? → Default to AuthorityOnly for safety
      */
    UFUNCTION(BlueprintCallable)
    void SetExecuteSide(ECapabilityExecuteSide Mode) { executeSide = Mode; }
    
    UFUNCTION(BlueprintCallable)
    ECapabilityExecuteSide GetExecuteSide() { return executeSide; }
    
    // Call At Construct
    UFUNCTION(BlueprintCallable)
    void SetCanEverTick(bool bCanTick) { bCanEverTick = bCanTick; }

    UFUNCTION(BlueprintCallable)
    bool GetCanEverTick() const { return bCanEverTick; }

    // Set bCanEverTick at Runtime
    UFUNCTION(BlueprintCallable)
    void SetEnable(bool bEnable);

    UFUNCTION(BlueprintCallable)
    void SetTickInterval(float InInterval) { tickInterval = InInterval; }
    
    UFUNCTION(BlueprintCallable)
    float GetTickInterval() const { return tickInterval; }
    
    bool ShouldRunOnThisSide() const;
    
    UFUNCTION(BlueprintCallable)
    void Activate();

    UFUNCTION(BlueprintCallable)
    void Deactivate();

    UFUNCTION(BlueprintCallable)
    bool IsCapabilityActive() const { return bIsCapabilityActive; }

    UFUNCTION(BlueprintCallable)
    bool IsSideLocalControlled() const;

    UFUNCTION(BlueprintCallable)
    bool IsSideAuthorityOnly() const;

    UFUNCTION(BlueprintCallable)
    bool IsSideAllClients() const;

    UFUNCTION(BlueprintCallable)
    void BlockCapability(const FName& Tag, UObject* From);

    UFUNCTION(BlueprintCallable)
    void UnBlockCapability(const FName& Tag, UObject* From);
    
    // Life Cycle
    UFUNCTION(BlueprintNativeEvent)
    void StartLife();
    
    UFUNCTION(BlueprintNativeEvent)
    void Setup();
    
    UFUNCTION(BlueprintNativeEvent)
    bool ShouldActive();

    UFUNCTION(BlueprintNativeEvent)
    bool ShouldDeactivate();

    UFUNCTION(BlueprintNativeEvent)
    void OnActivated();

    UFUNCTION(BlueprintNativeEvent)
    void OnDeactivated();
    
    UFUNCTION(BlueprintNativeEvent)
    void Tick(float DeltaTime);

    UFUNCTION(BlueprintNativeEvent)
    void EndCapability();

    UFUNCTION(BlueprintNativeEvent)
    void EndLife();

    void NativeBeginPlay();
    void NativeEndPlay();
    void NativePreEndPlay();
    void NativeTick(float DeltaTime);

    virtual void BeginPlay() {}
    virtual void EndPlay() {}
    virtual void UpdateCapabilityState() {}

    virtual void PreDestroyFromReplication() override;
    
    virtual bool IsSupportedForNetworking() const override { return true; }

    virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override {
        if (UObject* Outer = GetOuter()) return Outer->GetFunctionCallspace(Function, Stack);
        return FunctionCallspace::Local;
    }

    virtual bool CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack) override {
        AActor* Actor = GetOwner();
        if (!Actor) return false;
        if (UNetDriver* NetDriver = Actor->GetNetDriver()) {
            NetDriver->ProcessRemoteFunction(Actor, Function, Parms, OutParms, Stack, this);
            return true;
        }
        return false;
    }
};
