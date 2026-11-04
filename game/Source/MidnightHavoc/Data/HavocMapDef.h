#pragma once

// Mirrors data/maps/*.json. Parsed by UHavocDataSubsystem.
// FJsonObjectConverter matches JSON keys to property names with the first
// letter lower-cased (FailState <-> "failState"). Properties whose JSON key
// does not map that way (bool "fictional", "class", ...) are filled in by hand
// in HavocDataSubsystem.cpp.

#include "CoreMinimal.h"
#include "HavocMapDef.generated.h"

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocSectorDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Role;
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocMapSizeDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double Width = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    double Height = 0.0;
};

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocMapDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Name;

    /** JSON "fictional" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    bool bFictional = true;

    /** "fictional" (default when absent) or "real_world_hypothetical". */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Setting;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Note;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FString Mode;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    FHavocMapSizeDef CoreSizeMeters;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FHavocSectorDef> Sectors;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FString> SurpriseEvents;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Data")
    TArray<FString> UndergroundRoutes;
};
