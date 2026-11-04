#pragma once

// Mirrors data/maps/*.json. Parsed by UPaktiaDataSubsystem.
// FJsonObjectConverter matches JSON keys to property names with the first
// letter lower-cased (FailState <-> "failState"). Properties whose JSON key
// does not map that way (bool "fictional", "class", ...) are filled in by hand
// in PaktiaDataSubsystem.cpp.

#include "CoreMinimal.h"
#include "PaktiaMapDef.generated.h"

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaSectorDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Role;
};

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaMapSizeDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    double Width = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    double Height = 0.0;
};

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaMapDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Name;

    /** JSON "fictional" (filled by hand). */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    bool bFictional = true;

    /** "fictional" (default when absent) or "real_world_hypothetical". */
    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Setting;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Note;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FString Mode;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    FPaktiaMapSizeDef CoreSizeMeters;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FPaktiaSectorDef> Sectors;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FString> SurpriseEvents;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Data")
    TArray<FString> UndergroundRoutes;
};
