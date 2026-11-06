#pragma once

// Player controller for the playable loop. Owns the always-on "meta" input (pause, beach choice,
// Raven / Falcon calls, interact, return to Marine), the main / pause / results menus and the HUD.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/player-controllers-in-unreal-engine
//       https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/HavocUITypes.h"
#include "Player/HavocPlayerCharacter.h"
#include "HavocPlayerController.generated.h"

class AHavocEpisode03Flow;
class AHavocEpisode04Flow;
class IHavocEpisodeFlow;
class UHavocMenuWidget;
class UHavocPlayHUD;

UCLASS()
class MIDNIGHTHAVOC_API AHavocPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    /** Widget classes; defaults are the C++ widgets, a Blueprint child may swap them. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Havoc|UI")
    TSubclassOf<UHavocMenuWidget> MenuClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Havoc|UI")
    TSubclassOf<UHavocPlayHUD> HudClass;

    AHavocPlayerController();

    UFUNCTION(BlueprintCallable, Category = "Havoc|UI")
    void ShowMenu(EHavocMenuMode Mode);

    UFUNCTION(BlueprintCallable, Category = "Havoc|UI")
    void HideMenu();

    UFUNCTION(BlueprintCallable, Category = "Havoc|UI")
    void ShowHud();

    UFUNCTION(BlueprintPure, Category = "Havoc|UI")
    bool IsMenuOpen() const;

    UFUNCTION(BlueprintCallable, Category = "Havoc|UI")
    void TogglePause();

    /** The active episode flow through the shared interface (null if the level has none). */
    IHavocEpisodeFlow* GetFlow();
    /** Episode 04 only: for its specific inputs (beach choice, Raven, Falcon). */
    AHavocEpisode04Flow* GetEpisode04Flow();

    /** The first-time hint on screen now (empty if none). Shown by the HUD until dismissed or it times out. */
    const FString& GetActiveHint() const { return ActiveHint; }

    /** Dismisses the first-time hint (H or gamepad X). */
    void DismissHint();

    /** Shows the hint for this kind of machine unless it was shown before (remembered in UHavocPlaySettings). */
    void ShowFirstTimeHint(EHavocHintKind Kind);

    /** The Episode 03 flow when the session plays Episode 03 (greybox Gardun), else null. */
    AHavocEpisode03Flow* GetFlow03();

    /** The Marine, even while the controller possesses the Raven or Falcon. Null before it exists. */
    AHavocPlayerCharacter* GetMarine() const { return Marine.Get(); }

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void AcknowledgePossession(APawn* P) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void OnChooseNorth();
    void OnChooseWest();
    void OnInteract();
    void OnDeployRaven();
    void OnCallFalcon();
    void OnReturn();
    void OnVehicle();
    void OnDismissHint() { DismissHint(); }
    void EnsureMetaContext();

    /** Smoke test only (-HavocSmokeStart): start Episode 04 without the menu, then take a screenshot for
     *  game/scripts/smoke-run.ps1 to check the world is visible. See game/docs/smoke-test.md. */
    void StartSmokeRun();
    FTimerHandle SmokeStartTimer;
    FTimerHandle SmokeShotTimer;

    /** F on foot enters the nearest vehicle in range, F in a vehicle exits (state changes are server only). */
    UFUNCTION(Server, Reliable)
    void ServerToggleVehicle();

    FString ActiveHint;
    float HintSecondsLeft = 0.f;

    UPROPERTY() TObjectPtr<UHavocMenuWidget> Menu;
    UPROPERTY() TObjectPtr<UHavocPlayHUD> Hud;
    UPROPERTY() TScriptInterface<IHavocEpisodeFlow> Flow;
    TWeakObjectPtr<AHavocPlayerCharacter> Marine;
};
