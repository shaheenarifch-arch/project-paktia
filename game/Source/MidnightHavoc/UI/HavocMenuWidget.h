#pragma once

// Main menu (Play Episode 04, Settings, Quit), pause menu (Resume, Restart from checkpoint, Settings,
// Quit) and results screen (outcome, stats, Restart / Play again, Quit), all built in C++.
// Settings: reduce flashes and camera shake (docs/vfx-plan.md accessibility), look sensitivity and
// master volume, saved by UHavocPlaySettings.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/HavocUITypes.h"
#include "HavocMenuWidget.generated.h"

class UBorder;
class UVerticalBox;

UCLASS()
class MIDNIGHTHAVOC_API UHavocMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Havoc|UI")
    void SetMode(EHavocMenuMode NewMode);

    EHavocMenuMode GetMode() const { return Mode; }

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION() void OnPlay();
    UFUNCTION() void OnResume();
    UFUNCTION() void OnRestartCheckpoint();
    UFUNCTION() void OnSettings();
    UFUNCTION() void OnBack();
    UFUNCTION() void OnQuit();
    UFUNCTION() void OnToggleFlashes();
    UFUNCTION() void OnSensitivityDown();
    UFUNCTION() void OnSensitivityUp();
    UFUNCTION() void OnVolumeDown();
    UFUNCTION() void OnVolumeUp();

private:
    void Rebuild();
    void AddLabel(const FString& Text, int32 Size, const FLinearColor& Color);
    void AddButton(const FString& Label, FName Handler);

    class UHavocPlaySettings* GetSettings() const;
    class AHavocEpisode04Flow* GetFlow() const;

    EHavocMenuMode Mode = EHavocMenuMode::Main;
    EHavocMenuMode ReturnMode = EHavocMenuMode::Main;

    UPROPERTY() TObjectPtr<UBorder> Backdrop;
    UPROPERTY() TObjectPtr<UVerticalBox> List;
};
