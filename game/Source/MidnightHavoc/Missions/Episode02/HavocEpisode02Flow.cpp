#include "Missions/Episode02/HavocEpisode02Flow.h"

#include "AI/HavocAIController.h"
#include "AI/HavocEnemyCharacter.h"
#include "AI/HavocSquadSubsystem.h"
#include "Assets/HavocAssetSubsystem.h"
#include "Audio/HavocAudioSubsystem.h"
#include "Combat/HavocWeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HavocEventSubsystem.h"
#include "Data/HavocDataSubsystem.h"
#include "Drones/HavocDrone.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Missions/Episode02/HavocConvoyEscort.h"
#include "Missions/Episode02/HavocRelayNetwork.h"
#include "Missions/Episode02/HavocSilentCorridorMission.h"
#include "Player/HavocPlayerCharacter.h"
#include "Player/HavocPlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHavocFlow02, Log, All);

namespace
{
    // Landmarks, game-space metres from Checkpoint Echo (x east, y up, z along the valley).
    const FVector Echo(0.0, 0.0, 0.0);
    const FVector OutagePoint(1000.0, 0.0, 1050.0);
    const FVector BridgeApproach(1380.0, 0.0, 1480.0);
    const FVector Bridge(1450.0, 0.0, 1550.0);
    const FVector DrainageMouth(1400.0, 0.0, 1480.0);
    const FVector FarBank(1560.0, 0.0, 1620.0);
    const FVector Narrows(1250.0, 0.0, 1650.0);
    const FVector GateApproach(1760.0, 0.0, 1840.0);
    const FVector SouthGate(1800.0, 0.0, 1900.0);

    const FName AmbushTag(TEXT("e02_ambush"));
    const FName FarBankTag(TEXT("e02_farbank"));
    const FName YardTag(TEXT("e02_yard"));

    // The ambush shoots at the convoy while it is this close to the narrows (metres).
    const double KillZoneRadius = 150.0;

    double Horizontal(const FVector& A, const FVector& B)
    {
        return FMath::Sqrt(FMath::Square(A.X - B.X) + FMath::Square(A.Z - B.Z));
    }

    /** Attaches a visual for an asset role (skeletal or static mesh) to Parent. Null if the role has no mesh. */
    USceneComponent* AttachVisual(AActor* Owner, USceneComponent* Parent, FName Role)
    {
        const UGameInstance* GI = Owner ? Owner->GetGameInstance() : nullptr;
        const UHavocAssetSubsystem* Assets = GI ? GI->GetSubsystem<UHavocAssetSubsystem>() : nullptr;
        UObject* Asset = Assets ? Assets->LoadAsset(Role) : nullptr;
        if (USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Asset))
        {
            USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(Owner);
            Comp->SetSkeletalMesh(Skeletal);
            Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Comp->SetupAttachment(Parent);
            Comp->RegisterComponent();
            return Comp;
        }
        return nullptr;
    }

    struct FTrackerStep
    {
        const TCHAR* Label;
        TArray<FString> States;
    };

    const TArray<FTrackerStep>& TrackerSteps()
    {
        static const TArray<FTrackerStep> Steps = {
            { TEXT("Place Relay Kilo with the Raven"), { TEXT("E02_BRIEFING"), TEXT("E02_CONVOY_FORMUP"), TEXT("E02_RELAY_KILO") } },
            { TEXT("Scout the corridor"), { TEXT("E02_CORRIDOR_SCOUT") } },
            { TEXT("Advance the convoy"), { TEXT("E02_CORRIDOR_ADVANCE") } },
            { TEXT("Restore Relay Kilo (comms outage)"), { TEXT("E02_COMMS_OUTAGE") } },
            { TEXT("Choose: riverbed or drainage"), { TEXT("E02_BRIDGE_OBSTRUCTED") } },
            { TEXT("Get past the bridge"), { TEXT("E02_RIVERBED_DETOUR"), TEXT("E02_DRAINAGE_INFILTRATION"), TEXT("E02_BRIDGE_CLEARED") } },
            { TEXT("Clear South Gate"), { TEXT("E02_SOUTH_GATE_APPROACH") } },
            { TEXT("Set up the staging area"), { TEXT("E02_STAGING_ESTABLISHED") } }
        };
        return Steps;
    }
}

// =========================================================== flow

AHavocEpisode02Flow::AHavocEpisode02Flow()
    : MissionId(TEXT("episode_02_silent_corridor"))
{
    PrimaryActorTick.bCanEverTick = true;
}

void AHavocEpisode02Flow::BeginPlay()
{
    Super::BeginPlay();
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Subscribe(TEXT("player:died"), this, GET_FUNCTION_NAME_CHECKED(AHavocEpisode02Flow, OnPlayerDied));
        }
    }
}

void AHavocEpisode02Flow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Unsubscribe(TEXT("player:died"), this, GET_FUNCTION_NAME_CHECKED(AHavocEpisode02Flow, OnPlayerDied));
        }
    }
    Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------- lookup and helpers

UHavocSilentCorridorMission* AHavocEpisode02Flow::GetMission() const
{
    const UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    return Missions ? Missions->GetSilentCorridor() : nullptr;
}

AHavocPlayerController* AHavocEpisode02Flow::GetPC()
{
    if (!CachedPC)
    {
        CachedPC = Cast<AHavocPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    }
    return CachedPC;
}

AHavocPlayerCharacter* AHavocEpisode02Flow::GetMarine()
{
    if (!CachedMarine)
    {
        if (AHavocPlayerController* PC = GetPC())
        {
            CachedMarine = Cast<AHavocPlayerCharacter>(PC->GetPawn());
            if (!CachedMarine)
            {
                CachedMarine = PC->GetMarine();
            }
        }
    }
    return CachedMarine;
}

int32 AHavocEpisode02Flow::CountAlive(FName Tag) const
{
    int32 N = 0;
    for (const AHavocEnemyCharacter* Enemy : Enemies)
    {
        if (Enemy && !Enemy->IsDead() && Enemy->SectorId == Tag)
        {
            ++N;
        }
    }
    return N;
}

