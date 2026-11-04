#pragma once

// Test helper for PaktiaMissionTests.cpp: a stand-in IPaktiaMissionSystems provider with one
// counter that is captured at checkpoints. Not used outside tests.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Missions/PaktiaMissionJson.h"
#include "Missions/PaktiaMissionSystems.h"
#include "PaktiaTestSystems.generated.h"

UCLASS()
class PROJECTPAKTIA_API UPaktiaTestSystems : public UObject, public IPaktiaMissionSystems
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
        PaktiaMissionJson::FromString(SnapshotJson)->TryGetNumberField(TEXT("counter"), Value);
        Counter = Value;
    }
};
