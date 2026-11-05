#pragma once

// Player settings for the playable loop. Saved to GameUserSettings.ini.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/configuration-files-in-unreal-engine

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HavocPlaySettings.generated.h"

UCLASS(Config = GameUserSettings)
class MIDNIGHTHAVOC_API UHavocPlaySettings : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Accessibility: skips the large explosion effect and camera shake (docs/vfx-plan.md). */
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings")
    bool bReduceFlashes = false;

    /** Multiplier on mouse look. */
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings")
    float LookSensitivity = 1.0f;

    /** 0..1 master volume. */
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings")
    float MasterVolume = 1.0f;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Settings")
    void ApplyAndSave();
};