FVector AHavocEpisode02Flow::ToWorld(const FVector& Game) const
{
    return GreyboxOrigin + FVector(Game.X, Game.Z, Game.Y) * 100.0;
}

FVector AHavocEpisode02Flow::ToWorldGround(const FVector& Game) const
{
    return GreyboxOrigin + FVector(Game.X, Game.Z, 0.0) * 100.0;
}

FVector AHavocEpisode02Flow::ToGame(const FVector& World) const
{
    const FVector D = (World - GreyboxOrigin) / 100.0;
    return FVector(D.X, D.Z, D.Y);
}

void AHavocEpisode02Flow::Publish(const TCHAR* EventName, const FString& Payload) const
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Publish(EventName, Payload);
        }
    }
}

bool AHavocEpisode02Flow::IsPiloting() const
{
    const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    return Pawn && Pawn == Raven.Get();
}

TArray<FString> AHavocEpisode02Flow::BuildTracker(const FString& State, bool bComplete)
{
    const TArray<FTrackerStep>& Steps = TrackerSteps();
    int32 Current = INDEX_NONE;
    for (int32 i = 0; i < Steps.Num(); ++i)
    {
        if (Steps[i].States.Contains(State))
        {
            Current = i;
        }
    }
    TArray<FString> Lines;
    for (int32 i = 0; i < Steps.Num(); ++i)
    {
        const bool bDone = bComplete || (Current != INDEX_NONE && i < Current);
        const bool bNow = !bComplete && i == Current;
        Lines.Add(FString::Printf(TEXT("%s %s"), bDone ? TEXT("[x]") : bNow ? TEXT("[>]") : TEXT("[ ]"), Steps[i].Label));
    }
    return Lines;
}

FString AHavocEpisode02Flow::GetObjectiveText() const
{
    const UHavocSilentCorridorMission* Mission = GetMission();
    return Mission && Mission->GetRunner() ? Mission->GetRunner()->GetObjective() : FString();
}

FString AHavocEpisode02Flow::GetTrackerText() const
{
    const UHavocSilentCorridorMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner())
    {
        return FString();
    }
    const UHavocMissionRunner* Runner = Mission->GetRunner();
    const bool bDone = Runner->IsComplete() && Runner->GetOutcome() != TEXT("FAILED");
    return FString::Join(BuildTracker(Runner->GetState(), bDone), TEXT("\n"));
}

FString AHavocEpisode02Flow::GetStatusText() const
{
    const UHavocSilentCorridorMission* Mission = GetMission();
    if (!Mission)
    {
        return FString();
    }
    FString Text = TEXT("Convoy:");
    for (const FHavocConvoyVehicle& V : Mission->GetConvoy()->GetVehicles())
    {
        Text += FString::Printf(TEXT("  %s %s"), *V.Id, V.Integrity > 0.0 ? *FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V.Integrity * 100.0)) : TEXT("LOST"));
    }
    FHavocFieldRelay Kilo;
    const TCHAR* Relay = TEXT("not placed");
    if (Mission->GetRelays()->FindRelay(TEXT("relay-kilo"), Kilo))
    {
        Relay = Kilo.State == EHavocRelayState::Online ? TEXT("online") : TEXT("JAMMED");
    }
    Text += FString::Printf(TEXT("\nRelay Kilo: %s"), Relay);
    if (const AHavocRavenDrone* Drone = Raven.Get())
    {
        Text += FString::Printf(TEXT("   Link: %d%%"), FMath::RoundToInt(Drone->Core.Signal * 100.0));
    }
    return Text;
}

FString AHavocEpisode02Flow::GetResultTitle() const
{
    return Outcome == TEXT("SUCCESS") ? TEXT("CORRIDOR SECURED - MISSION COMPLETE")
        : Outcome == TEXT("PARTIAL") ? TEXT("CORRIDOR SECURED - WITH LOSSES")
        : TEXT("CONVOY LOST");
}

// ---------------------------------------------------------------- episode control

void AHavocEpisode02Flow::StartEpisode()
{
    UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    if (!Missions)
    {
        return;
    }
    Missions->SetEpisode1CrewRecovered(bCrewRecovered);
    if (!Missions->StartMission(MissionId))
    {
        ShowToast(TEXT("Episode 02 could not start. Check that the data folder is packaged beside the game (game/README.md)."), 10.f);
        UE_LOG(LogHavocFlow02, Error, TEXT("Mission '%s' failed to start"), *MissionId.ToString());
        return;
    }
    ClearWorld();
    BuildGreybox();
    Phase = EHavocFlowPhase::Playing;
    LastState.Empty();
    bRespawnPending = false;
    ResupplyLeft = ResupplyCharges;
    ResupplyCooldown = 0.f;
    Outcome.Empty();
    ResultSummary.Empty();
    SetHint(FString());

    if (AHavocPlayerCharacter* Marine = GetMarine())
    {
        TeleportMarine(FVector(-12.0, 0.0, -12.0));
        if (Marine->IsDead())
        {
            Marine->Revive(Marine->GetActorTransform());
        }
        CheckpointTransform = Marine->GetActorTransform();
        bHaveCheckpointTransform = true;
    }
    if (AHavocPlayerController* PC = GetPC())
    {
        PC->HideMenu();
        PC->ShowHud();
    }
    if (UHavocAudioSubsystem* Audio = GetWorld()->GetSubsystem<UHavocAudioSubsystem>())
    {
        Audio->StartAmbient();
    }
    // Intro fly-over of the convoy route, then briefing -> convoy form-up -> Relay Kilo (the runner stops
    // there until the relay is placed). Tick pauses the state machine while the cutscene plays.
    TWeakObjectPtr<AHavocEpisode02Flow> Self(this);
    PlayCutscene(true, [Self]()
    {
        if (AHavocEpisode02Flow* Flow = Self.Get())
        {
            if (UHavocSilentCorridorMission* Mission = Flow->GetMission())
            {
                Mission->Progress();
            }
        }
    });
}

