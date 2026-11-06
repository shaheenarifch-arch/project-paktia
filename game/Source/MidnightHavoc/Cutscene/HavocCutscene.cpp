#include "Cutscene/HavocCutscene.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Player/HavocPlayerController.h"

DEFINE_LOG_CATEGORY(LogHavocCutscene);

namespace
{
    /** Keys ignored for the first moments so the click or Enter that started the episode is not read as "skip". */
    constexpr float SkipGraceSeconds = 0.6f;
}

AHavocCutscene::AHavocCutscene()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Root);
    Camera->FieldOfView = 70.f;
}

void AHavocCutscene::Begin(AHavocPlayerController* InPlayer, const FString& InLabel, const TArray<FHavocCutsceneShot>& InShots,
                           TFunction<void(bool)> InOnDone)
{
    Player = InPlayer;
    Label = InLabel;
    Shots = InShots;
    OnDone = MoveTemp(InOnDone);
    Elapsed = 0.f;
    Subtitle.Empty();

    if (!Player || Shots.Num() == 0)
    {
        // Nothing to show (no player, or no shots): report it so CI sees the gap, and carry on with the game.
        UE_LOG(LogHavocCutscene, Warning, TEXT("HavocCutscene: %s missing"), *Label);
        Finish(true);
        return;
    }

    bPlaying = true;
    SetActorTickEnabled(true);
    Player->SetCinematicMode(true, false, false, true, true);
    Player->SetViewTargetWithBlend(this, 0.5f);
    UE_LOG(LogHavocCutscene, Display, TEXT("HavocCutscene: %s started"), *Label);
}

void AHavocCutscene::Skip()
{
    if (bPlaying)
    {
        Finish(true);
    }
}

void AHavocCutscene::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPlaying)
    {
        return;
    }
    Elapsed += DeltaSeconds;

    if (Elapsed > SkipGraceSeconds && Player &&
        (Player->WasInputKeyJustPressed(EKeys::SpaceBar) || Player->WasInputKeyJustPressed(EKeys::Enter) ||
         Player->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom)))
    {
        Finish(true);
        return;
    }

    float Alpha = 0.f;
    const int32 Index = HavocCutsceneMath::ShotAt(Shots, Elapsed, Alpha);
    if (Index == INDEX_NONE)
    {
        Finish(false);
        return;
    }
    const FHavocCutsceneShot& Shot = Shots[Index];
    const FVector Location = HavocCutsceneMath::CameraLocation(Shot, Alpha);
    SetActorLocationAndRotation(Location, HavocCutsceneMath::CameraRotation(Shot, Location));
    Subtitle = Shot.Subtitle;
}

void AHavocCutscene::Finish(bool bSkipped)
{
    const bool bWasPlaying = bPlaying;
    bPlaying = false;
    SetActorTickEnabled(false);
    Subtitle.Empty();
    if (bWasPlaying && Player)
    {
        Player->SetCinematicMode(false, false, false, true, true);
        // Hand the camera back to whatever the player controls (the Marine at the start and end of an episode).
        if (APawn* Pawn = Player->GetPawn())
        {
            Player->SetViewTarget(Pawn);
        }
        UE_LOG(LogHavocCutscene, Display, TEXT("HavocCutscene: %s %s"), *Label, bSkipped ? TEXT("skipped") : TEXT("finished"));
    }
    TFunction<void(bool)> Done = MoveTemp(OnDone);
    OnDone = nullptr;
    if (Done)
    {
        Done(bSkipped);
    }
}
