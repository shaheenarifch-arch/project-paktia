#include "World/HavocLevelDirector.h"
#include "World/HavocSectorVolume.h"
#include "World/HavocWorldLog.h"
#include "AI/HavocSquadSubsystem.h"
#include "Core/HavocEventSubsystem.h"
#include "Data/HavocDataSubsystem.h"
#include "Data/HavocMapDef.h"
#include "Missions/HavocMissionJson.h"
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

    ResolveMissionId();

    const bool bSectorsOk = ValidateSectors();
    if (!bSectorsOk)
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Map '%s': sector check found problems (see above)"), *MapId.ToString());
    }

    // Subscribe before the mission starts so the first state is seen too.
    if (const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr)
    {
        if (UHavocEventSubsystem* Events = GameInstance->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Subscribe(TEXT("mission:state"), this, GET_FUNCTION_NAME_CHECKED(AHavocLevelDirector, OnMissionState));
        }
    }

    if (bStartMissionOnBeginPlay)
    {
        StartMission();
    }
}

void AHavocLevelDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (const UWorld* World = GetWorld())
    {
        if (const UGameInstance* GameInstance = World->GetGameInstance())
        {
            if (UHavocEventSubsystem* Events = GameInstance->GetSubsystem<UHavocEventSubsystem>())
            {
                Events->Unsubscribe(TEXT("mission:state"), this, GET_FUNCTION_NAME_CHECKED(AHavocLevelDirector, OnMissionState));
            }
        }
    }
    Super::EndPlay(EndPlayReason);
}

void AHavocLevelDirector::OnMissionState(FName EventName, const FString& Payload)
{
    const TSharedRef<FJsonObject> Json = HavocMissionJson::FromString(Payload);
    FString MissionField, To;
    if (!Json->TryGetStringField(TEXT("to"), To))
    {
        return;
    }
    if (Json->TryGetStringField(TEXT("missionId"), MissionField) && MissionField != MissionId.ToString())
    {
        return; // another mission's state
    }
    Phase = To;
    if (UWorld* World = GetWorld())
    {
        if (UHavocSquadSubsystem* Squads = World->GetSubsystem<UHavocSquadSubsystem>())
        {
            Squads->ApplyPhase(Phase);
        }
    }
}

void AHavocLevelDirector::ResolveMissionId()
{
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UHavocDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    FHavocMissionDef Def;
    if (!Data || Data->GetMission(MissionId, Def))
    {
        return;
    }
    // Levels built earlier name the mission "episode_NN_..." (Episode 01's file id is "mission_01_mountain_shield").
    // Find it by episode number instead, so those levels keep working without a rebuild.
    FString Digits = MissionId.ToString();
    if (Digits.RemoveFromStart(TEXT("episode_")) && Digits.Len() >= 2 && Digits.Left(2).IsNumeric()
        && Data->FindMissionByEpisode(FCString::Atoi(*Digits.Left(2)), Def))
    {
        UE_LOG(LogHavocWorld, Log, TEXT("Mission '%s' not in data/missions; using '%s' (episode %d)"), *MissionId.ToString(), *Def.Id, Def.Episode);
        MissionId = FName(*Def.Id);
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
