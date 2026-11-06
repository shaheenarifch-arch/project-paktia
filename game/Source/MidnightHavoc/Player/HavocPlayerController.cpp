#include "Player/HavocPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Input/HavocPlayInput.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Missions/Episode04/HavocEpisode04Flow.h"
#include "Missions/HavocEpisodeFlow.h"
#include "Player/HavocPlayerCharacter.h"
#include "TimerManager.h"
#include "UI/HavocMenuWidget.h"
#include "UI/HavocPlayHUD.h"
#include "Vehicles/HavocVehicleBase.h"

AHavocPlayerController::AHavocPlayerController()
{
    MenuClass = UHavocMenuWidget::StaticClass();
    HudClass = UHavocPlayHUD::StaticClass();
}

void AHavocPlayerController::BeginPlay()
{
    Super::BeginPlay();
    EnsureMetaContext();
    if (IsLocalController() && FParse::Param(FCommandLine::Get(), TEXT("HavocSmokeStart")))
    {
        GetWorldTimerManager().SetTimer(SmokeStartTimer, this, &AHavocPlayerController::StartSmokeRun, 3.f, false);
    }
}

void AHavocPlayerController::StartSmokeRun()
{
    IHavocEpisodeFlow* F = GetFlow();
    if (!F)
    {
        UE_LOG(LogTemp, Warning, TEXT("HavocSmoke: no Episode 04 flow in this level; cannot start the episode"));
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("HavocSmoke: starting Episode 04"));
    F->StartEpisode();
    // 30 s in: the Raven is over the beaches, so the island, sea and sky should all be in view.
    // Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/taking-screenshots-in-unreal-engine
    GetWorldTimerManager().SetTimer(SmokeShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        UE_LOG(LogTemp, Display, TEXT("HavocSmoke: taking screenshot"));
        ConsoleCommand(TEXT("HighResShot 1280x720"));
    }), 30.f, false);
}

void AHavocPlayerController::EnsureMetaContext()
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (const UHavocPlayInput* Input = GI->GetSubsystem<UHavocPlayInput>())
        {
            Input->EnsureMeta(this);
        }
    }
}

void AHavocPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (AHavocPlayerCharacter* AsMarine = Cast<AHavocPlayerCharacter>(InPawn))
    {
        Marine = AsMarine;
    }
    // The Falcon clears all mappings when it takes input; put the meta keys (pause, return) back.
    GetWorldTimerManager().SetTimerForNextTick(this, &AHavocPlayerController::EnsureMetaContext);
}

void AHavocPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
    const UGameInstance* GI = GetGameInstance();
    const UHavocPlayInput* Input = GI ? GI->GetSubsystem<UHavocPlayInput>() : nullptr;
    if (!Enhanced || !Input)
    {
        UE_LOG(LogTemp, Error, TEXT("AHavocPlayerController: Enhanced Input is not the default input component. Set DefaultInputComponentClass in DefaultInput.ini."));
        return;
    }
    Enhanced->BindAction(Input->PauseAction, ETriggerEvent::Started, this, &AHavocPlayerController::TogglePause);
    Enhanced->BindAction(Input->ChooseNorthAction, ETriggerEvent::Started, this, &AHavocPlayerController::OnChooseNorth);
    Enhanced->BindAction(Input->ChooseWestAction, ETriggerEvent::Started, this, &AHavocPlayerController::OnChooseWest);
    Enhanced->BindAction(Input->InteractAction, ETriggerEvent::Started, this, &AHavocPlayerController::OnInteract);
    Enhanced->BindAction(Input->DeployRavenAction, ETriggerEvent::Started, this, &AHavocPlayerController::OnDeployRaven);
    Enhanced->BindAction(Input->CallFalconAction, ETriggerEvent::Started, this, &AHavocPlayerController::OnCallFalcon);
    Enhanced->BindAction(Input->ReturnAction, ETriggerEvent::Started, this, &AHavocPlayerController::OnReturn);
    Enhanced->BindAction(Input->VehicleAction, ETriggerEvent::Started, this, &AHavocPlayerController::OnVehicle);
}

IHavocEpisodeFlow* AHavocPlayerController::GetFlow()
{
    if (!Flow.GetObject())
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (It->Implements<UHavocEpisodeFlow>())
            {
                Flow = *It;
                break;
            }
        }
    }
    return Flow.GetInterface();
}

AHavocEpisode04Flow* AHavocPlayerController::GetEpisode04Flow()
{
    GetFlow();
    return Cast<AHavocEpisode04Flow>(Flow.GetObject());
}

void AHavocPlayerController::OnChooseNorth() { if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->ChooseBeach(true); } }
void AHavocPlayerController::OnChooseWest() { if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->ChooseBeach(false); } }
void AHavocPlayerController::OnInteract() { if (IHavocEpisodeFlow* F = GetFlow()) { F->HandleInteract(); } }
void AHavocPlayerController::OnDeployRaven() { if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->DeployRaven(); } }
void AHavocPlayerController::OnCallFalcon() { if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->CallFalcon(); } }
void AHavocPlayerController::OnReturn() { if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->ReturnToMarine(); } }

void AHavocPlayerController::OnVehicle() { ServerToggleVehicle(); }

void AHavocPlayerController::ServerToggleVehicle_Implementation()
{
    if (IsMenuOpen())
    {
        return;
    }
    if (AHavocVehicleBase* Current = Cast<AHavocVehicleBase>(GetPawn()))
    {
        Current->Exit();
    }
    else if (AHavocVehicleBase* Near = AHavocVehicleBase::FindEnterable(this))
    {
        // Only the on-foot Marine may get in; the Raven and Falcon are mission pawns.
        if (Cast<AHavocPlayerCharacter>(GetPawn()))
        {
            Near->Enter(this);
        }
    }
}

bool AHavocPlayerController::IsMenuOpen() const
{
    return Menu && Menu->IsInViewport();
}

void AHavocPlayerController::ShowMenu(EHavocMenuMode Mode)
{
    if (!Menu)
    {
        Menu = CreateWidget<UHavocMenuWidget>(this, MenuClass ? MenuClass.Get() : UHavocMenuWidget::StaticClass());
    }
    if (!Menu)
    {
        return;
    }
    Menu->SetMode(Mode);
    if (!Menu->IsInViewport())
    {
        Menu->AddToViewport(20);
    }
    if (Mode == EHavocMenuMode::Pause)
    {
        UGameplayStatics::SetGamePaused(this, true);
    }
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(Menu->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    bShowMouseCursor = true;
    // The input mode focuses the menu root; move focus onto a button so arrows / D-pad and Enter / A work.
    Menu->FocusDefault();
}

void AHavocPlayerController::HideMenu()
{
    if (Menu && Menu->IsInViewport())
    {
        Menu->RemoveFromParent();
    }
    UGameplayStatics::SetGamePaused(this, false);
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;
}

void AHavocPlayerController::ShowHud()
{
    if (!Hud)
    {
        Hud = CreateWidget<UHavocPlayHUD>(this, HudClass ? HudClass.Get() : UHavocPlayHUD::StaticClass());
    }
    if (Hud && !Hud->IsInViewport())
    {
        Hud->AddToViewport(0);
    }
}

void AHavocPlayerController::TogglePause()
{
    IHavocEpisodeFlow* F = GetFlow();
    if (!F || F->GetPhase() != EHavocFlowPhase::Playing)
    {
        return;
    }
    if (IsMenuOpen())
    {
        HideMenu();
    }
    else
    {
        ShowMenu(EHavocMenuMode::Pause);
    }
}
