#pragma once

// Ports: src/gameplay/missions/framework/MissionRunner.ts
// Runs an FPaktiaMissionDef: follows `Next`, takes named `Branches`, enforces `Requires`
// flags, runs `Effects` through registered handlers, records checkpoints and publishes
// every transition on UPaktiaEventSubsystem (and on native delegates for C++ listeners).
//
// Event names and JSON payloads match MissionEvents in the TypeScript:
//   mission:state       {missionId, from?, to}            ("from" omitted when undefined)
//   mission:blocked     {missionId, state, missing:[...]}
//   mission:checkpoint  {missionId, checkpoint:{missionId, state, flags, history, systems:{...}}}
//   mission:complete    {missionId, state, outcome}

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/PaktiaMissionDef.h"
#include "PaktiaMissionRunner.generated.h"

class UPaktiaEventSubsystem;
class FJsonObject;

DECLARE_LOG_CATEGORY_EXTERN(LogPaktiaMission, Log, All);

/** Ports MissionCheckpoint. SystemsJson is the JSON object string from IPaktiaMissionSystems. */
USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaMissionCheckpoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Missions")
    FString MissionId;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Missions")
    FString State;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Missions")
    TArray<FString> Flags;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Missions")
    TArray<FString> History;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Missions")
    FString SystemsJson;
};

/** Effect handler, ports EffectHandler. Registered by effect name. */
using FPaktiaEffectHandler = TFunction<void(const FPaktiaMissionStateDef&)>;

DECLARE_MULTICAST_DELEGATE_TwoParams(FPaktiaMissionStateNative, const FString& /*From, empty if none*/, const FString& /*To*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FPaktiaMissionBlockedNative, const FString& /*State*/, const TArray<FString>& /*Missing*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPaktiaMissionCheckpointNative, const FPaktiaMissionCheckpoint&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FPaktiaMissionCompleteNative, const FString& /*State*/, const FString& /*Outcome*/);

UCLASS()
class PROJECTPAKTIA_API UPaktiaMissionRunner : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Builds the runner and enters the start state (TS constructor).
     * Fails (returns false, OutError set, nothing entered) if a state uses an effect with no
     * handler ("startup error" in the TS) or the start state does not exist.
     * @param SystemsProvider optional; must implement IPaktiaMissionSystems.
     * @param Events optional; when null only the native delegates fire.
     */
    bool Initialize(const FPaktiaMissionDef& InDefinition, const TMap<FString, FPaktiaEffectHandler>& InEffects,
        UPaktiaEventSubsystem* Events, UObject* SystemsProvider, FString& OutError);

    // ---- State (TS getters) ----
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString GetMissionId() const { return Definition.Id; }

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString GetState() const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString GetObjective() const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    TArray<FString> GetFlags() const { return Flags; }

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    TArray<FString> GetHistory() const { return Trail; }

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool IsComplete() const;

    /** Branch names of the current state, in definition order. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    TArray<FString> GetChoices() const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool HasBranch(const FString& Branch) const;

    /** "SUCCESS" / "PARTIAL" / "FAILED", or empty while the mission is running. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString GetOutcome() const { return Outcome; }

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool HasCheckpoint() const { return bHasCheckpoint; }

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FPaktiaMissionCheckpoint GetCheckpoint() const { return LastCheckpoint; }

    const FPaktiaMissionDef& GetDefinition() const { return Definition; }

    // ---- Flags ----
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    void SetFlag(const FString& Flag);

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    void ClearFlag(const FString& Flag);

    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool HasFlag(const FString& Flag) const { return Flags.Contains(Flag); }

    /** Flags the current state still needs before it can be left. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    TArray<FString> Missing() const;

    // ---- Transitions (return the resulting state id, like the TS) ----
    /** Moves to `Next` when requirements are met. Publishes mission:blocked otherwise. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString Advance();

    /**
     * Takes a named branch. The TS throws on an unknown branch; here it logs an error and
     * leaves the state unchanged (use HasBranch to check first).
     */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString Choose(const FString& Branch);

    /** Ends the mission in its FailState (if defined and not already complete). */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    FString Fail();

    /** Returns to the last checkpoint: state, flags, history and the mission's systems. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool RestoreCheckpoint();

    /** As RestoreCheckpoint but with an explicit checkpoint (TS: restoreCheckpoint(checkpoint)). */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool RestoreCheckpointData(const FPaktiaMissionCheckpoint& Checkpoint);

    // ---- Native events for C++ listeners ----
    FPaktiaMissionStateNative OnState;
    FPaktiaMissionBlockedNative OnBlocked;
    FPaktiaMissionCheckpointNative OnCheckpoint;
    FPaktiaMissionCompleteNative OnComplete;

    // ---- Payload builders (JSON identical in shape to MissionEvents) ----
    static TSharedRef<FJsonObject> CheckpointToJson(const FPaktiaMissionCheckpoint& Checkpoint);

private:
    bool Ready();
    bool Go(const FString& Id);
    void Enter(const FPaktiaMissionStateDef& State, const FString& From);
    const FPaktiaMissionStateDef* FindState(const FString& Id) const;
    FString SnapshotSystems() const;
    void Publish(const TCHAR* EventName, const TSharedRef<FJsonObject>& Payload);

    UPROPERTY()
    FPaktiaMissionDef Definition;

    UPROPERTY()
    TObjectPtr<UObject> SystemsObject;

    UPROPERTY()
    TObjectPtr<UPaktiaEventSubsystem> EventSubsystem;

    /** Index into Definition.States of the current state; INDEX_NONE before Initialize. */
    int32 CurrentIndex = INDEX_NONE;

    TMap<FString, int32> StateIndex;
    TMap<FString, FPaktiaEffectHandler> Effects;

    TArray<FString> Flags;   // insertion-ordered set (TS Set order)
    TArray<FString> Trail;
    FString Outcome;
    FPaktiaMissionCheckpoint LastCheckpoint;
    bool bHasCheckpoint = false;
};
