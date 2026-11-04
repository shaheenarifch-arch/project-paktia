#pragma once

// Pure-data helicopter model (no world needed, so the automation tests can drive it).
//   FHavocHelicopterDamageState -> src/gameplay/aircraft/HelicopterDamageState.ts
//   FHavocCrewState             -> src/gameplay/aircraft/CrewState.ts
//   FHavocHelicopterState       -> src/gameplay/aircraft/HelicopterController.ts
//   FHavocCrashSite             -> src/gameplay/aircraft/CrashSite.ts
//   FHavocRecoveryZone          -> src/gameplay/aircraft/RecoveryZone.ts
// Positions are plain FVectors in whatever unit the caller uses (the pawn passes Unreal units).

#include "CoreMinimal.h"
#include "Aircraft/HavocAircraftTypes.h"
#include "HavocHelicopterModel.generated.h"

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocHelicopterDamageState
{
    GENERATED_BODY()

    /** 0..1, 1 = undamaged. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    double FlightControl = 1.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    double EngineCondition = 1.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    double StructuralCondition = 1.0;

    /** Amounts below 0 are ignored; results are clamped to 0..1. */
    void ApplyDamage(double InFlightControl, double InEngine, double InStructure);

    FHavocHelicopterDamageSnapshot Snapshot() const;
    EHavocHelicopterDamageLevel GetLevel() const;

    /** Level for the lowest of the three conditions (<=0 Destroyed, <=0.15 Disabled, <=0.35 Critical, <=0.7 Damaged). */
    static EHavocHelicopterDamageLevel LevelFor(double Lowest);

    /** Ports reduce(): max(0, min(1, current - max(0, amount))). */
    static double Reduce(double Current, double Amount);
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocCrewState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    TArray<FHavocCrewMember> Members;

    /** The three Falcon crew from HelicopterCrashController.ts (pilot, copilot, crew chief), all UNKNOWN. */
    static TArray<FHavocCrewMember> MakeFalconCrew();

    /** Fills Seat for each member in order from the aircraft data seats. Extra crew get no seat. */
    void AssignSeats(const TArray<FString>& Seats);

    bool Locate(FName Id, EHavocCrewCondition Condition);
    bool Recover(FName Id);

    int32 GetLocatedCount() const;
    int32 GetRecoveredCount() const;
    bool AllRecovered() const;
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocHelicopterState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FString Callsign;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    EHavocHelicopterFlightState FlightState = EHavocHelicopterFlightState::Parked;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FVector LastKnownPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FHavocHelicopterDamageState Damage;

    void BeginInbound(const FVector& InPosition);
    /** Ignored once crashed. INBOUND becomes TRANSIT. */
    void UpdatePosition(const FVector& InPosition);
    /** Only INBOUND or TRANSIT become DISTRESS. */
    void ReportDistress();
    void AttemptEmergencyLanding(const FVector& InPosition);
    void RegisterCrash(const FVector& InPosition);
    /** CRASHED becomes RECOVERED, anything else is ignored. */
    void RecoverAircraft();
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocCrashSite
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    EHavocCrashSiteState State = EHavocCrashSiteState::Unknown;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    bool bHasPosition = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    bool bSmokeVisible = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    bool bBeaconActive = false;

    void BeginSearch();
    void Locate(const FVector& InPosition);
    void Contest();
    void Secure();
    void Abandon();
    void CompleteRecovery();
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocRecoveryZone
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    EHavocRecoveryZoneState State = EHavocRecoveryZoneState::Inactive;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    bool bHasCenter = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Aircraft")
    FVector Center = FVector::ZeroVector;

    void Designate(const FVector& InCenter);
    void Activate();
    void Contest();
    void Secure();
    void CompleteEvacuation();

    /** The TS default radius is 40 (its units); callers in Unreal units pass their own radius. */
    bool Contains(const FVector& InPosition, double Radius = 40.0) const;
};