TArray<FVector> AHavocEpisode02Flow::BuildCutsceneTargets(const FVector& Origin, bool bIntro)
{
    // The convoy route in game metres (same frame as ToWorldGround: x east, z along the valley).
    const TArray<FVector> Route = bIntro ? TArray<FVector>{ Echo, Bridge, SouthGate } : TArray<FVector>{ SouthGate, Narrows };
    TArray<FVector> Targets;
    for (const FVector& P : Route)
    {
        Targets.Add(Origin + FVector(P.X, P.Z, 0.0) * 100.0);
    }
    return Targets;
}

void AHavocEpisode02Flow::RestartFromCheckpoint()
{
    if (Phase == EHavocFlowPhase::Menu)
    {
        return;
    }
    Phase = EHavocFlowPhase::Playing;
    if (AHavocPlayerController* PC = GetPC())
    {
        PC->HideMenu();
        PC->ShowHud();
    }
    RespawnFromCheckpoint();
}

void AHavocEpisode02Flow::ClearWorld()
{
    for (AHavocEnemyCharacter* Enemy : Enemies)
    {
        if (Enemy)
        {
            Enemy->Destroy();
        }
    }
    Enemies.Reset();
    AmbushEnemies.Reset();
    DestroyConvoyActors();
    RecoverRaven(nullptr);
    AmbushTimer = 0.f;
    AmbushHits = 0;
    PhaseTimer = 0.f;
}

void AHavocEpisode02Flow::EndEpisode()
{
    UHavocSilentCorridorMission* Mission = GetMission();
    if (!Mission)
    {
        return;
    }
    Phase = EHavocFlowPhase::Results;
    Outcome = Mission->GetRunner()->GetOutcome();
    RecoverRaven(nullptr);

    const FHavocEpisode02Carryover C = Mission->GetCarryover();
    const TCHAR* Route = C.Route == TEXT("RIVERBED") ? TEXT("Dry Riverbed")
        : C.Route == TEXT("RIVER_DRAINAGE") ? TEXT("River drainage tunnel") : TEXT("Main road");
    ResultSummary = FString::Printf(
        TEXT("Route: %s\nFalcon-2 crew delivered: %s\nNomads remaining: %d of 2\nWarden-1: %s\nRelays online: %d"),
        Route, C.bCrewDelivered ? TEXT("yes") : TEXT("no"), C.NomadsRemaining,
        C.bWardenOperational ? TEXT("operational") : TEXT("lost"), C.RelaysOnline);
    Publish(TEXT("episode:results"), Mission->GetSummaryJson());

    // Outro fly-over, then the results screen (skippable).
    TWeakObjectPtr<AHavocEpisode02Flow> Self(this);
    PlayCutscene(false, [Self]()
    {
        if (AHavocEpisode02Flow* Flow = Self.Get())
        {
            if (AHavocPlayerController* PC = Flow->GetPC())
            {
                PC->ShowMenu(EHavocMenuMode::Results);
            }
        }
    });
}

// ---------------------------------------------------------------- tick

void AHavocEpisode02Flow::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ToastTime = FMath::Max(0.f, ToastTime - DeltaSeconds);
    LinkToastCooldown = FMath::Max(0.f, LinkToastCooldown - DeltaSeconds);
    ResupplyCooldown = FMath::Max(0.f, ResupplyCooldown - DeltaSeconds);

    if (Phase != EHavocFlowPhase::Playing || IsCutsceneActive())
    {
        return;
    }
    UHavocSilentCorridorMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner())
    {
        return;
    }
    UHavocMissionRunner* Runner = Mission->GetRunner();

    if (Runner->IsComplete())
    {
        EndEpisode();
        return;
    }
    if (bRespawnPending)
    {
        return;
    }

    const FString State = Runner->GetState();
    if (State != LastState)
    {
        OnEnterState(State, false);
    }

    TickConvoy(DeltaSeconds);
    TickRaven(DeltaSeconds);
    TickState(Runner->GetState(), DeltaSeconds);
}

void AHavocEpisode02Flow::Step()
{
    if (UHavocSilentCorridorMission* Mission = GetMission())
    {
        Mission->GetRunner()->Advance();
    }
}

