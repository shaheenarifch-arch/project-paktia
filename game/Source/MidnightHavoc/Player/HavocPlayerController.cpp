#include "Player/HavocPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Campaign/HavocCampaign.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Aircraft/HavocFalconHelicopter.h"
#include "Drones/HavocDrone.h"
#include "Input/HavocPlayInput.h"
#include "Player/HavocPlaySettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Missions/HavocEpisodeFlowBase.h"
#include "Missions/Episode01/HavocEpisode01Flow.h"
#include "Missions/Episode02/HavocEpisode02Flow.h"
#include "Missions/Episode03/HavocEpisode03Flow.h"
#include "Missions/Episode04/HavocEpisode04Flow.h"
#include "Missions/HavocEpisodeFlow.h"
#include "Player/HavocPlayerCharacter.h"
#include "TimerManager.h"
#include "UI/HavocMenuWidget.h"
#include "UI/HavocPlayHUD.h"
#include "Vehicles/HavocVehicleBase.h"
#include "World/HavocSectorVolume.h"

namespace
{
// Smoke test only: put the view over the episode's map (Kharg oil terminal, Paktia Highlands or Gardun sectors). The Raven at 30 s is not a reliable
// vantage (it hovers between the beaches and the screenshot showed only sky haze), so the picture must not depend on it.
// The island is centred on 0,0 in the level (build_kharg_level.py); the terminal sector is at (-170000, 120000) cm.
void FrameMapForSmoke(AHavocPlayerController* PC, int32 EpisodeNumber)
{
    UWorld* World = PC->GetWorld();
    if (!World)
    {
        return;
    }
    // 1. Find the terrain actor(s) and log everything that could explain "nothing at (0,0)".
    bool bFound = false;
    FBox TerrainBox(ForceInit);
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        AStaticMeshActor* A = *It;
        // KhargTerrain, PaktiaTerrain, GardunTerrain (the level builders' names).
        if (!A->GetName().Contains(TEXT("Terrain")))
        {
            continue;
        }
        const UStaticMeshComponent* Comp = A->GetStaticMeshComponent();
        FVector Origin, Extent;
        A->GetActorBounds(false, Origin, Extent);
        const UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
        const UEnum* CollEnum = StaticEnum<ECollisionEnabled::Type>();
        const ECollisionEnabled::Type Coll = Comp ? Comp->GetCollisionEnabled() : ECollisionEnabled::NoCollision;
        UE_LOG(LogTemp, Display, TEXT("HavocSmoke: terrain actor %s loc %s rot %s bounds origin %s extent %s mesh %s collision %s hidden %d"),
            *A->GetName(), *A->GetActorLocation().ToString(), *A->GetActorRotation().ToString(), *Origin.ToString(), *Extent.ToString(),
            Mesh ? *Mesh->GetPathName() : TEXT("(none)"), CollEnum ? *CollEnum->GetNameStringByValue(Coll) : TEXT("?"), A->IsHidden() ? 1 : 0);
        TerrainBox += FBox(Origin - Extent, Origin + Extent);
        bFound = true;
    }
    if (!bFound)
    {
        UE_LOG(LogTemp, Warning, TEXT("HavocSmoke: no AStaticMeshActor named *Terrain* in this world (missing or not streamed in)"));
    }

