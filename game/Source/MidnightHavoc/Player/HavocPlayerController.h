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

class AHavocEpisode04Flow;
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

    AHavocEpisode04Flow* GetFlow();

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
    void EnsureMetaContext();

    UPROPERTY() TObjectPtr<UHavocMenuWidget> Menu;
    UPROPERTY() TObjectPtr<UHavocPlayHUD> Hud;
    UPROPERTY() TObjectPtr<AHavocEpisode04Flow> Flow;
    TWeakObjectPtr<AHavocPlayerCharacter> Marine;
};
