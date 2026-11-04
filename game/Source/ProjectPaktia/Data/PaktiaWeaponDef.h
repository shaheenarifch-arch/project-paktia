#pragma once

// Mirrors data/weapons/*.json. Optional fields keep their defaults when missing.
// See PaktiaMapDef.h for the JSON key / property name convention.

#include "CoreMinimal.h"
#include "PaktiaWeaponDef.generated.h"

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaWeaponDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Name;

    /** JSON "fictional" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    bool bFictional = true;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Category;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FString> FireModes;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    int32 MagazineCapacity = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString DamageProfile;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString RecoilProfile;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString AnimationSet;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString AudioSet;

    /** JSON "enabled" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    bool bEnabled = true;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Notice;
};