void AHavocEpisode02Flow::OnEnterState(const FString& State, bool bRestored)
{
    UHavocSilentCorridorMission* Mission = GetMission();
    UHavocMissionRunner* Runner = Mission->GetRunner();
    LastState = State;
    PhaseTimer = 0.f;
    const bool bCheckpointState = Runner->HasCheckpoint() && Runner->GetCheckpoint().State == State;

    // Where the convoy sits for this state, and whether it snaps there (after a restore) or rolls there.
    if (State == TEXT("E02_RELAY_KILO") || State == TEXT("E02_CORRIDOR_SCOUT"))
    {
        SetConvoyGoal(Echo, true);
    }
    else if (State == TEXT("E02_CORRIDOR_ADVANCE"))
    {
        SetConvoyGoal(OutagePoint, bRestored);
    }
    else if (State == TEXT("E02_COMMS_OUTAGE"))
    {
        SetConvoyGoal(OutagePoint, true);
    }
    else if (State == TEXT("E02_BRIDGE_OBSTRUCTED") || State == TEXT("E02_DRAINAGE_INFILTRATION"))
    {
        SetConvoyGoal(BridgeApproach, State == TEXT("E02_DRAINAGE_INFILTRATION") || bRestored);
    }
    else if (State == TEXT("E02_RIVERBED_DETOUR"))
    {
        SetConvoyGoal(Narrows, bRestored);
    }
    else if (State == TEXT("E02_BRIDGE_CLEARED"))
    {
        SetConvoyGoal(Bridge, bRestored);
    }
    else if (State == TEXT("E02_SOUTH_GATE_APPROACH"))
    {
        SetConvoyGoal(GateApproach, bRestored);
    }
    else if (State == TEXT("E02_STAGING_ESTABLISHED"))
    {
        SetConvoyGoal(SouthGate, bRestored);
    }
    SyncConvoy();

    if (State == TEXT("E02_RELAY_KILO"))
    {
        RecoverRaven(nullptr);
        LaunchRaven(Echo);
        ShowToast(TEXT("Raven is airborne. Fly to the relay site on the valley shoulder and press E to drop Relay Kilo."), 8.f);
    }
    else if (State == TEXT("E02_CORRIDOR_SCOUT"))
    {
        if (!Raven)
        {
            LaunchRaven(HavocE02::RelayKilo());
        }
        ShowToast(TEXT("Relay Kilo is online. Fly Raven to the end of the corridor and mark what is ahead."), 8.f);
    }
    else if (State == TEXT("E02_CORRIDOR_ADVANCE"))
    {
        RecoverRaven(nullptr);
        ShowToast(TEXT("The convoy rolls down the main road."), 5.f);
    }
    else if (State == TEXT("E02_COMMS_OUTAGE"))
    {
        RecoverRaven(nullptr);
        TeleportMarine(FVector(OutagePoint.X - 15.0, 0.0, OutagePoint.Z - 15.0));
        ShowToast(TEXT("COMMUNICATIONS OUTAGE: every relay is jammed. Go back to Relay Kilo and press E."), 8.f);
    }
    else if (State == TEXT("E02_BRIDGE_OBSTRUCTED"))
    {
        if (!bRestored)
        {
            TeleportMarine(FVector(BridgeApproach.X - 20.0, 0.0, BridgeApproach.Z - 20.0));
        }
        ShowToast(TEXT("The bridge is blocked. Press 1 for the Dry Riverbed (whole convoy) or 2 for the drainage tunnel (infantry only)."), 10.f);
    }
    else if (State == TEXT("E02_RIVERBED_DETOUR"))
    {
        RecoverRaven(nullptr);
        TeleportMarine(FVector(Narrows.X - 15.0, 0.0, Narrows.Z - 70.0));
        AmbushEnemies.Reset();
        const FVector P = HavocE02::AmbushNarrows();
        for (int32 i = 0; i < Mission->GetRemainingAmbush().Num(); ++i)
        {
            TArray<AHavocEnemyCharacter*> One;
            SpawnEnemies(FVector(P.X + i * 40.0, 0.0, P.Z - i * 25.0), 1, 6.f, AmbushTag, &One);
            AmbushEnemies.Add(One.Num() > 0 ? One[0] : nullptr);
        }
        AmbushTimer = AmbushVolleySeconds;
        AmbushHits = 0;
        ShowToast(TEXT("AMBUSH in the narrows! Repel it and keep Nomad-1 alive."), 8.f);
    }
    else if (State == TEXT("E02_DRAINAGE_INFILTRATION"))
    {
        TeleportMarine(FVector(DrainageMouth.X, 0.0, DrainageMouth.Z));
        if (CountAlive(FarBankTag) == 0)
        {
            SpawnEnemies(FarBank, FarBankGarrison, 25.f, FarBankTag);
        }
        ShowToast(TEXT("Infantry only: through the drainage tunnel and clear the far bank while the convoy holds."), 8.f);
    }
    else if (State == TEXT("E02_BRIDGE_CLEARED"))
    {
        ShowToast(TEXT("Warden-1 is pushing the wreck off the bridge."), 6.f);
    }
    else if (State == TEXT("E02_SOUTH_GATE_APPROACH"))
    {
        TeleportMarine(FVector(GateApproach.X - 20.0, 0.0, GateApproach.Z - 40.0));
        if (CountAlive(YardTag) == 0)
        {
            SpawnEnemies(SouthGate, StagingGarrison, 35.f, YardTag);
        }
        ShowToast(TEXT("South Gate. Clear the staging yard."), 6.f);
    }
    else if (State == TEXT("E02_STAGING_ESTABLISHED"))
    {
        ShowToast(TEXT("Yard clear. Setting up the forward staging area."), 6.f);
    }

    // The checkpoint remembers where the Marine stood once the state's own teleport has happened.
    if (bCheckpointState && !bRestored)
    {
        if (AHavocPlayerCharacter* Marine = GetMarine())
        {
            CheckpointTransform = Marine->GetActorTransform();
            bHaveCheckpointTransform = true;
        }
    }
}

