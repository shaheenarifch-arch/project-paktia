#pragma once

// The tanker route for one lane. Reads escort state from the Episode 04 tanker logic;
// it does not move the tanker itself (a Blueprint or later step does, along the spline).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HavocTankerSpline.generated.h"

class USplineComponent;

UCLASS(Blueprintable)
class MIDNIGHTHAVOC_API AHavocTankerSpline : public AActor
{
    GENERATED_BODY()

public:
    AHavocTankerSpline();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Tanker",
        meta = (ToolTip = "Lane id this route belongs to: inbound or outbound."))
    FName LaneId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Havoc|Tanker")
    TObjectPtr<USplineComponent> Route;

    /** Number of tankers currently transiting this lane according to the escort logic. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Tanker",
        meta = (ToolTip = "Tankers currently transiting this lane (from the Episode 04 escort state)."))
    int32 GetTransitingCount() const;

    /** Ids of tankers transiting this lane. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Tanker")
    TArray<FString> GetTransitingTankerIds() const;

    /** World location for a distance (cm) along the route. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Tanker",
        meta = (ToolTip = "World location at a distance in cm along the route."))
    FVector GetLocationAtDistance(float DistanceCm) const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Tanker")
    float GetRouteLength() const;

    /** Tells the mission a tanker has entered this lane. False if the lane is not open or no mission. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Tanker",
        meta = (ToolTip = "Begin a transit of TankerId on this lane. Fails if the lane still has hazards."))
    bool BeginTransit(const FString& TankerId);

    /** Tells the mission a tanker reached the end of the route. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Tanker")
    void CompleteTransit(const FString& TankerId);
};
