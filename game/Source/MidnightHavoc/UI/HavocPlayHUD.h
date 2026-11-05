#pragma once

// The in-game HUD for the playable loop, built in C++ (no widget assets needed). It extends
// UHavocTacticalHUD, so a designer can still make a Blueprint child (game/docs/hud-guide.md) and use
// the same events. It shows: objective, hint, toast, health, ammo, reinforcements, terminal and
// supply status, drone link and battery while flying the Raven, and Falcon support status.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine

#include "CoreMinimal.h"
#include "TacticalUI/HavocTacticalHUD.h"
#include "HavocPlayHUD.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class MIDNIGHTHAVOC_API UHavocPlayHUD : public UHavocTacticalHUD
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY() TObjectPtr<UTextBlock> ObjectiveText;
    UPROPERTY() TObjectPtr<UTextBlock> HintText;
    UPROPERTY() TObjectPtr<UTextBlock> ToastText;
    UPROPERTY() TObjectPtr<UTextBlock> HealthText;
    UPROPERTY() TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY() TObjectPtr<UTextBlock> AmmoText;
    UPROPERTY() TObjectPtr<UTextBlock> StatusText;
    UPROPERTY() TObjectPtr<UTextBlock> LinkText;
    UPROPERTY() TObjectPtr<UTextBlock> Crosshair;
};
