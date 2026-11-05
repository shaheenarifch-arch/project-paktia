#include "World/HavocLaneHazard.h"
#include "World/HavocWorldLog.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"
#include "Engine/World.h"

namespace
{
    UHavocOpenStraitMission* FindHazardMission(const UWorld* World)
    {
        UHavocMissionSubsystem* Missions = World ? World->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
        return Missions ? Missions->GetOpenStrait() : nullptr;
    }
}

AHavocLaneHazard::AHavocLaneHazard()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
}

void AHavocLaneHazard::BeginPlay()
{
    Super::BeginPlay();
    bRevealed = false;
    bCleared = false;
    SetActorHiddenInGame(true);
}

void AHavocLaneHazard::ResetForCheckpoint()
{
    bRevealed = false;
    bCleared = false;
    SetActorHiddenInGame(true);
}

bool AHavocLaneHazard::Reveal()
{
    if (bRevealed || bCleared)
    {
        return false;
    }
    UHavocOpenStraitMission* Mission = FindHazardMission(GetWorld());
    if (!Mission)
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Lane hazard '%s': no Episode 04 mission active"), *HazardId.ToString());
        return false;
    }
    // The mission only accepts reveals during lane clearance; 0 means nothing was revealed.
    if (Mission->RevealHazards(LaneId, 1) < 1)
    {
        return false;
    }
    bRevealed = true;
    SetActorHiddenInGame(false);
    OnRevealed();
    return true;
}

bool AHavocLaneHazard::Clear()
{
    if (!bRevealed || bCleared)
    {
        return false;
    }
    UHavocOpenStraitMission* Mission = FindHazardMission(GetWorld());
    if (!Mission || !Mission->ClearHazard(LaneId))
    {
        return false;
    }
    bCleared = true;
    SetActorHiddenInGame(true);
    OnCleared();
    return true;
}
