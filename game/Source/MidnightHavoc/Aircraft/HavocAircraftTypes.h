#pragma once

// Shared enums and small structs for the Falcon helicopter.
// Ports the string unions in src/gameplay/aircraft/*.ts. Enum value names are the
// TypeScript names in PascalCase (EMERGENCY_LANDING -> EmergencyLanding).

#include "CoreMinimal.h"
#include "HavocAircraftTypes.generated.h"

/** HelicopterFlightState in HelicopterController.ts. */
UENUM(BlueprintType)
enum class EHavocHelicopterFlightState : uint8
{
    Parked,
    Inbound,
    Transit,
    Distress,
    EmergencyLanding,
    Crashed,
    Recovered
};

/** HelicopterDamageLevel in HelicopterDamageState.ts. */
UENUM(BlueprintType)
enum class EHavocHelicopterDamageLevel : uint8
{
    Operational,
    Damaged,
    Critical,
    Disabled,
    Destroyed
};

/** CrewMemberCondition in CrewState.ts. */
UENUM(BlueprintType)
enum class EHavocCrewCondition : uint8
{
    Unknown,
    Operational,
    Injured,
    Critical,
    Recovered,
    Lost
};

/** CrashSequenceState in HelicopterCrashController.ts. */
UENUM(BlueprintType)
enum class EHavocCrashSequenceState : uint8
{
    Inactive,
    AircraftInbound,
    DistressReceived,
    SignalLost,
    CrashConfirmed,
    SearchActive,
    SiteLocated,
    SiteSecured,
    CrewRecovery,
    RecoveryDefense,
    EvacuationComplete,
    Abandoned
};

/** CrashSiteState in CrashSite.ts. */
UENUM(BlueprintType)
enum class EHavocCrashSiteState : uint8
{
    Unknown,
    Searching,
    Located,
    Contested,
    Secured,
    Abandoned,
    RecoveryComplete
};

/** RecoveryZoneState in RecoveryZone.ts. */
UENUM(BlueprintType)
enum class EHavocRecoveryZoneState : uint8
{
    Inactive,
    Designated,
    Active,
    Contested,
    Secured,
    EvacuationComplete
};

/** CrewMember in CrewState.ts. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocCrewMember
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FName Role;

    /** Seat name from data/aircraft/*.json ("pilot", "gunner", ...). None if the aircraft has fewer seats than crew. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FName Seat;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    EHavocCrewCondition Condition = EHavocCrewCondition::Unknown;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    bool bLocated = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    bool bEvacuated = false;
};

/** HelicopterDamageSnapshot in HelicopterDamageState.ts. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocHelicopterDamageSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    EHavocHelicopterDamageLevel Level = EHavocHelicopterDamageLevel::Operational;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    double FlightControl = 1.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    double EngineCondition = 1.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    double StructuralCondition = 1.0;
};
