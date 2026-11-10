#pragma once

// Main menu (Campaign, How to play, Settings, Quit), campaign select (Episodes 01-04, Start), pause menu (Resume, Restart from checkpoint, How to play, Settings,
// Quit) and results screen (outcome, stats, Restart / Play again, Quit), all built in C++.
// Settings: graphics quality, subtitles, reduce flashes and camera shake (docs/vfx-plan.md
// accessibility), look sensitivity, master volume and key bindings, each with a help line, saved by
// UHavocPlaySettings. Works by keyboard (arrows, Enter, Esc) and gamepad (D-pad or stick, A, B).
// See game/docs/ui.md.
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

    /** Puts keyboard / gamepad focus on the first button. Call after the menu is added to the viewport and the input mode is set. */
    void FocusDefault() { PendingFocus = 0; FocusButton(0); }

    EHavocMenuMode GetMode() const { return Mode; }

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION() void OnPlay();
    /** Campaign "Start Episode N": opens that episode's level and starts its flow (AHavocPlayerController::PlayEpisode). */
    UFUNCTION() void OnPlayEpisode1() { PlayEpisode(1); }
    UFUNCTION() void OnPlayEpisode2() { PlayEpisode(2); }
    UFUNCTION() void OnPlayEpisode3() { PlayEpisode(3); }
    UFUNCTION() void OnPlayEpisode4() { PlayEpisode(4); }
    void PlayEpisode(int32 Episode);
    UFUNCTION() void OnCampaign();
    UFUNCTION() void OnUnlockAll();
    UFUNCTION() void OnMainMenu();
    UFUNCTION() void OnResume();
    UFUNCTION() void OnRestartCheckpoint();
    UFUNCTION() void OnSettings();
    UFUNCTION() void OnHowToPlay();
    UFUNCTION() void OnBack();
    UFUNCTION() void OnQuit();
    UFUNCTION() void OnToggleFlashes();
    UFUNCTION() void OnSensitivityDown();
    UFUNCTION() void OnSensitivityUp();
    UFUNCTION() void OnVolumeDown();
    UFUNCTION() void OnVolumeUp();
    UFUNCTION() void OnCycleQuality();
    UFUNCTION() void OnToggleSubtitles();
    UFUNCTION() void OnResetSettings();
    UFUNCTION() void OnRebindInteract();
    UFUNCTION() void OnRebindReload();
    UFUNCTION() void OnRebindRaven();
    UFUNCTION() void OnRebindFalcon();
    UFUNCTION() void OnRebindReturn();

private:
    void Rebuild();
    void AddLabel(const FString& Text, int32 Size, const FLinearColor& Color);
    void AddButton(const FString& Label, FName Handler);
    void BeginRebind(FName ActionId);
    void FinishRebind(const FKey& Key);
    FKey* BindingFor(class UHavocPlaySettings& Settings, FName ActionId) const;
    void FocusButton(int32 Index);

    class UHavocPlaySettings* GetSettings() const;
    class IHavocEpisodeFlow* GetFlow() const;
    class UHavocCampaign* GetCampaign() const;
    /** The Episode 03 flow when this session plays Episode 03, else null. */
    class AHavocEpisode03Flow* GetFlow03() const;

    EHavocMenuMode Mode = EHavocMenuMode::Main;
    EHavocMenuMode ReturnMode = EHavocMenuMode::Main;

    /** Set while the next key press becomes the new binding for this action ("interact", "reload", ...). */
    FName RebindingAction;
    FString RebindMessage;
    int32 PendingFocus = 0;
    bool bResetFocus = true;

    UPROPERTY() TArray<TObjectPtr<class UButton>> Buttons;
    UPROPERTY() TObjectPtr<UBorder> Backdrop;
    UPROPERTY() TObjectPtr<UVerticalBox> List;
    UPROPERTY() TObjectPtr<class UScrollBox> Scroll;
};
