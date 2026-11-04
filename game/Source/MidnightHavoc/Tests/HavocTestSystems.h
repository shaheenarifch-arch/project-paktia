#pragma once

// Test helper for HavocMissionTests.cpp: a stand-in IHavocMissionSystems provider with one
// counter that is captured at checkpoints. Not used outside tests.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionSystems.h"
#include "HavocTestSystems.generated.h"

UCLASS()
class MIDNIGHTHAVOC_API UHavocTestSystems : public UObject, public IHavocMissionSystems
{
    GENERATED_BODY()

public:
    int32 Counter = 0;

    virtual FString SnapshotSystems() const override
    {
        return FString::Printf(TEXT("{\"counter\":%d}"), Counter);
    }

    virtual void RestoreSystems(const FString& SnapshotJson) override
    {
        int32 Value = 0;
        HavocMissionJson::FromString(SnapshotJson)->TryGetNumberField(TEXT("counter"), Value);
        Counter = Value;
    }
};
