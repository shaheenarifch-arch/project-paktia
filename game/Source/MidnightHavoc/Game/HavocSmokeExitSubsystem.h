// Ends the game cleanly after N seconds when launched with -HavocSmokeSeconds=N.
// Used by the CI smoke test (game/scripts/smoke-run.ps1): a clean exit flushes the -csvprofile CSV
// (FPS data) and the log, which a forced kill does not. Does nothing without the flag.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HavocSmokeExitSubsystem.generated.h"

UCLASS()
class MIDNIGHTHAVOC_API UHavocSmokeExitSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    FTSTicker::FDelegateHandle ExitHandle;
};
