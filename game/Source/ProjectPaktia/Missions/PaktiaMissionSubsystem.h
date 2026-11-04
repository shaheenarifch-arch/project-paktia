#pragma once

// Ports: src/gameplay/missions/framework/MissionRunner.ts
// Stub: mission start/stop and objective progression will be ported here.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/PaktiaMissionDef.h"
#include "PaktiaMissionSubsystem.generated.h"

UCLASS()
class PROJECTPAKTIA_API UPaktiaMissionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** Looks a mission definition up in UPaktiaDataSubsystem. Returns false if unknown. */
    UFUNCTION(BlueprintCallable, Category = "Paktia|Missions")
    bool GetMissionDef(FName Id, FPaktiaMissionDef& OutMission) const;
};