    // 2. Frame on the real terrain centre (0,0 only if nothing was found).
    const FVector Centre = bFound ? FVector(TerrainBox.GetCenter().X, TerrainBox.GetCenter().Y, 0.f) : FVector::ZeroVector;
    // Look-at point: Kharg = the oil terminal sector; Paktia / Gardun = the middle of the map's sector volumes
    // (they are where the episode happens), else along +X.
    FVector Terminal(-170000.f, 120000.f, 0.f);
    if (EpisodeNumber != 4)
    {
        FVector Sum = FVector::ZeroVector;
        int32 Count = 0;
        for (TActorIterator<AHavocSectorVolume> It(World); It; ++It)
        {
            Sum += It->GetActorLocation();
            ++Count;
        }
        Terminal = Count > 0 ? FVector(Sum.X / Count, Sum.Y / Count, 0.f) : Centre + FVector(100000.f, 0.f, 0.f);
        UE_LOG(LogTemp, Display, TEXT("HavocSmoke: Episode %02d framing toward %s (%d sector volumes)"), EpisodeNumber, *Terminal.ToString(), Count);
        if ((Terminal - Centre).SizeSquared2D() < 1.f)
        {
            Terminal = Centre + FVector(100000.f, 0.f, 0.f);
        }
    }
    float GroundZ = 0.f;
    FCollisionQueryParams QParams(SCENE_QUERY_STAT(HavocSmokeTrace), /*bTraceComplex*/ true);
    FHitResult Hit;
    if (World->LineTraceSingleByChannel(Hit, Centre + FVector(0.f, 0.f, 100000.f), Centre + FVector(0.f, 0.f, -100000.f), ECC_WorldStatic, QParams))
    {
        GroundZ = Hit.ImpactPoint.Z;
        UE_LOG(LogTemp, Display, TEXT("HavocSmoke: down-trace at %s hit %s at z %.0f"), *Centre.ToString(),
            Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("(no actor)"), GroundZ);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("HavocSmoke: down-trace at %s hit nothing"), *Centre.ToString());
        if (bFound)
        {
            GroundZ = TerrainBox.Max.Z;
        }
    }
    const FVector Eye = Centre + FVector(0.f, 0.f, FMath::Max(GroundZ, 0.f) + 10000.f); // 100 m above the ground
    // -12 deg pitch: island in the lower part, sea and sky above it. Steeper (-30) filled the frame with flat
    // placeholder sand, which reads as "one flat colour" to the check even though the world is rendering.
    const FRotator Look(-12.f, (Terminal - Centre).Rotation().Yaw, 0.f);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (ACameraActor* Cam = World->SpawnActor<ACameraActor>(Eye, Look, Params))
    {
        PC->SetViewTargetWithBlend(Cam, 0.f);
        UE_LOG(LogTemp, Display, TEXT("HavocSmoke: view framed on Episode %02d map (ground z %.0f, eye %s)"), EpisodeNumber, GroundZ, *Eye.ToString());
    }
}
}

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
    else if (IsLocalController())
    {
        // Opened by PlayEpisode: start the episode the travel URL names, once the level has settled.
        const AGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
        if (Mode && UGameplayStatics::HasOption(Mode->OptionsString, TEXT("AutoStart")))
        {
            const int32 Episode = UGameplayStatics::GetIntOption(Mode->OptionsString, TEXT("Episode"), 0);
            FTimerHandle AutoStart;
            GetWorldTimerManager().SetTimer(AutoStart, FTimerDelegate::CreateWeakLambda(this, [this, Episode]()
            {
                StartEpisodeFlow(Episode);
            }), 1.f, false);
        }
    }
}

bool AHavocPlayerController::OpenEpisodeLevel(int32 Episode, bool bAutoStart)
{
    const FString Path = UHavocCampaign::EpisodeMapPath(Episode);
    if (Path.IsEmpty())
    {
        return false;
    }
    if (UHavocCampaign::EpisodeMapName(Episode).Equals(UGameplayStatics::GetCurrentLevelName(this, true), ESearchCase::IgnoreCase))
    {
        return false; // already on it
    }
    UE_LOG(LogTemp, Display, TEXT("HavocPlay: opening %s for Episode %02d"), *Path, Episode);
    UGameplayStatics::OpenLevel(this, FName(*Path), true,
        FString::Printf(TEXT("Episode=%d%s"), Episode, bAutoStart ? TEXT("?AutoStart=1") : TEXT("")));
    return true;
}

void AHavocPlayerController::PlayEpisode(int32 Episode)
{
    if (Episode < UHavocCampaign::FirstEpisode || Episode > UHavocCampaign::LastEpisode)
    {
        return;
    }
    // Same level (e.g. Episode 02 after 01): no reload needed, just start the other flow.
    if (!OpenEpisodeLevel(Episode, /*bAutoStart*/ true))
    {
        StartEpisodeFlow(Episode);
    }
}

