#pragma once

// Ports the `systems` snapshot provider of src/gameplay/missions/framework/MissionRunner.ts
// ({ snapshot(), restore(data) }). A mission's own systems (terminal, lanes, convoy, ...)
// are captured as a JSON object string at every checkpoint and handed back on restore.

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HavocMissionSystems.generated.h"

UINTERFACE(MinimalAPI)
class UHavocMissionSystems : public UInterface
{
    GENERATED_BODY()
};

class MIDNIGHTHAVOC_API IHavocMissionSystems
{
    GENERATED_BODY()

public:
    /** JSON object (as a string) describing the current state of the mission's systems. */
    virtual FString SnapshotSystems() const = 0;

    /** Restores state captured by SnapshotSystems. */
    virtual void RestoreSystems(const FString& SnapshotJson) = 0;
};
