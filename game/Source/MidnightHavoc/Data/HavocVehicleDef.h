#pragma once

// Mirrors data/vehicles/*.json. Optional fields keep their defaults when missing.
// See HavocMapDef.h for the JSON key / property name convention.

#include "CoreMinimal.h"
#include "HavocVehicleDef.generated.h"

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocVehicleDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Name;

    /** JSON "fictional" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    bool bFictional = true;

    /** JSON "class" (filled by hand; "class" is a C++ keyword). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString ClassName;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FString> Seats;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FString> Systems;

    /** JSON "enabled" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    bool bEnabled = true;
};
