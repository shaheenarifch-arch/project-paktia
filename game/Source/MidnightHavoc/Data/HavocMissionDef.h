#pragma once

// Mirrors data/missions/*.json (campaign episodes and the stronghold definition).
// Every field except Id is optional: a missing key keeps the default below.
// See HavocMapDef.h for the JSON key / property name convention.

#include "CoreMinimal.h"
#include "HavocMissionDef.generated.h"

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocMissionStateDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Objective;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Next;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FString> Effects;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FString> Requires;

    /** Branch name -> target state id. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TMap<FString, FString> Branches;

    /** JSON "checkpoint" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    bool bCheckpoint = false;

    /** JSON "terminal" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    bool bTerminal = false;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Outcome;
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocVec3Def
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double X = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double Y = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double Z = 0.0;
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocProtectedZoneDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Type;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FHavocVec3Def Position;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double Radius = 0.0;
};

/** One phase of the stronghold definition (data/missions/operation-*.json). */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocMissionPhaseDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 Order = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FString> Objectives;
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocMissionDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 Episode = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Name;

    /** JSON "fictional" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    bool bFictional = true;

    /** "fictional" (default when absent) or "real_world_hypothetical". */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Setting;

    /** Id of the map in data/maps. Empty for the stronghold definition. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Map;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Mode;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Summary;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Briefing;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FString> Sectors;

    /** Id of the first state. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Start;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString FailState;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FHavocMissionStateDef> States;

    /** Id of the stronghold mission this episode plays (episode 3). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Stronghold;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FHavocMissionPhaseDef> Phases;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TMap<FString, FString> PhaseRules;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TMap<FString, FString> PhaseObjectives;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TMap<FString, FString> Unlocks;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FHavocProtectedZoneDef> ProtectedZones;

    // Reinforcement fields
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 BaseReinforcements = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 ReinforcementsPerNomad = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 BonusIfEpisode3Succeeded = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 SurpriseEventPhase = 0;

    // Episode 4 scenario tuning
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 Tankers = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double TerminalIntactThreshold = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double SupplyFlowTarget = 0.0;
};