void AHavocEpisode02Flow::TickState(const FString& State, float Dt)
{
    UHavocSilentCorridorMission* Mission = GetMission();
    if (!Mission)
    {
        return;
    }
    UHavocMissionRunner* Runner = Mission->GetRunner();
    AHavocPlayerCharacter* Marine = GetMarine();

    if (State == TEXT("E02_BRIEFING") || State == TEXT("E02_CONVOY_FORMUP"))
    {
        Mission->Progress();
    }
    else if (State == TEXT("E02_RELAY_KILO"))
    {
        const double Left = Raven ? Horizontal(ToGame(Raven->GetActorLocation()), HavocE02::RelayKilo()) : -1.0;
        SetHint(Raven
            ? FString::Printf(TEXT("Raven: fly to the relay site (%d m away), press E to drop Relay Kilo. W/A/S/D, Space up, Ctrl down. Tab = back to Marine."), FMath::RoundToInt(Left))
            : FString(TEXT("Press T to launch the Raven.")));
        if (Runner->HasFlag(TEXT("relay_kilo_online")))
        {
            Step();
        }
    }
    else if (State == TEXT("E02_CORRIDOR_SCOUT"))
    {
        TickScout(Dt);
        if (Runner->HasFlag(TEXT("corridor_scouted")))
        {
            Step();
        }
    }
    else if (State == TEXT("E02_CORRIDOR_ADVANCE"))
    {
        SetHint(FString::Printf(TEXT("Convoy advancing: %d m to the halt."), FMath::RoundToInt(FVector::Dist(ConvoyPos, ConvoyGoal))));
        if (ConvoyAtGoal())
        {
            Step(); // -> communications outage
        }
    }
    else if (State == TEXT("E02_COMMS_OUTAGE"))
    {
        if (Marine)
        {
            const FVector Here = ToGame(Marine->GetActorLocation());
            SetHint(FString::Printf(TEXT("Jammed. Walk to Relay Kilo (%d m) and press E to restore it. Raven cannot fly."),
                FMath::RoundToInt(Horizontal(Here, HavocE02::RelayKilo()))));
        }
        if (Runner->HasFlag(TEXT("relay_kilo_restored")))
        {
            Step(); // -> bridge obstructed
        }
    }
    else if (State == TEXT("E02_BRIDGE_OBSTRUCTED"))
    {
        SetHint(TEXT("Press 1: Dry Riverbed detour (ambush likely, convoy at risk)   Press 2: drainage tunnel (infantry clears the far bank first)"));
    }
    else if (State == TEXT("E02_RIVERBED_DETOUR"))
    {
        for (int32 i = 0; i < AmbushEnemies.Num(); ++i)
        {
            const AHavocEnemyCharacter* Enemy = AmbushEnemies[i];
            if (!Enemy || Enemy->IsDead())
            {
                Mission->ClearAmbushContact(FString::Printf(TEXT("e02-ambush-%d"), i + 1));
            }
        }
        TickAmbush(Dt);
        SetHint(FString::Printf(TEXT("Repel the ambush: %d hostiles left. The convoy is under fire."), Mission->GetRemainingAmbush().Num()));
        if (Runner->HasFlag(TEXT("ambush_repelled")))
        {
            ShowToast(TEXT("Ambush repelled. The convoy moves on."), 5.f);
            Step();
        }
    }
    else if (State == TEXT("E02_DRAINAGE_INFILTRATION"))
    {
        const int32 Left = CountAlive(FarBankTag);
        SetHint(FString::Printf(TEXT("Clear the far bank: %d hostiles left. No vehicle fits the tunnel."), Left));
        if (Left == 0)
        {
            Mission->ClearFarBank();
            Step();
            ShowToast(TEXT("Far bank clear."), 4.f);
        }
    }
    else if (State == TEXT("E02_BRIDGE_CLEARED"))
    {
        if (!Mission->GetConvoy()->IsWardenOperational())
        {
            Runner->Fail(); // nothing can push the wreck off the bridge
            return;
        }
        PhaseTimer += Dt;
        SetHint(FString::Printf(TEXT("Warden-1 is clearing the bridge: %d s."), FMath::CeilToInt(FMath::Max(0.f, BridgeClearSeconds - PhaseTimer))));
        if (PhaseTimer >= BridgeClearSeconds)
        {
            Mission->ClearBridge();
            Step();
        }
    }
    else if (State == TEXT("E02_SOUTH_GATE_APPROACH"))
    {
        const int32 Left = CountAlive(YardTag);
        SetHint(FString::Printf(TEXT("Clear the staging yard: %d hostiles left."), Left));
        if (Left == 0)
        {
            Mission->ClearStaging();
            Step();
        }
    }
    else if (State == TEXT("E02_STAGING_ESTABLISHED"))
    {
        PhaseTimer += Dt;
        SetHint(FString::Printf(TEXT("Setting up the forward staging area: %d s."), FMath::CeilToInt(FMath::Max(0.f, StagingSetupSeconds - PhaseTimer))));
        if (PhaseTimer >= StagingSetupSeconds)
        {
            Mission->Finish();
        }
    }
}

// ---------------------------------------------------------------- greybox world

AStaticMeshActor* AHavocEpisode02Flow::SpawnBlock(const FVector& GameGroundCentre, const FVector& SizeMeters, double BaseMeters)
{
    UWorld* World = GetWorld();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!World || !Cube)
    {
        return nullptr;
    }
    // The engine cube is 100 cm, so the scale is the size in metres. Size is (east, valley, up).
    const FVector Location = ToWorldGround(GameGroundCentre) + FVector(0.0, 0.0, (BaseMeters + SizeMeters.Z * 0.5) * 100.0);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Block = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator, Params);
    if (!Block)
    {
        return nullptr;
    }
    Block->SetMobility(EComponentMobility::Movable);
    Block->GetStaticMeshComponent()->SetStaticMesh(Cube);
    Block->SetActorScale3D(SizeMeters);
    return Block;
}

void AHavocEpisode02Flow::BuildGreybox()
{
    if (Greybox.Num() > 0)
    {
        return;
    }
    const auto Add = [this](AStaticMeshActor* Block) { if (Block) { Greybox.Add(Block); } };

    Add(SpawnBlock(FVector(900.0, 0.0, 1000.0), FVector(2600.0, 2800.0, 2.0), -2.0));        // valley floor
    Add(SpawnBlock(FVector(-40.0, 0.0, -40.0), FVector(4.0, 4.0, 8.0)));                       // Checkpoint Echo mast
    Add(SpawnBlock(FVector(HavocE02::RelayKilo().X, 0.0, HavocE02::RelayKilo().Z), FVector(2.0, 2.0, 14.0))); // Relay Kilo site
    Add(SpawnBlock(FVector(HavocE02::CorridorEnd().X, 0.0, HavocE02::CorridorEnd().Z), FVector(3.0, 3.0, 22.0))); // corridor end marker
    Add(SpawnBlock(Bridge, FVector(60.0, 14.0, 1.0), 4.0));                                    // bridge deck
    Add(SpawnBlock(Bridge, FVector(10.0, 5.0, 3.0), 5.0));                                     // the wreck on it
    Add(SpawnBlock(DrainageMouth, FVector(8.0, 8.0, 5.0)));                                    // drainage tunnel mouth
    Add(SpawnBlock(FVector(Narrows.X - 95.0, 0.0, Narrows.Z), FVector(50.0, 200.0, 26.0)));    // riverbed narrows, west wall
    Add(SpawnBlock(FVector(Narrows.X + 150.0, 0.0, Narrows.Z), FVector(50.0, 200.0, 26.0)));   // east wall
    Add(SpawnBlock(FVector(SouthGate.X - 40.0, 0.0, SouthGate.Z + 20.0), FVector(10.0, 10.0, 20.0))); // South Gate west tower
    Add(SpawnBlock(FVector(SouthGate.X + 40.0, 0.0, SouthGate.Z + 20.0), FVector(10.0, 10.0, 20.0))); // east tower
}

// ---------------------------------------------------------------- convoy

void AHavocEpisode02Flow::DestroyConvoyActors()
{
    for (TPair<FString, TObjectPtr<AStaticMeshActor>>& Pair : ConvoyActors)
    {
        if (Pair.Value)
        {
            Pair.Value->Destroy();
        }
    }
    ConvoyActors.Reset();
}

