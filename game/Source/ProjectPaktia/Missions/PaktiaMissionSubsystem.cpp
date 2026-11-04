#include "PaktiaMissionSubsystem.h"

#include "Core/PaktiaEventSubsystem.h"
#include "Data/PaktiaDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Missions/PaktiaMissionRunner.h"
#include "Missions/Episode04/PaktiaOpenStraitMission.h"

bool UPaktiaMissionSubsystem::GetMissionDef(FName Id, FPaktiaMissionDef& OutMission) const
{
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UPaktiaDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UPaktiaDataSubsystem>() : nullptr;
    return Data && Data->GetMission(Id, OutMission);
}

bool UPaktiaMissionSubsystem::StartMission(FName MissionId)
{
    FPaktiaMissionDef Def;
    if (!GetMissionDef(MissionId, Def))
    {
        UE_LOG(LogPaktiaMission, Error, TEXT("StartMission: unknown mission '%s'."), *MissionId.ToString());
        return false;
    }

    const UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    UPaktiaEventSubsystem* Events = GameInstance ? GameInstance->GetSubsystem<UPaktiaEventSubsystem>() : nullptr;

    ActiveRunner = nullptr;
    ActiveOpenStrait = nullptr;

    FString Error;
    if (Def.Id == TEXT("episode_04_open_strait"))
    {
        UPaktiaOpenStraitMission* Mission = NewObject<UPaktiaOpenStraitMission>(this);
        if (!Mission->Initialize(Def, Episode3Outcome, Events, Error))
        {
            return false;
        }
        ActiveOpenStrait = Mission;
        ActiveRunner = Mission->GetRunner();
        return true;
    }

    UPaktiaMissionRunner* Runner = NewObject<UPaktiaMissionRunner>(this);
    if (!Runner->Initialize(Def, TMap<FString, FPaktiaEffectHandler>(), Events, nullptr, Error))
    {
        return false;
    }
    ActiveRunner = Runner;
    return true;
}

FString UPaktiaMissionSubsystem::Advance()
{
    return ActiveRunner ? ActiveRunner->Advance() : FString();
}

FString UPaktiaMissionSubsystem::Choose(const FString& Branch)
{
    return ActiveRunner ? ActiveRunner->Choose(Branch) : FString();
}

FString UPaktiaMissionSubsystem::Fail()
{
    return ActiveRunner ? ActiveRunner->Fail() : FString();
}

bool UPaktiaMissionSubsystem::RestoreCheckpoint()
{
    return ActiveRunner && ActiveRunner->RestoreCheckpoint();
}

FString UPaktiaMissionSubsystem::GetObjective() const
{
    return ActiveRunner ? ActiveRunner->GetObjective() : FString();
}

TArray<FString> UPaktiaMissionSubsystem::GetChoices() const
{
    return ActiveRunner ? ActiveRunner->GetChoices() : TArray<FString>();
}
