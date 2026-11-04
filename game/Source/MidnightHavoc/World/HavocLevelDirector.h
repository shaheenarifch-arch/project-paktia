#pragma once

// One per level. On BeginPlay it checks the placed sector volumes against the map
// definition (data/maps/<MapId>.json) and starts the mission.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HavocLevelDirector.generated.h"

UCLASS(Blueprintable)
class MIDNIGHTHAVOC_API AHavocLevelDirector : public AActor
{
    GENERATED_BODY()

public:
    AHavocLevelDirector();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Level",
        meta = (ToolTip = "Map id from data/maps (default kharg_hormuz)."))
    FName MapId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Level",
        meta = (ToolTip = "Mission id from data/missions (default episode_04_open_strait)."))
    FName MissionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|Level",
        meta = (ToolTip = "If true, the mission starts automatically when play begins."))
    bool bStartMissionOnBeginPlay = true;

    /** Checks every sector in the map definition has exactly one placed AHavocSectorVolume. Logs each problem. Returns true if all is well. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Level",
        meta = (ToolTip = "Compare placed sector volumes with the map JSON. Logs missing, unknown, duplicate and unnamed volumes. Returns true if none."))
    bool ValidateSectors();

    /** Starts MissionId through UHavocMissionSubsystem. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Level",
        meta = (ToolTip = "Start the configured mission now. Returns false if it cannot start."))
    bool StartMission();

protected:
    virtual void BeginPlay() override;
};
