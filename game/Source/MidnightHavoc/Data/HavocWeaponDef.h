#pragma once

// Mirrors data/weapons/*.json. Optional fields keep their defaults when missing.
// See HavocMapDef.h for the JSON key / property name convention.

#include "CoreMinimal.h"
#include "HavocWeaponDef.generated.h"

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocWeaponDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Name;

    /** JSON "fictional" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    bool bFictional = true;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Category;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FString> FireModes;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    int32 MagazineCapacity = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString DamageProfile;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString RecoilProfile;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString AnimationSet;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString AudioSet;

    /** JSON "enabled" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    bool bEnabled = true;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Notice;
};