void AHavocEpisode02Flow::SetConvoyGoal(const FVector& GameGoal, bool bSnapNow)
{
    ConvoyGoal = FVector(GameGoal.X, 0.0, GameGoal.Z);
    if (bSnapNow)
    {
        ConvoyPos = ConvoyGoal;
    }
}

bool AHavocEpisode02Flow::ConvoyAtGoal() const
{
    return FVector::Dist(ConvoyPos, ConvoyGoal) < 2.0;
}

void AHavocEpisode02Flow::SyncConvoy()
{
    UHavocSilentCorridorMission* Mission = GetMission();
    UWorld* World = GetWorld();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Mission || !World || !Cube)
    {
        return;
    }
    const FVector Heading(FMath::Cos(ConvoyHeading), 0.0, FMath::Sin(ConvoyHeading));
    const TArray<FHavocConvoyVehicle> Vehicles = Mission->GetConvoy()->GetVehicles();
    for (int32 i = 0; i < Vehicles.Num(); ++i)
    {
        const FHavocConvoyVehicle& V = Vehicles[i];
        TObjectPtr<AStaticMeshActor>* Found = ConvoyActors.Find(V.Id);
        if (V.Integrity <= 0.0)
        {
            if (Found && *Found)
            {
                (*Found)->Destroy();
                ShowToast(FString::Printf(TEXT("%s was destroyed!"), *V.Id), 6.f);
                Publish(TEXT("convoy:destroyed"), FString::Printf(TEXT("{\"id\":\"%s\"}"), *V.Id));
            }
            if (Found)
            {
                ConvoyActors.Remove(V.Id);
            }
            continue;
        }
        const FVector Slot = ConvoyPos - Heading * (i * 14.0);
        AStaticMeshActor* Actor = Found ? Found->Get() : nullptr;
        if (!Actor)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Actor = World->SpawnActor<AStaticMeshActor>(ToWorldGround(Slot) + FVector(0.0, 0.0, 150.0), FRotator::ZeroRotator, Params);
            if (!Actor)
            {
                continue;
            }
            Actor->SetMobility(EComponentMobility::Movable);
            Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
            const bool bWarden = V.Kind == EHavocConvoyVehicleKind::Warden;
            Actor->SetActorScale3D(bWarden ? FVector(5.5, 3.0, 2.4) : FVector(7.0, 3.0, 3.0)); // Warden-1 is the low armoured one
            ConvoyActors.Add(V.Id, Actor);
        }
        Actor->SetActorLocationAndRotation(ToWorldGround(Slot) + FVector(0.0, 0.0, Actor->GetActorScale3D().Z * 50.0),
            FRotator(0.0, FMath::RadiansToDegrees(ConvoyHeading), 0.0));
    }
}

void AHavocEpisode02Flow::TickConvoy(float Dt)
{
    const FVector Delta = ConvoyGoal - ConvoyPos;
    if (Delta.Size() > 0.5)
    {
        ConvoyHeading = FMath::Atan2(Delta.Z, Delta.X);
        const double Step = ConvoySpeedMps * Dt;
        ConvoyPos = Delta.Size() <= Step ? ConvoyGoal : ConvoyPos + Delta.GetSafeNormal() * Step;
    }
    SyncConvoy();
}

// ---------------------------------------------------------------- enemies

void AHavocEpisode02Flow::SpawnEnemies(const FVector& GameCentre, int32 Count, float RadiusMeters, FName Tag, TArray<AHavocEnemyCharacter*>* OutSpawned)
{
    UWorld* World = GetWorld();
    const UGameInstance* GI = GetGameInstance();
    const UHavocDataSubsystem* Data = GI ? GI->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    if (!World || !Data || Count <= 0)
    {
        UE_LOG(LogHavocFlow02, Warning, TEXT("Cannot spawn enemies for '%s' (world or data missing)"), *Tag.ToString());
        return;
    }
    const TArray<FString> Factions = { TEXT("isis"), TEXT("houthi") }; // AI-only hostiles, abstract data/units ratings
    TArray<AHavocEnemyCharacter*> Spawned;
    for (int32 i = 0; i < Count; ++i)
    {
        TArray<FHavocUnitDef> Units = Data->GetInfantryUnits(Factions[i % Factions.Num()]);
        if (Units.Num() == 0)
        {
            UE_LOG(LogHavocFlow02, Warning, TEXT("No infantry units for faction '%s' in data/units"), *Factions[i % Factions.Num()]);
            continue;
        }
        const FHavocUnitDef& Unit = Units[FMath::RandRange(0, Units.Num() - 1)];
        const double Angle = FMath::FRandRange(0.0, 2.0 * PI);
        const double Reach = FMath::Sqrt(FMath::FRand()) * RadiusMeters * 100.0;
        const FVector Spot = ToWorldGround(GameCentre) + FVector(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach, 110.0);
        const FTransform Where(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Spot);
        AHavocEnemyCharacter* Enemy = World->SpawnActorDeferred<AHavocEnemyCharacter>(
            AHavocEnemyCharacter::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Enemy)
        {
            continue;
        }
        Enemy->ApplyUnitDef(Unit, Tag);
        UGameplayStatics::FinishSpawningActor(Enemy, Where);
        Enemies.Add(Enemy);
        Spawned.Add(Enemy);
        if (OutSpawned)
        {
            OutSpawned->Add(Enemy);
        }
        if (AHavocAIController* AI = Cast<AHavocAIController>(Enemy->GetController()))
        {
            AI->NotifyShotBy(GetMarine()); // the ambush and the yard already know where the Marine is
        }
    }
    if (UHavocSquadSubsystem* Squads = World->GetSubsystem<UHavocSquadSubsystem>())
    {
        Squads->RegisterSquad(EHavocTeam::Opposing, Tag, EHavocSquadPosture::Assault, Spawned);
    }
}

