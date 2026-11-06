#pragma once

// Ports: src/gameplay/missions/mission01/ (MountainShield.ts, DroneTutorial.ts, RidgeStation.ts,
// Counterattack.ts, HelicopterCrashEvent.ts) for Episode 01, "Operation Mountain Shield".
// The state graph lives in data/missions/episode-01-mountain-shield.json (mission_01_mountain_shield);
// this class owns the tutorial, Checkpoint Echo, Ridge Station, the Falcon-2 crash/crew and the
// counterattack, and registers the Episode 01 effect handlers. It implements IHavocMissionSystems so
// checkpoints capture and restore them. Fictional setting, abstract game rules only.
// Campaign carryover (GetCarryover): crew recovered or abandoned, read by Episode 02.
// Help: docs/campaign.md

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/HavocMissionDef.h"
#include "Missions/HavocMissionSystems.h"
#include "HavocMountainShieldMission.generated.h"

class UHavocEventSubsystem;
class UHavocMissionRunner;

/** Ports TutorialStep. The first two steps (move, aim) are new: Shaheen found the controls hard. */
UENUM(BlueprintType)
enum class EHavocTutorialStep : uint8
{
    Move,
    Aim,
    LaunchRaven,
    MoveRaven,
    ObserveContact,
    ConfirmContact,
    ReturnToMarine,
    Complete
};

UENUM(BlueprintType)
enum class EHavocRidgeStationState : uint8 { Unknown, Discovered, Contested, Captured, Destroyed, Lost };

UENUM(BlueprintType)
enum class EHavocCounterattackState : uint8 { Inactive, Preparing, Active, Repelled, CheckpointLost };

/** Ports Mission 01 carryover (Campaign.carryoverFromMission01). */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocEpisode01Result
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode01") int32 Episode = 1;
    /** "SUCCESS" (crew recovered) / "PARTIAL" (crew abandoned) / "FAILED", or empty while running. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode01") FString Outcome;
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode01") bool bCrewRecovered = false;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocMountainShieldMission : public UObject, public IHavocMissionSystems
{
    GENERATED_BODY()

public:
    /** Builds the runner and enters the briefing. Fails (OutError set) if the definition cannot start. */
    bool Initialize(const FHavocMissionDef& Definition, UHavocEventSubsystem* Events, FString& OutError);

    // ---- IHavocMissionSystems ----
    virtual FString SnapshotSystems() const override;
    virtual void RestoreSystems(const FString& SnapshotJson) override;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") UHavocMissionRunner* GetRunner() const { return Runner; }
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") FString GetState() const;
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") FString Progress();

    // ---- Tutorial (ports DroneTutorial.advance: only the current step can be completed) ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") EHavocTutorialStep AdvanceTutorial(EHavocTutorialStep Completed);
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") EHavocTutorialStep GetTutorialStep() const { return TutorialStep; }
    /** On-screen prompt for a step (keys match the controls screen). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") static FString GetTutorialPrompt(EHavocTutorialStep Step);

    // ---- World inputs ----
    /** Checkpoint Echo was held for the full timer. Only in M01_HOLD_ECHO. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") bool HoldEcho();
    /** The relay is set up at Ridge Station: it is captured and Aegis comes online. Only in M01_RIDGE_STATION. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") bool SetUpRidgeStation();
    /** Falcon-2 hit the ground (cutscene finished). Only in M01_FALCON_DOWN. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") bool NotifyCrashSeen();
    /** Player choice at M01_CRASH_DECISION: recover the crew (true) or abandon them (false). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") FString ChooseCrew(bool bRecover);
    /** One crew member reached the evacuation point. All CrewSize recovered completes the recovery. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") int32 RecoverCrewMember();
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") bool RepelCounterattack();
    /** Checkpoint Echo or Ridge Station overrun: the mission fails to its fail state. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") FString LoseCheckpoint();
    /** Debrief: SUCCESS if the crew came home, PARTIAL if abandoned. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") FString Finish();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") EHavocRidgeStationState GetRidgeStation() const { return RidgeStation; }
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") EHavocCounterattackState GetCounterattack() const { return Counterattack; }
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") bool IsAegisOnline() const { return RidgeStation == EHavocRidgeStationState::Captured; }
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") int32 GetCrewRecovered() const { return CrewRecovered; }
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") int32 GetCrewSize() const { return CrewSize; }
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") bool IsCrewAbandoned() const { return bCrewAbandoned; }
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode01") FHavocEpisode01Result GetCarryover() const;

private:
    bool InState(const TCHAR* StateId) const;

    UPROPERTY() TObjectPtr<UHavocMissionRunner> Runner;
    EHavocTutorialStep TutorialStep = EHavocTutorialStep::Move;
    EHavocRidgeStationState RidgeStation = EHavocRidgeStationState::Unknown;
    EHavocCounterattackState Counterattack = EHavocCounterattackState::Inactive;
    int32 CrewSize = 3;
    int32 CrewRecovered = 0;
    bool bCrewLocated = false;
    bool bCrewAbandoned = false;
};
