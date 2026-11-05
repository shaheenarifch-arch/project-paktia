#pragma once

// Player settings for the playable loop. Saved to GameUserSettings.ini.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/configuration-files-in-unreal-engine

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
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

    /** 0 Low, 1 Medium, 2 High, 3 Epic. Applied through UGameUserSettings::SetOverallScalabilityLevel. */
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings")
    int32 GraphicsQuality = 2;

    /** Shows the radio / mission text (toasts and hints) as subtitles. */
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings")
    bool bSubtitles = true;

    /** Rebindable keys (keyboard). Defaults match game/README.md "How to play". */
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings") FKey InteractKey = EKeys::E;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings") FKey ReloadKey = EKeys::R;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings") FKey RavenKey = EKeys::T;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings") FKey FalconKey = EKeys::G;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Havoc|Settings") FKey ReturnKey = EKeys::Tab;

    UFUNCTION(BlueprintCallable, Category = "Havoc|Settings")
    void ApplyAndSave();

    /** Display name and one-line help for each setting id (used by the menu). */
    static FString HelpFor(FName SettingId);
    static const TCHAR* QualityName(int32 Level);

    /** Restores every setting to its default and saves. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|Settings")
    void ResetToDefaults();
};
