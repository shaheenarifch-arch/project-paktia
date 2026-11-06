#pragma once

// Ports: src/gameplay/missions/episode02/SilentCorridor.ts (Episode 02, "Operation Silent Corridor").
// The state graph lives in data/missions/episode-02-silent-corridor.json; this class owns the systems
// (convoy, relays, ambush contacts) and turns gameplay results into the flags the graph waits on.
// It registers the Episode 02 effect handlers and implements IHavocMissionSystems so checkpoints
// capture and restore the convoy, relays and ambush. Positions are game-space metres from Checkpoint
// Echo, the same numbers as the TS (x east, y up, z along the valley). Abstract game rules only.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/HavocMissionDef.h"
#include "Missions/HavocMissionSystems.h"
#include "HavocSilentCorridorMission.generated.h"

class UHavocConvoyEscort;
class UHavocEventSubsystem;
class UHavocMissionRunner;
class UHavocRelayNetwork;

/** Ports E02_POINTS (game-space metres). */
namespace HavocE02
{
    inline FVector RelayKilo() { return FVector(600.0, 40.0, 900.0); }
    inline FVector CorridorEnd() { return FVector(1700.0, 10.0, 1500.0); }
    inline FVector AmbushNarrows() { return FVector(1250.0, 0.0, 1650.0); }
}

/** A contact the Raven or the squads have marked (the TS keeps these in ContactManager; no C++ port yet). */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocE02Contact
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Havoc|Episode02")
    FString Id;

    UPROPERTY(BlueprintReadWrite, Category = "Havoc|Episode02")
    FVector Position = FVector::ZeroVector;

    /** Link quality when it was marked, 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    double Quality = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    double Time = 0.0;

    /** False once the contact is neutralised (ContactManager.loseTracking). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    bool bTracked = true;
};

/** Ports E02Carryover: what Episode 03 reads. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocEpisode02Carryover
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    int32 Episode = 2;

    /** "SUCCESS" / "PARTIAL" / "FAILED" or empty while running. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    FString Outcome;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    bool bCrewDelivered = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    int32 NomadsRemaining = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    bool bWardenOperational = false;

    /** "MAIN_ROAD", "RIVERBED" or "RIVER_DRAINAGE". */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    FString Route;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode02")
    int32 RelaysOnline = 0;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocSilentCorridorMission : public UObject, public IHavocMissionSystems
{
    GENERATED_BODY()

public:
    /**
     * Builds the convoy, relays and runner (TS constructor).
     * @param bCrewRecovered From Mission 01: was the Falcon-2 crew recovered? If not Nomad-1 carries supplies.
     */
    bool Initialize(const FHavocMissionDef& Definition, bool bCrewRecovered, UHavocEventSubsystem* Events, FString& OutError);

    // ---- IHavocMissionSystems ----
    virtual FString SnapshotSystems() const override;
    virtual void RestoreSystems(const FString& SnapshotJson) override;

    // ---- Accessors ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    UHavocMissionRunner* GetRunner() const { return Runner; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    UHavocConvoyEscort* GetConvoy() const { return Convoy; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    UHavocRelayNetwork* GetRelays() const { return Relays; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    FString GetState() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool WasCrewRecovered() const { return bCrewRecovered; }

    // ---- Mission flow (same names and rules as the TS) ----
    /** Advances as far as the current flags allow (several states can open at once). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    FString Progress();

    /** Raven flies out and drops Relay Kilo; it only counts if the link reaches it. Only valid in E02_RELAY_KILO. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool PlaceRelayKilo(const FVector& DronePosition);

    /** Raven reaches the end of the corridor with a usable link (not lost or critical) and marks what it sees. Only valid in E02_CORRIDOR_SCOUT. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool ScoutCorridor(const TArray<FHavocE02Contact>& Observed, const FVector& At, double Now = 0.0);

    /** A squad standing at Relay Kilo restores it after the outage. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool RestoreRelayKilo(const FVector& SquadPosition);

    /** RouteBranch is "riverbed_detour" or "drainage_infiltration". */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    FString ChooseRoute(const FString& RouteBranch);

    /** World input: convoy damage. Losing the crew transport (or, without crew, every Nomad) fails the mission. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    void DamageVehicle(const FString& VehicleId, double Amount);

    /** Each ambush contact the squads neutralise; the ambush is over when none remain. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    bool ClearAmbushContact(const FString& ContactId, double Now = 0.0);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    TArray<FString> GetRemainingAmbush() const { return AmbushContacts; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    void ClearFarBank();

    /** Needs the bridge-cleared state and a working Warden-1. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    void ClearBridge();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    void ClearStaging();

    /** Closes the episode: full strength or with losses, depending on the convoy. Only from E02_STAGING_ESTABLISHED. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    FString Finish();

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    FHavocEpisode02Carryover GetCarryover() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    TArray<FHavocE02Contact> GetContacts() const { return Contacts; }

    /** Ports snapshot(): a JSON summary for HUDs and telemetry. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode02")
    FString GetSummaryJson() const;

private:
    void StartAmbush();
    void UpsertContact(const FString& Id, const FVector& Position, double Quality, double Time);

    UPROPERTY()
    TObjectPtr<UHavocConvoyEscort> Convoy;

    UPROPERTY()
    TObjectPtr<UHavocRelayNetwork> Relays;

    UPROPERTY()
    TObjectPtr<UHavocMissionRunner> Runner;

    UPROPERTY()
    TArray<FHavocE02Contact> Contacts;

    UPROPERTY()
    TArray<FString> AmbushContacts;

    UPROPERTY()
    bool bCrewRecovered = true;
};
