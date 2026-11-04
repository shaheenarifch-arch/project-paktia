#pragma once

// Mirrors data/missions/*.json (campaign episodes and the stronghold definition).
// Every field except Id is optional: a missing key keeps the default below.
// See PaktiaMapDef.h for the JSON key / property name convention.

#include "CoreMinimal.h"
#include "PaktiaMissionDef.generated.h"

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaMissionStateDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Objective;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Next;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FString> Effects;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FString> Requires;

    /** Branch name -> target state id. */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TMap<FString, FString> Branches;

    /** JSON "checkpoint" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    bool bCheckpoint = false;

    /** JSON "terminal" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    bool bTerminal = false;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Outcome;
};

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaVec3Def
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    double X = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    double Y = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    double Z = 0.0;
};

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaProtectedZoneDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Type;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FPaktiaVec3Def Position;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    double Radius = 0.0;
};

/** One phase of the stronghold definition (data/missions/operation-*.json). */
USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaMissionPhaseDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    int32 Order = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FString> Objectives;
};

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaMissionDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    int32 Episode = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Name;

    /** JSON "fictional" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    bool bFictional = true;

    /** "fictional" (default when absent) or "real_world_hypothetical". */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Setting;

    /** Id of the map in data/maps. Empty for the stronghold definition. */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Map;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Mode;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Summary;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Briefing;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FString> Sectors;

    /** Id of the first state. */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Start;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString FailState;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FPaktiaMissionStateDef> States;

    /** Id of the stronghold mission this episode plays (episode 3). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Stronghold;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FPaktiaMissionPhaseDef> Phases;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TMap<FString, FString> PhaseRules;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TMap<FString, FString> PhaseObjectives;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TMap<FString, FString> Unlocks;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FPaktiaProtectedZoneDef> ProtectedZones;

    // Reinforcement fields
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    int32 BaseReinforcements = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    int32 ReinforcementsPerNomad = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    int32 BonusIfEpisode3Succeeded = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    int32 SurpriseEventPhase = 0;

    // Episode 4 scenario tuning
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    int32 Tankers = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    double TerminalIntactThreshold = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    double SupplyFlowTarget = 0.0;
};
