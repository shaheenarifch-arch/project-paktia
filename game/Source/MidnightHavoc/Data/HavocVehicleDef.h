#pragma once

// Mirrors data/vehicles/*.json. Optional fields keep their defaults when missing.
// See HavocMapDef.h for the JSON key / property name convention.

#include "CoreMinimal.h"
#include "HavocVehicleDef.generated.h"

/** JSON "stats": abstract game-balance values for the playable pawns (metres, seconds, degrees). Never real specs. */
USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocVehicleStats
{
    GENERATED_BODY()

    /** m/s (JSON "maxSpeed"). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double MaxSpeed = 10.0;

    /** m/s^2 (JSON "acceleration"). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double Acceleration = 4.0;

    /** Degrees per second at full steering (JSON "turnRate"). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double TurnRate = 45.0;

    /** m/s vertical speed for flying vehicles, 0 for surface vehicles (JSON "climbRate"). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double ClimbRate = 0.0;
};

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

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FHavocVehicleStats Stats;

    /** JSON "enabled" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    bool bEnabled = true;
};
