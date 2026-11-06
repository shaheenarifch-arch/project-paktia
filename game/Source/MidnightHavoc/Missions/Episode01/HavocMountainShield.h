#pragma once

// Ports: src/gameplay/missions/mission01/ (MountainShield.ts, DroneTutorial.ts, RidgeStation.ts,
// Counterattack.ts, HelicopterCrashEvent.ts). Episode 01, "Operation Mountain Shield".
// Pure-data state machine (no actors): the Episode 01 level flow drives it, like
// UHavocOpenStraitMission does for Episode 04. Aegis is a flag until its C++ port lands.
// Abstract game rules only. Help: game/README.md; campaign order: docs/campaign.md

#include "CoreMinimal.h"
#include "Aircraft/HavocCrashSequence.h"
#include "Aircraft/HavocHelicopterModel.h"

/** DroneTutorial.ts steps, in order. Prompts are shown on screen by the HUD. */
enum class EHavocTutorialStep : uint8
{
    LaunchRaven,
    MoveRaven,
    ObserveContact,
    ConfirmContact,
    ReturnRaven,
    Complete
};

struct MIDNIGHTHAVOC_API FHavocDroneTutorial
{
    EHavocTutorialStep Step = EHavocTutorialStep::LaunchRaven;

    /** Advances only when Completed is the current step (TS advance()). Returns the new step. */
    EHavocTutorialStep Advance(EHavocTutorialStep Completed);

    /** On-screen prompt for the current step. */
    FString GetPrompt() const;
};

enum class EHavocRidgeStationState : uint8 { Unknown, Discovered, Contested, Captured, Destroyed, Lost };

struct MIDNIGHTHAVOC_API FHavocRidgeStation
{
    EHavocRidgeStationState State = EHavocRidgeStationState::Unknown;

    void Discover();
    void Contest();
    void Capture();
    void Destroy();
    void Lose();
    bool IsAegisOnline() const { return State == EHavocRidgeStationState::Captured; }
};

enum class EHavocCounterattackState : uint8 { Inactive, Preparing, Active, Repelled, CheckpointLost };

struct MIDNIGHTHAVOC_API FHavocCounterattack
{
    EHavocCounterattackState State = EHavocCounterattackState::Inactive;

    void BeginPreparation() { State = EHavocCounterattackState::Preparing; }
    void Begin();
    void Repel();
    void LoseCheckpoint();
};

/** The Mission01State names from MountainShield.ts, in the order they are played. */
struct MIDNIGHTHAVOC_API FHavocMountainShield
{
    /** Current state name, e.g. "M01_INIT". */
    FString State = TEXT("M01_INIT");

    FHavocDroneTutorial Tutorial;
    FHavocRidgeStation RidgeStation;
    FHavocCounterattack Counterattack;
    FHavocHelicopterState Helicopter;
    FHavocCrashSequence CrashSequence;
    bool bAegisOnline = false;

    FHavocMountainShield();

    /** Enters Next and runs its side effects (TS transition()). */
    void Transition(const FString& Next);

    /** Advances along the crash chain COUNTERATTACK -> ... -> FINAL_DRONE_DEFENSE. Returns the new state. */
    FString ContinueCrashSequence();

    /** Crew abandoned: carryover for Episode 02. Goes straight to M01_FINAL_DRONE_DEFENSE. */
    void AbandonCrashRecovery();

    /** True when every crew member was recovered (Episode 02 carryover "crew recovered"). */
    bool WasCrewRecovered() const;

    /** All states in play order; the first is the start state. */
    static const TArray<FString>& GetStates();
};
