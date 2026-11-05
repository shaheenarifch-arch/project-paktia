#pragma once

// Mirrors the fields of data/units/<faction>.json that the playable loop needs.
// The data layer owns the full contract (see data/README.md); only game-balance ratings
// (1-10) are read here, never real-world figures. ISIS units are AI-only: see IsFactionPlayable.

#include "CoreMinimal.h"
#include "HavocUnitDef.generated.h"

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocUnitDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Faction;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Branch;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Role;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 Crew = 1;

    /** firepower.antiInfantry (game-balance 1-10). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 AntiInfantry = 3;

    /** sensors.optics (game-balance 1-10). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 Optics = 2;

    /** stealth (game-balance 1-10). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 Stealth = 5;

    /** Playable factions only. ISIS is never playable. */
    static bool IsFactionPlayable(const FString& InFaction)
    {
        return InFaction != TEXT("isis");
    }
};
