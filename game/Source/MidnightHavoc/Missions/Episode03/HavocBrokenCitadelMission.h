#pragma once

// Ports: src/gameplay/missions/episode03/BrokenCitadel.ts (Episode 03, "Operation Broken Citadel").
// The five phases come from the Stronghold Assault data (data/missions/operation-broken-citadel.json);
// the episode file (data/missions/episode-03-broken-citadel.json) adds the campaign rules: phase
// completion rules, unlocks, reinforcements and the surprise event. The state graph is built from
// both (BuildDefinition), as in the TS. This class implements IHavocMissionSystems so checkpoints
// capture and restore captures, unlocks, reinforcements and the surprise event.
// Aegis is deliberately abstract and not ported (see IsAegisOnline and GetProtectedZones for what it
// must honour). Abstract game rules only.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/HavocMissionDef.h"
#include "Missions/HavocMissionSystems.h"
#include "Missions/Episode02/HavocSilentCorridorMission.h"
#include "Missions/Episode03/HavocSurpriseEventDirector.h"
#include "HavocBrokenCitadelMission.generated.h"

class UHavocEventSubsystem;
class UHavocMissionRunner;

// FHavocEpisode02Carryover (crew delivered, Nomads remaining) comes from Episode 02's mission header.

/** Outcome and what Episode 04 (and the campaign) read afterwards. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocEpisode03Result
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode03")
    int32 Episode = 3;

    /** "SUCCESS" / "FAILED" or empty while running. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode03")
    FString Outcome;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode03")
    int32 ReinforcementsLeft = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode03")
    int32 ObjectivesCaptured = 0;

    /** Highest phase reached (1..5), 0 before the assault starts. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode03")
    int32 PhaseReached = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode03")
    bool bAegisOnline = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode03")
    bool bMedicalStationProtected = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode03")
    FString SurpriseEvent;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocBrokenCitadelMission : public UObject, public IHavocMissionSystems
{
    GENERATED_BODY()

public:
    /** Ports buildBrokenCitadelDefinition: briefing, staging, one state per phase, secured and failed. */
    static bool BuildDefinition(const FHavocMissionDef& Episode, const FHavocMissionDef& Stronghold, FHavocMissionDef& OutDefinition, FString& OutError);

    /**
     * Builds the runner and enters the briefing.
     * @param Episode the episode file (episode_03_broken_citadel); Stronghold the operation file.
     * @param SurpriseEvents the map's surpriseEvents; Seed picks one deterministically.
     * @param Carryover from Episode 02; defaults (no crew, no Nomads) when Episode 02 was not played.
     */
    bool Initialize(const FHavocMissionDef& Episode, const FHavocMissionDef& Stronghold, const TArray<FString>& SurpriseEvents,
        const FHavocEpisode02Carryover& Carryover, const FString& Seed, UHavocEventSubsystem* Events, FString& OutError);

    // ---- IHavocMissionSystems ----
    virtual FString SnapshotSystems() const override;
    virtual void RestoreSystems(const FString& SnapshotJson) override;

    // ---- Accessors ----
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    UHavocMissionRunner* GetRunner() const { return Runner; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    FString GetState() const;

    /** 1..5 while in a phase state, else 0. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    int32 GetPhase() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    int32 GetReinforcements() const { return Reinforcements; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    TArray<FString> GetCapturedObjectives() const { return Captured; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    TArray<FString> GetUnlocks() const { return Unlocked; }

    /** Objectives of the current phase still open. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    TArray<FString> GetOpenObjectives() const;

    /** Objective ids of a phase (1..5), in data order. Empty for an unknown phase. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    TArray<FString> GetPhaseObjectives(int32 Phase) const;

    // ---- Mission flow (same names and rules as the TS) ----
    /** Briefing -> staging -> phase 1. Returns the resulting state. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    FString BeginAssault();

    /**
     * Server-confirmed capture. Only objectives of the current phase count; a repeat does not.
     * Completes the phase by its rule ("any" or "all", default "all") and advances.
     */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    bool CaptureObjective(const FString& ObjectiveId);

    /** Squads lost; at zero the assault fails (back to the last checkpoint on restore). Returns reinforcements left. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    int32 SpendReinforcements(int32 Count);

    /** Falcon air support needs both the airfield (capture_airfield) and the crew Episode 02 delivered. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    bool HasAirSupport() const;

    /** 0.6 once both phase-2 relay sites are taken, else 1. Scales enemy counterattack strength. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    double GetCounterattackStrength() const;

    /** The Mountain Fort brings Aegis online (abstract; no targeting logic here). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    bool IsAegisOnline() const { return Unlocked.Contains(TEXT("aegis_online")); }

    /** The medical station is taken and now a protected zone no support may target. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    bool IsMedicalStationProtected() const { return Unlocked.Contains(TEXT("medical_station_protected")); }

    /** The unlock an objective grants (empty if none), from the episode file's unlocks. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    FString GetUnlockFor(const FString& ObjectiveId) const;

    const FHavocSurpriseEventDirector& GetSurprise() const { return Surprise; }
    const FHavocEpisode02Carryover& GetCarryover() const { return Carryover; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    FHavocEpisode03Result GetResult() const;

    /** Ports snapshot(): a JSON summary for HUDs and telemetry. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode03")
    FString GetSummaryJson() const;

    // ---- Protected zones (the episode file's protectedZones; ports Aegis setProtectedZones) ----
    /** All zones declared in the episode file. They are in force from the start, as in the TS (setProtectedZones in the constructor). */
    const TArray<FHavocProtectedZoneDef>& GetProtectedZones() const { return Zones; }

    /** True (and OutZoneId set) if the game-space position (metres) is inside a protected zone. Targeting and support systems must never select anything there. */
    bool IsInProtectedZone(double X, double Y, double Z, FString& OutZoneId) const;

private:
    void ApplyUnlock(const FString& Objective);
    const FHavocMissionPhaseDef* FindPhase(int32 Order) const;

    UPROPERTY()
    TObjectPtr<UHavocMissionRunner> Runner;

    UPROPERTY()
    FHavocMissionDef EpisodeDef;

    UPROPERTY()
    TArray<FHavocMissionPhaseDef> Phases;

    UPROPERTY()
    TArray<FHavocProtectedZoneDef> Zones;

    UPROPERTY()
    FHavocEpisode02Carryover Carryover;

    UPROPERTY()
    int32 Reinforcements = 0;

    TArray<FString> Captured;   // insertion-ordered set
    TArray<FString> Unlocked;   // insertion-ordered set
    FHavocSurpriseEventDirector Surprise;
};