void AHavocEpisode02Flow::TickAmbush(float Dt)
{
    UHavocSilentCorridorMission* Mission = GetMission();
    if (!Mission || FVector::Dist(ConvoyPos, FVector(Narrows.X, 0.0, Narrows.Z)) > KillZoneRadius)
    {
        return;
    }
    AmbushTimer -= Dt;
    if (AmbushTimer > 0.f)
    {
        return;
    }
    AmbushTimer = AmbushVolleySeconds;
    const int32 Shooters = CountAlive(AmbushTag);
    for (int32 i = 0; i < Shooters; ++i)
    {
        const TArray<FHavocConvoyVehicle> Alive = Mission->GetConvoy()->GetAlive();
        if (Alive.Num() == 0)
        {
            return;
        }
        const FHavocConvoyVehicle& Target = Alive[AmbushHits++ % Alive.Num()];
        const double Armour = Target.Kind == EHavocConvoyVehicleKind::Warden ? 0.5 : 1.0;
        Mission->DamageVehicle(Target.Id, AmbushDamagePerHit * Armour);
        ShowToast(FString::Printf(TEXT("Convoy under fire: %s hit."), *Target.Id), 3.f);
        if (Mission->GetRunner()->IsComplete())
        {
            return;
        }
    }
}

// ---------------------------------------------------------------- marine

void AHavocEpisode02Flow::TeleportMarine(const FVector& GamePos)
{
    AHavocPlayerCharacter* Marine = GetMarine();
    if (!Marine)
    {
        return;
    }
    const FRotator Facing(0.f, 55.f, 0.f); // looking down the valley (toward +x, +z)
    Marine->TeleportTo(ToWorldGround(GamePos) + FVector(0.0, 0.0, 120.0), Facing);
    if (AController* C = Marine->GetController())
    {
        C->SetControlRotation(Facing);
    }
}

void AHavocEpisode02Flow::PossessMarine()
{
    AHavocPlayerController* PC = GetPC();
    AHavocPlayerCharacter* Marine = GetMarine();
    if (PC && Marine && PC->GetPawn() != Marine)
    {
        PC->Possess(Marine);
    }
}

void AHavocEpisode02Flow::OnPlayerDied(FName EventName, const FString& Payload)
{
    if (Phase == EHavocFlowPhase::Playing && !bRespawnPending)
    {
        BeginRespawn();
    }
}

void AHavocEpisode02Flow::BeginRespawn()
{
    bRespawnPending = true;
    ShowToast(TEXT("Marine down. Restoring the last checkpoint..."), 4.f);
    GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        RespawnFromCheckpoint();
    }), 3.f, false);
}

void AHavocEpisode02Flow::RespawnFromCheckpoint()
{
    UHavocSilentCorridorMission* Mission = GetMission();
    if (!Mission)
    {
        bRespawnPending = false;
        return;
    }
    UHavocMissionRunner* Runner = Mission->GetRunner();
    ClearWorld();
    if (Runner->HasCheckpoint())
    {
        Runner->RestoreCheckpoint(); // state, flags, history, convoy, relays and ambush list
    }
    if (AHavocPlayerCharacter* Marine = GetMarine())
    {
        Marine->Revive(bHaveCheckpointTransform ? CheckpointTransform : Marine->GetActorTransform());
        PossessMarine();
    }
    bRespawnPending = false;
    if (!Runner->IsComplete())
    {
        OnEnterState(Runner->GetState(), true);
    }
}

// ---------------------------------------------------------------- Raven

void AHavocEpisode02Flow::LaunchRaven(const FVector& GameHome)
{
    UWorld* World = GetWorld();
    AHavocPlayerController* PC = GetPC();
    if (!World || !PC)
    {
        return;
    }
    RecoverRaven(nullptr);
    if (AHavocPlayerCharacter* Marine = GetMarine())
    {
        Marine->ReleaseInputState();
    }
    const FVector Home = ToWorldGround(GameHome) + FVector(0.0, 0.0, 3000.0); // 30 m up

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Raven = World->SpawnActor<AHavocRavenDrone>(AHavocRavenDrone::StaticClass(), FTransform(Home), Params);
    if (!Raven)
    {
        return;
    }
    AttachVisual(Raven, Raven->Mesh, TEXT("Drone.Raven"));
    Raven->Core.Definition.MaxSpeed *= RavenSpeedScale;

    // A hidden marker is the drone's link source; TickRaven moves it to the best online relay.
    AStaticMeshActor* Marker = World->SpawnActor<AStaticMeshActor>(Home, FRotator::ZeroRotator, Params);
    if (Marker)
    {
        Marker->SetMobility(EComponentMobility::Movable);
        Marker->SetActorHiddenInGame(true);
        LinkRelay = Marker;
        Raven->RelayActor = LinkRelay;
    }
    Raven->Launch(Home);
    Raven->SetCameraActive(true);
    PC->Possess(Raven);
}

void AHavocEpisode02Flow::RecoverRaven(const TCHAR* Reason)
{
    if (Raven)
    {
        if (AHavocPlayerController* PC = GetPC())
        {
            if (PC->GetPawn() == Raven.Get())
            {
                PossessMarine();
            }
        }
        Raven->Destroy();
        Raven = nullptr;
    }
    if (LinkRelay)
    {
        LinkRelay->Destroy();
        LinkRelay = nullptr;
    }
    if (Reason)
    {
        ShowToast(Reason);
    }
}

void AHavocEpisode02Flow::DeployRaven()
{
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending)
    {
        return;
    }
    if (LastState == TEXT("E02_RELAY_KILO") || LastState == TEXT("E02_CORRIDOR_SCOUT"))
    {
        if (Raven)
        {
            if (AHavocPlayerController* PC = GetPC())
            {
                PC->Possess(Raven);
            }
            return;
        }
        LaunchRaven(LastState == TEXT("E02_RELAY_KILO") ? Echo : HavocE02::RelayKilo());
    }
    else if (LastState == TEXT("E02_COMMS_OUTAGE"))
    {
        ShowToast(TEXT("The drone link is jammed. Restore Relay Kilo first."), 4.f);
    }
    else
    {
        ShowToast(TEXT("Raven is only used to place Relay Kilo and scout the corridor."), 4.f);
    }
}

void AHavocEpisode02Flow::ReturnToMarine()
{
    if (Phase == EHavocFlowPhase::Playing && IsPiloting())
    {
        PossessMarine();
        ShowToast(TEXT("Back on the Marine. T flies the Raven again."), 3.f);
    }
}

