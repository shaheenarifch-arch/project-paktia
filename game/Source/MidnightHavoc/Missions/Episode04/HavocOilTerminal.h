#pragma once

// Ports: src/gameplay/missions/episode04/OilTerminal.ts
// The Kharg oil terminal as game objects: each facility has an integrity of 0..1.
// Abstract game objects only; no real-world equipment or targeting data.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HavocOilTerminal.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocTerminalFacility
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    double Integrity = 1.0;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocOilTerminal : public UObject
{
    GENERATED_BODY()

public:
    UHavocOilTerminal();

    /** Ports damage(): returns false for an unknown facility id. Integrity floors at 0. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool Damage(FName FacilityId, double Amount);

    /** Average integrity across facilities, 0..1 (3 decimals, like the TS). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    double GetIntegrity() const;

    /** Exports need at least one working jetty (> 0.3) and the pump station (> 0.3). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool CanExport() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    TArray<FHavocTerminalFacility> GetFacilities() const { return Facilities; }

    TSharedRef<FJsonObject> Snapshot() const;
    void Restore(const TSharedPtr<FJsonObject>& Data);

private:
    UPROPERTY()
    TArray<FHavocTerminalFacility> Facilities;
};
