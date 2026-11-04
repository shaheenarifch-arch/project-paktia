#pragma once

// Ports src/gameplay/aircraft/HelicopterCrashController.ts.
// A pure-data state machine. Methods that touch the aircraft take the FHavocHelicopterState
// (the TS holds it as a constructor field); AHavocFalconHelicopter owns both and wraps every
// transition in a BlueprintCallable function.

#include "CoreMinimal.h"
#include "Aircraft/HavocAircraftTypes.h"
#include "Aircraft/HavocHelicopterModel.h"
#include "HavocCrashSequence.generated.h"

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocCrashSequence
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    EHavocCrashSequenceState State = EHavocCrashSequenceState::Inactive;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FHavocCrashSite CrashSite;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FHavocRecoveryZone RecoveryZone;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FHavocCrewState Crew;

    FHavocCrashSequence();

    void BeginInbound(FHavocHelicopterState& Helicopter, const FVector& Position);
    /** Only from AIRCRAFT_INBOUND: applies flightControl 0.45, engine 0.55, structure 0.25 damage. */
    void TriggerDistress(FHavocHelicopterState& Helicopter);
    void LoseSignal();
    void RegisterCrash(FHavocHelicopterState& Helicopter, const FVector& Position);
    void BeginSearch();
    void LocateCrashSite(const FVector& Position);
    void SecureCrashSite();
    void BeginCrewRecovery();
    void StartRecoveryDefense();
    /** False (and no change) unless every crew member is evacuated or lost. */
    bool CompleteEvacuation();
    void Abandon();
};
