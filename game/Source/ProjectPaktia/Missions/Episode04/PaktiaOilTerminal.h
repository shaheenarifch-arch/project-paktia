#pragma once

// Ports: src/gameplay/missions/episode04/OilTerminal.ts
// The Kharg oil terminal as game objects: each facility has an integrity of 0..1.
// Abstract game objects only; no real-world equipment or targeting data.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PaktiaOilTerminal.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct PROJECTPAKTIA_API FPaktiaTerminalFacility
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Episode04")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Episode04")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Paktia|Episode04")
    double Integrity = 1.0;
};

UCLASS()
class PROJECTPAKTIA_API UPaktiaOilTerminal : public UObject
{
    GENERATED_BODY()

public:
    UPaktiaOilTerminal();

    /** Ports damage(): returns false for an unknown facility id. Integrity floors at 0. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    bool Damage(FName FacilityId, double Amount);

    /** Average integrity across facilities, 0..1 (3 decimals, like the TS). */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    double GetIntegrity() const;

    /** Exports need at least one working jetty (> 0.3) and the pump station (> 0.3). */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    bool CanExport() const;

    UFUNCTION(BlueprintCallable, Category = "Paktia|Episode04")
    TArray<FPaktiaTerminalFacility> GetFacilities() const { return Facilities; }

    TSharedRef<FJsonObject> Snapshot() const;
    void Restore(const TSharedPtr<FJsonObject>& Data);

private:
    UPROPERTY()
    TArray<FPaktiaTerminalFacility> Facilities;
};