bool AHavocPlayerController::StartEpisodeFlow(int32 Episode)
{
    if (Episode == 3)
    {
        if (AHavocEpisode03Flow* F3 = GetFlow03())
        {
            F3->StartEpisode();
            return true;
        }
        return false;
    }
    if (UClass* FlowClass = UHavocCampaign::EpisodeFlowClass(Episode))
    {
        SelectFlow(FlowClass);
    }
    if (IHavocEpisodeFlow* F = GetFlow())
    {
        F->StartEpisode();
        return true;
    }
    return false;
}

void AHavocPlayerController::StartSmokeRun()
{
    // Which episode to smoke-test: -HavocEpisode=<n> or -HavocSmokeEpisode=<nn> (game/scripts/smoke-episodes.ps1).
    // Plain -HavocSmokeStart means Episode 04. See game/docs/smoke-test.md.
    int32 EpisodeNumber = 4;
    FParse::Value(FCommandLine::Get(), TEXT("HavocEpisode="), EpisodeNumber);
    FString EpisodeId;
    if (FParse::Value(FCommandLine::Get(), TEXT("HavocSmokeEpisode="), EpisodeId) && EpisodeId.IsNumeric())
    {
        EpisodeNumber = FCString::Atoi(*EpisodeId);
    }
    // The episode's own map first: if another one is loaded, open it (the command line still has
    // -HavocSmokeStart, so the new level's controller comes back here and carries on).
    if (OpenEpisodeLevel(EpisodeNumber, /*bAutoStart*/ false))
    {
        UE_LOG(LogTemp, Display, TEXT("HavocSmoke: Episode %02d is on another map; opening it first"), EpisodeNumber);
        return;
    }
    if (!StartEpisodeFlow(EpisodeNumber))
    {
        UE_LOG(LogTemp, Warning, TEXT("HavocSmoke: no Episode %02d flow in this level; cannot start the episode"), EpisodeNumber);
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("HavocSmoke: starting Episode %02d"), EpisodeNumber);
    // 30 s in: frame the episode's map explicitly, then shoot.
    // Help: https://dev.epicgames.com/documentation/en-us/unreal-engine/taking-screenshots-in-unreal-engine
    GetWorldTimerManager().SetTimer(SmokeShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        FrameMapForSmoke(this, EpisodeNumber);
        // Shoot a few seconds later so auto-exposure has adapted to the new view (it resets on a camera cut).
        GetWorldTimerManager().SetTimer(SmokeShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            const AActor* Target = GetViewTarget();
            UE_LOG(LogTemp, Display, TEXT("HavocSmoke: taking screenshot (view target %s)"), Target ? *Target->GetName() : TEXT("none"));
            ConsoleCommand(TEXT("HighResShot 1280x720"));
        }), 4.f, false);
    }), 30.f, false);
    // 35 s in (after the screenshot is written): the outro (10 s), so every episode reports both cutscenes.
    FTimerHandle OutroTimer;
    GetWorldTimerManager().SetTimer(OutroTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        // The cutscenes are not part of IHavocEpisodeFlow: Episode 04 has its own, 01/02 use the flow base, 03 its own.
        if (AHavocEpisode04Flow* E04 = GetEpisode04Flow())
        {
            E04->PlayOutro();
        }
        else if (AHavocEpisodeFlowBase* Base = Cast<AHavocEpisodeFlowBase>(Flow.GetObject()))
        {
            Base->PlayOutro();
        }
        else if (AHavocEpisode03Flow* E03 = GetFlow03())
        {
            E03->PlayOutro();
        }
    }), 35.f, false);
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

void AHavocPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P); // local player only
    if (!IsLocalController() || !P)
    {
        return;
    }
    if (Cast<AHavocFalconHelicopter>(P))
    {
        ShowFirstTimeHint(EHavocHintKind::Helicopter);
    }
    else if (Cast<AHavocDrone>(P))
    {
        ShowFirstTimeHint(EHavocHintKind::Drone);
    }
    else if (Cast<AHavocVehicleBase>(P))
    {
        ShowFirstTimeHint(EHavocHintKind::Vehicle);
    }
}

void AHavocPlayerController::ShowFirstTimeHint(EHavocHintKind Kind)
{
    const UGameInstance* GI = GetGameInstance();
    UHavocPlaySettings* Settings = GI ? GI->GetSubsystem<UHavocPlaySettings>() : nullptr;
    const UHavocPlayInput* Input = GI ? GI->GetSubsystem<UHavocPlayInput>() : nullptr;
    if (!Settings || !Input || Settings->IsHintSeen(Kind))
    {
        return;
    }
    Settings->MarkHintSeen(Kind);
    ActiveHint = Input->HintFor(Kind) + TEXT("\n(Press H or gamepad X to dismiss)");
    HintSecondsLeft = 25.f;
}

