#include "World/HavocLevelDirector.h"
#include "World/HavocSectorVolume.h"
#include "World/HavocWorldLog.h"
#include "Data/HavocDataSubsystem.h"
#include "Data/HavocMapDef.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AHavocLevelDirector::AHavocLevelDirector()
    : MapId(TEXT("kharg_hormuz"))
    , MissionId(TEXT("episode_04_open_strait"))
{
    PrimaryActorTick.bCanEverTick = false;
}

void AHavocLevelDirector::BeginPlay()
{
    Super::BeginPlay();

    const UWorld* World = GetWorld();
    int32 Directors = 0;
    for (TActorIterator<AHavocLevelDirector> It(World); It; ++It)
    {
        ++Directors;
    }
    if (Directors > 1)
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("%d AHavocLevelDirector actors in the level; only one is expected"), Directors);
    }

    const bool bSectorsOk = ValidateSectors();
    if (!bSectorsOk)
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Map '%s': sector check found problems (see above)"), *MapId.ToString());
    }

    if (bStartMissionOnBeginPlay)
    {
        StartMission();
    }
}

bool AHavocLevelDirector::ValidateSectors()
{
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UHavocDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    FHavocMapDef Map;
    if (!Data || !Data->GetMap(MapId, Map))
    {
        UE_LOG(LogHavocWorld, Error, TEXT("Map '%s' not found in the data subsystem; cannot check sectors"), *MapId.ToString());
        return false;
    }

    TMap<FName, int32> Placed;
    bool bOk = true;
    for (TActorIterator<AHavocSectorVolume> It(World); It; ++It)
    {
        const AHavocSectorVolume* Volume = *It;
        if (Volume->SectorId.IsNone())
        {
            UE_LOG(LogHavocWorld, Warning, TEXT("Sector volume '%s' has no SectorId"), *Volume->GetName());
            bOk = false;
            continue;
        }
        Placed.FindOrAdd(Volume->SectorId) += 1;
    }

    TSet<FName> Known;
    for (const FHavocSectorDef& Sector : Map.Sectors)
    {
        const FName Id(*Sector.Id);
        Known.Add(Id);
        const int32* Count = Placed.Find(Id);
        if (!Count)
        {
            UE_LOG(LogHavocWorld, Error, TEXT("Map '%s': sector '%s' (%s) has no AHavocSectorVolume placed"), *MapId.ToString(), *Sector.Id, *Sector.Name);
            bOk = false;
        }
        else if (*Count > 1)
        {
            UE_LOG(LogHavocWorld, Warning, TEXT("Map '%s': sector '%s' has %d volumes placed (expected 1)"), *MapId.ToString(), *Sector.Id, *Count);
            bOk = false;
        }
    }

    for (const TPair<FName, int32>& Pair : Placed)
    {
        if (!Known.Contains(Pair.Key))
        {
            UE_LOG(LogHavocWorld, Error, TEXT("Map '%s': placed sector volume '%s' is not in the map definition"), *MapId.ToString(), *Pair.Key.ToString());
            bOk = false;
        }
    }

    if (bOk)
    {
        UE_LOG(LogHavocWorld, Log, TEXT("Map '%s': all %d sectors have a placed volume"), *MapId.ToString(), Map.Sectors.Num());
    }
    return bOk;
}

bool AHavocLevelDirector::StartMission()
{
    UWorld* World = GetWorld();
    UHavocMissionSubsystem* Missions = World ? World->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    if (!Missions)
    {
        UE_LOG(LogHavocWorld, Error, TEXT("No UHavocMissionSubsystem; mission '%s' not started"), *MissionId.ToString());
        return false;
    }
    const bool bStarted = Missions->StartMission(MissionId);
    if (!bStarted)
    {
        UE_LOG(LogHavocWorld, Error, TEXT("Mission '%s' failed to start"), *MissionId.ToString());
    }
    return bStarted;
}
