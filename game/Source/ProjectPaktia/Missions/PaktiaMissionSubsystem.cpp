#include "PaktiaMissionSubsystem.h"

#include "Data/PaktiaDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

bool UPaktiaMissionSubsystem::GetMissionDef(FName Id, FPaktiaMissionDef& OutMission) const
{
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UPaktiaDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UPaktiaDataSubsystem>() : nullptr;
    return Data && Data->GetMission(Id, OutMission);
}
