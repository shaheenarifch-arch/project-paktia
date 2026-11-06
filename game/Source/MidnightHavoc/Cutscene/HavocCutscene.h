#pragma once

// A short skippable camera fly-over with subtitles: the intro and outro of an episode. Greybox
// version: a camera orbiting a few points (see HavocCutsceneMath.h), driven from code so it needs no
// editor assets. To use a hand-made Level Sequence instead, build it in Sequencer and trigger it from
// a Blueprint child of the episode flow; the skip key and subtitle line work the same way.
// The player skips with Space, Enter or the gamepad bottom face button; subtitles follow the
// Subtitles setting (the HUD reads GetSubtitle()). The log lines "HavocCutscene: <episode> <intro|outro>
// started|finished|skipped" are what game/scripts/smoke-episodes.ps1 checks in CI.
// Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/cinematics-and-movie-making-in-unreal-engine

#include "CoreMinimal.h"
#include "Cutscene/HavocCutsceneMath.h"
#include "GameFramework/Actor.h"
#include "HavocCutscene.generated.h"

class AHavocPlayerController;
class UCameraComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogHavocCutscene, Log, All);

UCLASS()
class MIDNIGHTHAVOC_API AHavocCutscene : public AActor
{
    GENERATED_BODY()

public:
    AHavocCutscene();

    /** Starts the shots; OnDone(bSkipped) runs once when they end or are skipped. Label e.g. "04 intro". */
    void Begin(AHavocPlayerController* InPlayer, const FString& InLabel, const TArray<FHavocCutsceneShot>& InShots,
               TFunction<void(bool)> InOnDone);
    void Skip();
    bool IsPlaying() const { return bPlaying; }
    const FString& GetSubtitle() const { return Subtitle; }

    virtual void Tick(float DeltaSeconds) override;

private:
    void Finish(bool bSkipped);

    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<AHavocPlayerController> Player;

    TArray<FHavocCutsceneShot> Shots;
    TFunction<void(bool)> OnDone;
    FString Label;
    FString Subtitle;
    float Elapsed = 0.f;
    bool bPlaying = false;
};
