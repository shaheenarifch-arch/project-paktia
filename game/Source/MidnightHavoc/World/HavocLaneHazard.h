#pragma once

// A shipping-lane hazard (abstract game object). Hidden until a drone pass reveals it;
// a clearance team then clears it. Both forward to the Episode 04 mission rules.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Drones/HavocDroneDetectable.h"
#include "HavocLaneHazard.generated.h"

UCLASS(Blueprintable)
class MIDNIGHTHAVOC_API AHavocLaneHazard : public AActor, public IHavocDroneDetectable
{
    GENERATED_BODY()

public:
    AHavocLaneHazard();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Lane",
        meta = (ToolTip = "Lane id: inbound or outbound."))
    FName LaneId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Lane",
        meta = (ToolTip = "Unique id for this hazard, e.g. inbound_1."))
    FName HazardId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Lane")
    TObjectPtr<USceneComponent> Root;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Lane")
    bool IsRevealed() const { return bRevealed; }

    UFUNCTION(BlueprintCallable, Category = "Havoc|Lane")
    bool IsCleared() const { return bCleared; }

    /** Called by drones. Shows the actor and reveals one hazard in the lane. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Lane",
        meta = (ToolTip = "Reveal this hazard (called by drones). Shows it and tells the mission. Returns false if already revealed/cleared."))
    bool Reveal();

    /** Removes a revealed hazard. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Lane",
        meta = (ToolTip = "Clear this hazard. It must be revealed first. Returns true if the mission accepted it."))
    bool Clear();

    /** Back to hidden and uncleared. Used when a checkpoint restore puts the mission back into lane clearance. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Lane")
    void ResetForCheckpoint();

    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|Lane")
    void OnRevealed();

    UFUNCTION(BlueprintImplementableEvent, Category = "Havoc|Lane")
    void OnCleared();

    // IHavocDroneDetectable: a drone sensor pass reveals this hazard.
    virtual FName GetDetectableId() const override { return HazardId; }
    virtual bool IsDetectableVisible() const override { return !bCleared; }
    virtual FString GetDetectableClassification() const override { return TEXT("lane_hazard"); }
    virtual void OnDetectedByDrone(FName DroneId) override { Reveal(); }

protected:
    virtual void BeginPlay() override;

private:
    bool bRevealed = false;
    bool bCleared = false;
};