void AHavocEpisode02Flow::CallFalcon()
{
    if (Phase == EHavocFlowPhase::Playing)
    {
        ShowToast(TEXT("There is no Falcon support in the corridor: the crew is on Nomad-1."), 4.f);
    }
}

void AHavocEpisode02Flow::TickRaven(float Dt)
{
    UHavocSilentCorridorMission* Mission = GetMission();
    if (!Raven || !Mission)
    {
        return;
    }
    // Link: the best online relay (Echo or Kilo) is the source, minus the outage interference.
    const FVector DroneGame = ToGame(Raven->GetActorLocation());
    FVector Best = Echo;
    double BestSignal = -1.0;
    for (const FHavocFieldRelay& R : Mission->GetRelays()->GetOnlineSources())
    {
        const double S = FHavocDroneSignalManager::Calculate(DroneGame, R.Position, R.Range);
        if (S > BestSignal)
        {
            BestSignal = S;
            Best = R.Position;
        }
    }
    if (LinkRelay)
    {
        LinkRelay->SetActorLocation(ToWorld(Best));
    }
    Raven->Interference = Mission->GetRelays()->GetInterference();

    switch (Raven->GetDroneState())
    {
    case EHavocDroneState::Recovered:
    case EHavocDroneState::SignalLost:
    case EHavocDroneState::Disabled:
    case EHavocDroneState::Destroyed:
        RecoverRaven(TEXT("Raven is down or has returned. Press T to launch a fresh one."));
        break;
    default:
        break;
    }
}

void AHavocEpisode02Flow::TickScout(float Dt)
{
    UHavocSilentCorridorMission* Mission = GetMission();
    if (!Mission)
    {
        return;
    }
    if (!Raven)
    {
        SetHint(TEXT("Press T to launch the Raven from Relay Kilo."));
        return;
    }
    const FVector DroneGame = ToGame(Raven->GetActorLocation());
    const double Left = Horizontal(DroneGame, HavocE02::CorridorEnd());
    SetHint(FString::Printf(TEXT("Raven: fly to the end of the corridor (%d m) to mark what is ahead. Stay inside relay range. Tab = Marine."), FMath::RoundToInt(Left)));
    if (Left > ScoutRadius)
    {
        return;
    }
    FHavocE02Contact Roadblock;
    Roadblock.Id = TEXT("e02-roadblock");
    Roadblock.Position = FVector(1650.0, 0.0, 1480.0);
    if (Mission->ScoutCorridor({ Roadblock }, DroneGame))
    {
        ShowToast(TEXT("Corridor scouted: a roadblock is marked at the far end."), 6.f);
    }
    else if (LinkToastCooldown <= 0.f)
    {
        LinkToastCooldown = 4.f;
        ShowToast(TEXT("The link is too weak to mark the corridor. Fly higher or closer to Relay Kilo and try again."), 4.f);
    }
}

// ---------------------------------------------------------------- player actions

void AHavocEpisode02Flow::ChooseBranch(bool bFirst)
{
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending)
    {
        return;
    }
    if (LastState != TEXT("E02_BRIDGE_OBSTRUCTED"))
    {
        ShowToast(TEXT("The route choice comes at the blocked bridge."), 3.f);
        return;
    }
    if (UHavocSilentCorridorMission* Mission = GetMission())
    {
        // The runner moves on; Tick sees the new state and OnEnterState sets the scene.
        Mission->ChooseRoute(bFirst ? TEXT("riverbed_detour") : TEXT("drainage_infiltration"));
    }
}

void AHavocEpisode02Flow::HandleInteract()
{
    UHavocSilentCorridorMission* Mission = GetMission();
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending || !Mission || IsCutsceneActive())
    {
        return;
    }
    if (Raven && GetPC() && GetPC()->GetPawn() == Raven.Get())
    {
        if (LastState != TEXT("E02_RELAY_KILO"))
        {
            return;
        }
        const FVector DroneGame = ToGame(Raven->GetActorLocation());
        const FVector Kilo = HavocE02::RelayKilo();
        const double Away = Horizontal(DroneGame, Kilo);
        if (Away > RelayDropRadius)
        {
            ShowToast(FString::Printf(TEXT("Not over the relay site yet: %d m to go."), FMath::RoundToInt(Away)), 3.f);
        }
        else if (Mission->PlaceRelayKilo(FVector(DroneGame.X, Kilo.Y, DroneGame.Z)))
        {
            ShowToast(TEXT("Relay Kilo dropped and online. The drone link now reaches down the corridor."), 6.f);
        }
        else
        {
            ShowToast(TEXT("The link is too weak to drop the relay here. Come back toward Echo and try again."), 4.f);
        }
        return;
    }
    // On foot.
    AHavocPlayerCharacter* Marine = GetMarine();
    if (!Marine || Marine->IsDead())
    {
        return;
    }
    if (LastState == TEXT("E02_COMMS_OUTAGE"))
    {
        const FVector Here = ToGame(Marine->GetActorLocation());
        const FVector Kilo = HavocE02::RelayKilo();
        if (Horizontal(Here, Kilo) <= RestoreRadius)
        {
            // The relay sits on the valley shoulder; the Marine is on the floor beneath it, so match its height.
            if (Mission->RestoreRelayKilo(FVector(Here.X, Kilo.Y, Here.Z)))
            {
                ShowToast(TEXT("Relay Kilo restored. The link is back."), 5.f);
                return;
            }
        }
        else
        {
            ShowToast(FString::Printf(TEXT("Relay Kilo is %d m away."), FMath::RoundToInt(Horizontal(Here, Kilo))), 3.f);
            return;
        }
    }
    if (ResupplyLeft <= 0)
    {
        ShowToast(TEXT("No resupply drops left."), 3.f);
    }
    else if (ResupplyCooldown > 0.f)
    {
        ShowToast(FString::Printf(TEXT("Resupply ready in %d s."), FMath::CeilToInt(ResupplyCooldown)), 3.f);
    }
    else
    {
        --ResupplyLeft;
        ResupplyCooldown = 45.f;
        Marine->Weapon->AddSpareMagazines(2);
        ShowToast(FString::Printf(TEXT("Resupply received: +2 magazines (%d drops left)."), ResupplyLeft), 4.f);
    }
}
