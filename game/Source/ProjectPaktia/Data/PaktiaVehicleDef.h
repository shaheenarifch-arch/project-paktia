#pragma once

// Mirrors data/vehicles/*.json. Optional fields keep their defaults when missing.
// See PaktiaMapDef.h for the JSON key / property name convention.

#include "CoreMinimal.h"
#include "PaktiaVehicleDef.generated.h"

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaVehicleDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Name;

    /** JSON "fictional" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    bool bFictional = true;

    /** JSON "class" (filled by hand; "class" is a C++ keyword). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString ClassName;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FString> Seats;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FString> Systems;

    /** JSON "enabled" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    bool bEnabled = true;
};
