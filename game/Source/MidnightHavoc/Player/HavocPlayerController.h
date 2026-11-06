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

class AHavocEpisodeFlowBase;
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

    /** The active episode flow (Episode 04 until the Campaign menu picks another). Null if none exists in the level. */
    AHavocEpisodeFlowBase* GetFlow();

    /** Makes the flow of this class the active one, spawning it if the level has none. Returns it. */
    AHavocEpisodeFlowBase* SelectFlow(TSubclassOf<AHavocEpisodeFlowBase> FlowClass);

    /** The Marine, even while the controller possesses the Raven or Falcon. Null before it exists. */
    AHavocPlayerCharacter* GetMarine() const { return Marine.Get(); }

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;

private:
    void OnChooseNorth();
    void OnChooseWest();
    void OnInteract();
    void OnDeployRaven();
    void OnCallFalcon();
    void OnReturn();
    void OnVehicle();
    void EnsureMetaContext();

    /** Smoke test only (-HavocSmokeStart): start Episode 04 without the menu, then take a screenshot for
     *  game/scripts/smoke-run.ps1 to check the world is visible. See game/docs/smoke-test.md. */
    void StartSmokeRun();
    FTimerHandle SmokeStartTimer;
    FTimerHandle SmokeShotTimer;

    /** F on foot enters the nearest vehicle in range, F in a vehicle exits (state changes are server only). */
    UFUNCTION(Server, Reliable)
    void ServerToggleVehicle();

    UPROPERTY() TObjectPtr<UHavocMenuWidget> Menu;
    UPROPERTY() TObjectPtr<UHavocPlayHUD> Hud;
    UPROPERTY() TObjectPtr<AHavocEpisodeFlowBase> Flow;
    TWeakObjectPtr<AHavocPlayerCharacter> Marine;
};
