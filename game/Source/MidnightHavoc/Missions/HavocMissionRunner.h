#pragma once

// Ports: src/gameplay/missions/framework/MissionRunner.ts
// Runs an FHavocMissionDef: follows `Next`, takes named `Branches`, enforces `Requires`
// flags, runs `Effects` through registered handlers, records checkpoints and publishes
// every transition on UHavocEventSubsystem (and on native delegates for C++ listeners).
//
// Event names and JSON payloads match MissionEvents in the TypeScript:
//   mission:state       {missionId, from?, to}            ("from" omitted when undefined)
//   mission:blocked     {missionId, state, missing:[...]}
//   mission:checkpoint  {missionId, checkpoint:{missionId, state, flags, history, systems:{...}}}
//   mission:complete    {missionId, state, outcome}

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/HavocMissionDef.h"
#include "HavocMissionRunner.generated.h"

class UHavocEventSubsystem;
class FJsonObject;

DECLARE_LOG_CATEGORY_EXTERN(LogHavocMission, Log, All);

/** Ports MissionCheckpoint. SystemsJson is the JSON object string from IHavocMissionSystems. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocMissionCheckpoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Missions")
    FString MissionId;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Missions")
    FString State;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Missions")
    TArray<FString> Flags;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Missions")
    TArray<FString> History;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Missions")
    FString SystemsJson;
};

/** Effect handler, ports EffectHandler. Registered by effect name. */
using FHavocEffectHandler = TFunction<void(const FHavocMissionStateDef&)>;

DECLARE_MULTICAST_DELEGATE_TwoParams(FHavocMissionStateNative, const FString& /*From, empty if none*/, const FString& /*To*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHavocMissionBlockedNative, const FString& /*State*/, const TArray<FString>& /*Missing*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FHavocMissionCheckpointNative, const FHavocMissionCheckpoint&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHavocMissionCompleteNative, const FString& /*State*/, const FString& /*Outcome*/);

UCLASS()
class MIDNIGHTHAVOC_API UHavocMissionRunner : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Builds the runner and enters the start state (TS constructor).
     * Fails (returns false, OutError set, nothing entered) if a state uses an effect with no
     * handler ("startup error" in the TS) or the start state does not exist.
     * @param SystemsProvider optional; must implement IHavocMissionSystems.
     * @param Events optional; when null only the native delegates fire.
     */
    bool Initialize(const FHavocMissionDef& InDefinition, const TMap<FString, FHavocEffectHandler>& InEffects,
        UHavocEventSubsystem* Events, UObject* SystemsProvider, FString& OutError);

    // ---- State (TS getters) ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString GetMissionId() const { return Definition.Id; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString GetState() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString GetObjective() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    TArray<FString> GetFlags() const { return Flags; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    TArray<FString> GetHistory() const { return Trail; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    bool IsComplete() const;

    /** Branch names of the current state, in definition order. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    TArray<FString> GetChoices() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    bool HasBranch(const FString& Branch) const;

    /** "SUCCESS" / "PARTIAL" / "FAILED", or empty while the mission is running. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString GetOutcome() const { return Outcome; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    bool HasCheckpoint() const { return bHasCheckpoint; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FHavocMissionCheckpoint GetCheckpoint() const { return LastCheckpoint; }

    const FHavocMissionDef& GetDefinition() const { return Definition; }

    // ---- Flags ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    void SetFlag(const FString& Flag);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    void ClearFlag(const FString& Flag);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    bool HasFlag(const FString& Flag) const { return Flags.Contains(Flag); }

    /** Flags the current state still needs before it can be left. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    TArray<FString> Missing() const;

    // ---- Transitions (return the resulting state id, like the TS) ----
    /** Moves to `Next` when requirements are met. Publishes mission:blocked otherwise. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString Advance();

    /**
     * Takes a named branch. The TS throws on an unknown branch; here it logs an error and
     * leaves the state unchanged (use HasBranch to check first).
     */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString Choose(const FString& Branch);

    /** Ends the mission in its FailState (if defined and not already complete). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    FString Fail();

    /** Returns to the last checkpoint: state, flags, history and the mission's systems. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    bool RestoreCheckpoint();

    /** As RestoreCheckpoint but with an explicit checkpoint (TS: restoreCheckpoint(checkpoint)). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Missions")
    bool RestoreCheckpointData(const FHavocMissionCheckpoint& Checkpoint);

    // ---- Native events for C++ listeners ----
    FHavocMissionStateNative OnState;
    FHavocMissionBlockedNative OnBlocked;
    FHavocMissionCheckpointNative OnCheckpoint;
    FHavocMissionCompleteNative OnComplete;

    // ---- Payload builders (JSON identical in shape to MissionEvents) ----
    static TSharedRef<FJsonObject> CheckpointToJson(const FHavocMissionCheckpoint& Checkpoint);

private:
    bool Ready();
    bool Go(const FString& Id);
    void Enter(const FHavocMissionStateDef& State, const FString& From);
    const FHavocMissionStateDef* FindState(const FString& Id) const;
    FString SnapshotSystems() const;
    void Publish(const TCHAR* EventName, const TSharedRef<FJsonObject>& Payload);

    UPROPERTY()
    FHavocMissionDef Definition;

    UPROPERTY()
    TObjectPtr<UObject> SystemsObject;

    UPROPERTY()
    TObjectPtr<UHavocEventSubsystem> EventSubsystem;

    /** Index into Definition.States of the current state; INDEX_NONE before Initialize. */
    int32 CurrentIndex = INDEX_NONE;

    TMap<FString, int32> StateIndex;
    TMap<FString, FHavocEffectHandler> Effects;

    TArray<FString> Flags;   // insertion-ordered set (TS Set order)
    TArray<FString> Trail;
    FString Outcome;
    FHavocMissionCheckpoint LastCheckpoint;
    bool bHasCheckpoint = false;
};
