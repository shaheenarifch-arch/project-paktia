#include "World/HavocTankerSpline.h"
#include "Components/SplineComponent.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"
#include "Missions/Episode04/HavocTankerEscort.h"
#include "Engine/World.h"

namespace
{
    UHavocOpenStraitMission* FindMission(const UWorld* World)
    {
        UHavocMissionSubsystem* Missions = World ? World->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
        return Missions ? Missions->GetOpenStrait() : nullptr;
    }
}

AHavocTankerSpline::AHavocTankerSpline()
{
    PrimaryActorTick.bCanEverTick = false;
    Route = CreateDefaultSubobject<USplineComponent>(TEXT("Route"));
    RootComponent = Route;
}

TArray<FString> AHavocTankerSpline::GetTransitingTankerIds() const
{
    TArray<FString> Ids;
    const UHavocOpenStraitMission* Mission = FindMission(GetWorld());
    const UHavocTankerEscort* Escort = Mission ? Mission->GetEscort() : nullptr;
    if (!Escort)
    {
        return Ids;
    }
    for (const FHavocTanker& Tanker : Escort->GetTankers())
    {
        if (Tanker.State == EHavocTankerState::Transiting && Tanker.Lane == LaneId)
        {
            Ids.Add(Tanker.Id);
        }
    }
    return Ids;
}

int32 AHavocTankerSpline::GetTransitingCount() const
{
    return GetTransitingTankerIds().Num();
}

FVector AHavocTankerSpline::GetLocationAtDistance(float DistanceCm) const
{
    return Route->GetLocationAtDistanceAlongSpline(DistanceCm, ESplineCoordinateSpace::World);
}

float AHavocTankerSpline::GetRouteLength() const
{
    return Route->GetSplineLength();
}

bool AHavocTankerSpline::BeginTransit(const FString& TankerId)
{
    UHavocOpenStraitMission* Mission = FindMission(GetWorld());
    return Mission && Mission->StartTransit(TankerId, LaneId);
}

void AHavocTankerSpline::CompleteTransit(const FString& TankerId)
{
    if (UHavocOpenStraitMission* Mission = FindMission(GetWorld()))
    {
        Mission->TankerArrived(TankerId);
    }
}
