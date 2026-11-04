#pragma once

// Ports: src/gameplay/missions/episode04/ShippingLanes.ts
// The strait's two shipping lanes. Each holds hidden hazards (abstract game obstacles).
// Drones reveal them; clearance teams remove revealed ones. A lane opens when every
// hazard in it is cleared.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HavocShippingLanes.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct MIDNIGHTHAVOC_API FHavocLane
{
    GENERATED_BODY()

    /** "inbound" or "outbound". */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    int32 Hazards = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    int32 Revealed = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|Episode04")
    int32 Cleared = 0;
};

UCLASS()
class MIDNIGHTHAVOC_API UHavocShippingLanes : public UObject
{
    GENERATED_BODY()

public:
    /** Replaces the lanes: inbound then outbound (TS default hazards are 4 and 3). */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    void Seed(int32 InboundHazards, int32 OutboundHazards);

    /** A drone pass reveals up to Count more hazards. Returns how many were revealed. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    int32 Reveal(FName LaneId, int32 Count);

    /** A clearance team removes one revealed hazard. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool Clear(FName LaneId);

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool IsOpen(FName LaneId) const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    bool AllOpen() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    TArray<FHavocLane> GetLanes() const { return Lanes; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Episode04")
    int32 NumLanes() const { return Lanes.Num(); }

    TSharedRef<FJsonObject> Snapshot() const;
    void Restore(const TSharedPtr<FJsonObject>& Data);

private:
    FHavocLane* FindLane(FName LaneId);
    const FHavocLane* FindLane(FName LaneId) const;

    UPROPERTY()
    TArray<FHavocLane> Lanes;
};
