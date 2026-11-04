#include "HavocMissionSubsystem.h"

#include "Core/HavocEventSubsystem.h"
#include "Data/HavocDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"

bool UHavocMissionSubsystem::GetMissionDef(FName Id, FHavocMissionDef& OutMission) const
{
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UHavocDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    return Data && Data->GetMission(Id, OutMission);
}

bool UHavocMissionSubsystem::StartMission(FName MissionId)
{
    FHavocMissionDef Def;
    if (!GetMissionDef(MissionId, Def))
    {
        UE_LOG(LogHavocMission, Error, TEXT("StartMission: unknown mission '%s'."), *MissionId.ToString());
        return false;
    }

    const UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    UHavocEventSubsystem* Events = GameInstance ? GameInstance->GetSubsystem<UHavocEventSubsystem>() : nullptr;

    ActiveRunner = nullptr;
    ActiveOpenStrait = nullptr;

    FString Error;
    if (Def.Id == TEXT("episode_04_open_strait"))
    {
        UHavocOpenStraitMission* Mission = NewObject<UHavocOpenStraitMission>(this);
        if (!Mission->Initialize(Def, Episode3Outcome, Events, Error))
        {
            return false;
        }
        ActiveOpenStrait = Mission;
        ActiveRunner = Mission->GetRunner();
        return true;
    }

    UHavocMissionRunner* Runner = NewObject<UHavocMissionRunner>(this);
    if (!Runner->Initialize(Def, TMap<FString, FHavocEffectHandler>(), Events, nullptr, Error))
    {
        return false;
    }
    ActiveRunner = Runner;
    return true;
}

FString UHavocMissionSubsystem::Advance()
{
    return ActiveRunner ? ActiveRunner->Advance() : FString();
}

FString UHavocMissionSubsystem::Choose(const FString& Branch)
{
    return ActiveRunner ? ActiveRunner->Choose(Branch) : FString();
}

FString UHavocMissionSubsystem::Fail()
{
    return ActiveRunner ? ActiveRunner->Fail() : FString();
}

bool UHavocMissionSubsystem::RestoreCheckpoint()
{
    return ActiveRunner && ActiveRunner->RestoreCheckpoint();
}

FString UHavocMissionSubsystem::GetObjective() const
{
    return ActiveRunner ? ActiveRunner->GetObjective() : FString();
}

TArray<FString> UHavocMissionSubsystem::GetChoices() const
{
    return ActiveRunner ? ActiveRunner->GetChoices() : TArray<FString>();
}