void AHavocPlayerController::DismissHint()
{
    ActiveHint.Reset();
    HintSecondsLeft = 0.f;
}

void AHavocPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HintSecondsLeft > 0.f)
    {
        HintSecondsLeft -= DeltaSeconds;
        if (HintSecondsLeft <= 0.f)
        {
            DismissHint();
        }
    }
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
    Enhanced->BindAction(Input->DismissHintAction, ETriggerEvent::Started, this, &AHavocPlayerController::OnDismissHint);
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

AHavocEpisode03Flow* AHavocPlayerController::GetFlow03()
{
    if (!Flow03)
    {
        for (TActorIterator<AHavocEpisode03Flow> It(GetWorld()); It; ++It)
        {
            Flow03 = *It;
            break;
        }
    }
    return Flow03;
}

AActor* AHavocPlayerController::SelectFlow(TSubclassOf<AActor> FlowClass)
{
    UWorld* World = GetWorld();
    if (!World || !FlowClass)
    {
        return nullptr;
    }
    if (Flow.GetObject() && Flow.GetObject()->IsA(FlowClass))
    {
        return Cast<AActor>(Flow.GetObject());
    }
    AActor* Found = nullptr;
    for (TActorIterator<AActor> It(World, FlowClass); It; ++It)
    {
        Found = *It;
        break;
    }
    if (!Found)
    {
        Found = World->SpawnActor<AActor>(FlowClass, FTransform::Identity);
    }
    if (Found && Found->Implements<UHavocEpisodeFlow>())
    {
        Flow = Found;
    }
    return Found;
}

// Episode-specific keys: Episode 04 (beach choice, Raven, Falcon), the AHavocEpisodeFlowBase episodes
// (Episode 02: route choice, Raven, Falcon) and Episode 03 (Falcon, interact).
void AHavocPlayerController::OnChooseNorth()
{
    if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->ChooseBeach(true); }
    else if (AHavocEpisodeFlowBase* B = Cast<AHavocEpisodeFlowBase>(Flow.GetObject())) { B->ChooseBranch(true); }
}
void AHavocPlayerController::OnChooseWest()
{
    if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->ChooseBeach(false); }
    else if (AHavocEpisodeFlowBase* B = Cast<AHavocEpisodeFlowBase>(Flow.GetObject())) { B->ChooseBranch(false); }
}
void AHavocPlayerController::OnInteract()
{
    if (IHavocEpisodeFlow* F = GetFlow()) { F->HandleInteract(); }
    else if (AHavocEpisode03Flow* F3 = GetFlow03()) { F3->HandleInteract(); }
}
void AHavocPlayerController::OnDeployRaven()
{
    if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->DeployRaven(); }
    else if (AHavocEpisodeFlowBase* B = Cast<AHavocEpisodeFlowBase>(Flow.GetObject())) { B->DeployRaven(); }
}
void AHavocPlayerController::OnCallFalcon()
{
    if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->CallFalcon(); }
    else if (AHavocEpisodeFlowBase* B = Cast<AHavocEpisodeFlowBase>(Flow.GetObject())) { B->CallFalcon(); }
    else if (AHavocEpisode03Flow* F3 = GetFlow03()) { F3->CallFalcon(); }
}
void AHavocPlayerController::OnReturn()
{
    if (AHavocEpisode04Flow* F = GetEpisode04Flow()) { F->ReturnToMarine(); }
    else if (AHavocEpisodeFlowBase* B = Cast<AHavocEpisodeFlowBase>(Flow.GetObject())) { B->ReturnToMarine(); }
}

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
    AHavocEpisode03Flow* F3 = F ? nullptr : GetFlow03();
    const EHavocFlowPhase FlowPhase = F ? F->GetPhase() : (F3 ? F3->GetPhase() : EHavocFlowPhase::Menu);
    if (FlowPhase != EHavocFlowPhase::Playing)
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
