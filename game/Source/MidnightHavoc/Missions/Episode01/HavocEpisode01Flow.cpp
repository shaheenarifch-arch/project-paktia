#include "Missions/Episode01/HavocEpisode01Flow.h"

#include "AI/HavocAIController.h"
#include "AI/HavocEnemyCharacter.h"
#include "AI/HavocSquadSubsystem.h"
#include "Audio/HavocAudioSubsystem.h"
#include "CollisionQueryParams.h"
#include "Combat/HavocWeaponComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HavocEventSubsystem.h"
#include "Data/HavocDataSubsystem.h"
#include "Drones/HavocDrone.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Missions/Episode01/HavocMountainShieldMission.h"
#include "Player/HavocPlayerCharacter.h"
#include "Player/HavocPlayerController.h"
#include "TimerManager.h"
#include "World/HavocSectorVolume.h"

DEFINE_LOG_CATEGORY_STATIC(LogHavocFlow01, Log, All);

namespace
{
    const FName StagingId(TEXT("highlands_staging"));
    const FName EchoId(TEXT("checkpoint_echo"));
    const FName RidgeId(TEXT("ridge_station"));
    const FName CrashId(TEXT("crash_grid_r17"));

    const FName RaiderTag(TEXT("m01_raiders"));
    const FName CrashTag(TEXT("m01_crash"));

    struct FTrackerStep
    {
        const TCHAR* Label;
        TArray<FString> States;
    };

    const TArray<FTrackerStep>& TrackerSteps()
    {
        static const TArray<FTrackerStep> Steps = {
            { TEXT("Learn the controls"), { TEXT("M01_BRIEFING"), TEXT("M01_DRONE_TUTORIAL") } },
            { TEXT("Hold Checkpoint Echo"), { TEXT("M01_HOLD_ECHO") } },
            { TEXT("Set up Ridge Station"), { TEXT("M01_RIDGE_STATION") } },
            { TEXT("Falcon-2 is down: decide"), { TEXT("M01_FALCON_DOWN"), TEXT("M01_CRASH_DECISION") } },
            { TEXT("Recover the crew or leave them"), { TEXT("M01_CREW_RECOVERY"), TEXT("M01_ABANDONED") } },
            { TEXT("Repel the counterattack"), { TEXT("M01_COUNTERATTACK") } },
            { TEXT("Debrief"), { TEXT("M01_DEBRIEF") } }
        };
        return Steps;
    }

    double Horizontal(const FVector& A, const FVector& B)
    {
        return FMath::Sqrt(FMath::Square(A.X - B.X) + FMath::Square(A.Y - B.Y));
    }
}

// =========================================================== static helpers

TArray<FString> AHavocEpisode01Flow::BuildTracker(const FString& State, bool bComplete)
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

FString AHavocEpisode01Flow::BuildSummary(const FString& Outcome, int32 CrewRecovered, int32 CrewSize, bool bAegisOnline)
{
    const TCHAR* Headline = Outcome == TEXT("SUCCESS") ? TEXT("Mountain Shield holds and the Falcon-2 crew is home.")
        : Outcome == TEXT("PARTIAL") ? TEXT("Mountain Shield holds, but the Falcon-2 crew was left behind.")
        : TEXT("Checkpoint Echo or Ridge Station was lost.");
    return FString::Printf(TEXT("%s\nFalcon-2 crew recovered: %d of %d\nAegis: %s"),
        Headline, CrewRecovered, CrewSize, bAegisOnline ? TEXT("online") : TEXT("offline"));
}

FVector2D AHavocEpisode01Flow::GetGreyboxPoint(FName SectorId)
{
    if (SectorId == StagingId) { return FVector2D(0.0, 0.0); }
    if (SectorId == EchoId) { return FVector2D(20.0, 0.0); }
    if (SectorId == RidgeId) { return FVector2D(350.0, -150.0); }
    if (SectorId == CrashId) { return FVector2D(500.0, 150.0); }
    return FVector2D::ZeroVector;
}

const TArray<FName>& AHavocEpisode01Flow::GetSectorIds()
{
    static const TArray<FName> Ids = { StagingId, EchoId, RidgeId, CrashId };
    return Ids;
}

// =========================================================== flow

AHavocEpisode01Flow::AHavocEpisode01Flow()
    : MissionId(TEXT("mission_01_mountain_shield"))
{
    PrimaryActorTick.bCanEverTick = true;
}

void AHavocEpisode01Flow::BeginPlay()
{
    Super::BeginPlay();
    FindSectors();
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Subscribe(TEXT("player:died"), this, GET_FUNCTION_NAME_CHECKED(AHavocEpisode01Flow, OnPlayerDied));
        }
    }
}

void AHavocEpisode01Flow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Unsubscribe(TEXT("player:died"), this, GET_FUNCTION_NAME_CHECKED(AHavocEpisode01Flow, OnPlayerDied));
        }
    }
    Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------- lookup and helpers

UHavocMountainShieldMission* AHavocEpisode01Flow::GetMission() const
{
    const UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    return Missions ? Missions->GetMountainShield() : nullptr;
}

AHavocPlayerController* AHavocEpisode01Flow::GetPC()
{
    if (!CachedPC)
    {
        CachedPC = Cast<AHavocPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    }
    return CachedPC;
}

AHavocPlayerCharacter* AHavocEpisode01Flow::GetMarine()
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

int32 AHavocEpisode01Flow::CountAlive(FName Tag) const
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

void AHavocEpisode01Flow::FindSectors()
{
    Sectors.Reset();
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AHavocSectorVolume> It(World); It; ++It)
        {
            Sectors.Add(It->SectorId, *It);
        }
    }
    // A built Paktia level has the Echo and Ridge Station volumes; anything else gets the greybox.
    bGreybox = !(Sectors.Contains(EchoId) && Sectors.Contains(RidgeId));
}

FVector AHavocEpisode01Flow::GroundAt(const FVector& Location) const
{
    const UWorld* World = GetWorld();
    FHitResult Hit;
    const FVector Top = Location + FVector(0.0, 0.0, 100000.0);
    const FVector Bottom = Location - FVector(0.0, 0.0, 100000.0);
    if (World && World->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_WorldStatic))
    {
        return Hit.ImpactPoint;
    }
    return Location;
}

FVector AHavocEpisode01Flow::SectorPoint(FName SectorId) const
{
    if (!bGreybox)
    {
        // The staging sector may be missing from the level; the Marine starts at Echo then.
        const TObjectPtr<AHavocSectorVolume>* Found = Sectors.Find(SectorId);
        if (!Found) { Found = Sectors.Find(EchoId); }
        if (Found && *Found)
        {
            return GroundAt((*Found)->GetActorLocation());
        }
    }
    const FVector2D P = GetGreyboxPoint(SectorId);
    return GreyboxOriginCm + FVector(P.X * 100.0, P.Y * 100.0, 0.0);
}

bool AHavocEpisode01Flow::IsNear(const FVector& A, const FVector& B, float RadiusM) const
{
    return Horizontal(A, B) <= RadiusM * 100.0;
}

void AHavocEpisode01Flow::Publish(const TCHAR* EventName, const FString& Payload) const
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Publish(EventName, Payload);
        }
    }
}

bool AHavocEpisode01Flow::IsPiloting() const
{
    const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    return Pawn && Pawn == Raven.Get();
}

FString AHavocEpisode01Flow::GetObjectiveText() const
{
    const UHavocMountainShieldMission* Mission = GetMission();
    return Mission && Mission->GetRunner() ? Mission->GetRunner()->GetObjective() : FString();
}

FString AHavocEpisode01Flow::GetTrackerText() const
{
    const UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner())
    {
        return FString();
    }
    const UHavocMissionRunner* Runner = Mission->GetRunner();
    const bool bDone = Runner->IsComplete() && Runner->GetOutcome() != TEXT("FAILED");
    return FString::Join(BuildTracker(Runner->GetState(), bDone), TEXT("\n"));
}

FString AHavocEpisode01Flow::GetStatusText() const
{
    const UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission)
    {
        return FString();
    }
    FString Text = FString::Printf(TEXT("Aegis: %s"), Mission->IsAegisOnline() ? TEXT("online") : TEXT("offline"));
    const FString State = Mission->GetState();
    if (State == TEXT("M01_HOLD_ECHO"))
    {
        Text += FString::Printf(TEXT("   Echo: %d s left"), FMath::CeilToInt(FMath::Max(0.f, EchoHoldSeconds - PhaseTimer)));
    }
    else if (State == TEXT("M01_CREW_RECOVERY"))
    {
        Text += FString::Printf(TEXT("   Crew: %d of %d home, %d carried"), Mission->GetCrewRecovered(), Mission->GetCrewSize(), CrewCarried);
    }
    else if (State == TEXT("M01_COUNTERATTACK"))
    {
        Text += FString::Printf(TEXT("   Raiders: %d   Ridge Station holds for %d s"), CountAlive(RaiderTag),
            FMath::CeilToInt(FMath::Max(0.f, CounterattackLimitSeconds - PhaseTimer)));
    }
    return Text;
}

FString AHavocEpisode01Flow::GetResultTitle() const
{
    return Outcome == TEXT("SUCCESS") ? TEXT("MOUNTAIN SHIELD HOLDS - MISSION COMPLETE")
        : Outcome == TEXT("PARTIAL") ? TEXT("MOUNTAIN SHIELD HOLDS - CREW LEFT BEHIND")
        : TEXT("RIDGE STATION LOST");
}

// ---------------------------------------------------------------- episode control

void AHavocEpisode01Flow::StartEpisode()
{
    UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    if (!Missions)
    {
        return;
    }
    if (!Missions->StartMission(MissionId))
    {
        ShowToast(TEXT("Episode 01 could not start. Check that the data folder is packaged beside the game (game/README.md)."), 10.f);
        UE_LOG(LogHavocFlow01, Error, TEXT("Mission '%s' failed to start"), *MissionId.ToString());
        return;
    }
    ClearWorld();
    FindSectors();
    if (bGreybox)
    {
        BuildGreybox();
    }
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
        if (Marine->IsDead())
        {
            Marine->Revive(Marine->GetActorTransform());
        }
        const FVector Start = SectorPoint(StagingId);
        TeleportMarine(Start, Start + FVector(100.0, 0.0, 0.0));
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
    UE_LOG(LogHavocFlow01, Log, TEXT("Episode 01 started (%s level)"), bGreybox ? TEXT("greybox") : TEXT("Paktia"));
    // Briefing -> drone tutorial (the runner stops there until the tutorial is complete).
    if (UHavocMountainShieldMission* Mission = GetMission())
    {
        Mission->Progress();
    }
}

void AHavocEpisode01Flow::RestartFromCheckpoint()
{
    if (Phase == EHavocFlowPhase::Menu)
    {
        return;
    }
    UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner() || !Mission->GetRunner()->HasCheckpoint())
    {
        StartEpisode();
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

void AHavocEpisode01Flow::ClearWorld()
{
    for (AHavocEnemyCharacter* Enemy : Enemies)
    {
        if (Enemy)
        {
            Enemy->Destroy();
        }
    }
    Enemies.Reset();
    for (AActor* Prop : TutorialProps)
    {
        if (Prop)
        {
            Prop->Destroy();
        }
    }
    TutorialProps.Reset();
    RecoverRaven(nullptr);
    PhaseTimer = 0.f;
    ObserveTimer = 0.f;
    bSecondWaveSent = false;
    CrewCarried = 0;
}

void AHavocEpisode01Flow::EndEpisode()
{
    UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner())
    {
        return;
    }
    Phase = EHavocFlowPhase::Results;
    Outcome = Mission->GetRunner()->GetOutcome();
    RecoverRaven(nullptr);

    const FHavocEpisode01Result Carry = Mission->GetCarryover();
    ResultSummary = BuildSummary(Outcome, Mission->GetCrewRecovered(), Mission->GetCrewSize(), Mission->IsAegisOnline());
    if (UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr)
    {
        Missions->SetEpisode1CrewRecovered(Carry.bCrewRecovered); // Episode 02 reads it
    }
    Publish(TEXT("episode:results"), FString::Printf(TEXT("{\"episode\":1,\"outcome\":\"%s\",\"crewRecovered\":%s}"),
        *Outcome, Carry.bCrewRecovered ? TEXT("true") : TEXT("false")));

    if (AHavocPlayerController* PC = GetPC())
    {
        PC->ShowMenu(EHavocMenuMode::Results);
    }
}

// ---------------------------------------------------------------- tick

void AHavocEpisode01Flow::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ToastTime = FMath::Max(0.f, ToastTime - DeltaSeconds);
    ResupplyCooldown = FMath::Max(0.f, ResupplyCooldown - DeltaSeconds);

    if (Phase != EHavocFlowPhase::Playing)
    {
        return;
    }
    UHavocMountainShieldMission* Mission = GetMission();
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
    TickRaven();
    TickState(Runner->GetState(), DeltaSeconds);
}

void AHavocEpisode01Flow::Step()
{
    if (UHavocMountainShieldMission* Mission = GetMission())
    {
        Mission->Progress();
    }
}

void AHavocEpisode01Flow::OnEnterState(const FString& State, bool bRestored)
{
    UHavocMountainShieldMission* Mission = GetMission();
    UHavocMissionRunner* Runner = Mission->GetRunner();
    LastState = State;
    PhaseTimer = 0.f;
    const bool bCheckpointState = Runner->HasCheckpoint() && Runner->GetCheckpoint().State == State;

    if (State != TEXT("M01_DRONE_TUTORIAL"))
    {
        for (AActor* Prop : TutorialProps)
        {
            if (Prop) { Prop->Destroy(); }
        }
        TutorialProps.Reset();
        RecoverRaven(nullptr);
    }

    const FVector Echo = SectorPoint(EchoId);
    const FVector Ridge = SectorPoint(RidgeId);
    const FVector Crash = SectorPoint(CrashId);

    if (State == TEXT("M01_DRONE_TUTORIAL"))
    {
        if (!bRestored)
        {
            ObserveTimer = 0.f;
        }
        BuildTutorialProps();
        ShowToast(TEXT("Tutorial. Follow the prompt at the top of the screen."), 6.f);
    }
    else if (State == TEXT("M01_HOLD_ECHO"))
    {
        TeleportMarine(Echo, Echo + FVector(0.0, 100.0, 0.0));
        bSecondWaveSent = false;
        SpawnEnemies(Echo + FVector(-4000.0, 3000.0, 0.0), EchoWaveSize, 15.f, RaiderTag);
        ShowToast(TEXT("Hold Checkpoint Echo. Raiders are coming. Stay inside the marked area."), 8.f);
    }
    else if (State == TEXT("M01_RIDGE_STATION"))
    {
        // Ridge Station is far from Echo: the helicopter lift is a cut, the Marine arrives next to the relay site.
        TeleportMarine(Ridge + FVector(-2000.0, 0.0, 0.0), Ridge);
        ShowToast(TEXT("Ridge Station. Walk to the relay site and press E to set it up. Aegis comes online."), 8.f);
    }
    else if (State == TEXT("M01_FALCON_DOWN"))
    {
        ShowToast(TEXT("Falcon-2 is under fire! It is going down near grid R-17."), 8.f);
    }
    else if (State == TEXT("M01_CRASH_DECISION"))
    {
        ShowToast(TEXT("Falcon-2 crashed at grid R-17. Press 1 to recover the crew (risky) or 2 to abandon them."), 12.f);
    }
    else if (State == TEXT("M01_CREW_RECOVERY"))
    {
        TeleportMarine(Crash + FVector(-3000.0, 0.0, 0.0), Crash);
        CrewCarried = 0;
        SpawnEnemies(Crash, CrashGarrison, 20.f, CrashTag);
        ShowToast(TEXT("Secure grid R-17. When it is clear press E at the wreck, then carry the crew to Ridge Station."), 10.f);
    }
    else if (State == TEXT("M01_ABANDONED"))
    {
        ShowToast(TEXT("The crew is left behind. Return to Ridge Station."), 6.f);
    }
    else if (State == TEXT("M01_COUNTERATTACK"))
    {
        if (!IsNear(GetMarine() ? GetMarine()->GetActorLocation() : Ridge, Ridge, 60.f))
        {
            TeleportMarine(Ridge + FVector(-2000.0, 0.0, 0.0), Ridge);
        }
        SpawnEnemies(Ridge + FVector(6000.0, 4000.0, 0.0), CounterattackSize, 40.f, RaiderTag);
        ShowToast(TEXT("COUNTERATTACK! Repel the raiders and keep Ridge Station."), 8.f);
    }
    else if (State == TEXT("M01_DEBRIEF"))
    {
        ShowToast(TEXT("Debrief."), 4.f);
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

void AHavocEpisode01Flow::TickState(const FString& State, float Dt)
{
    UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission)
    {
        return;
    }
    AHavocPlayerCharacter* Marine = GetMarine();
    const FVector MarinePos = Marine ? Marine->GetActorLocation() : FVector::ZeroVector;

    if (State == TEXT("M01_BRIEFING"))
    {
        Step();
    }
    else if (State == TEXT("M01_DRONE_TUTORIAL"))
    {
        TickTutorial(Dt);
    }
    else if (State == TEXT("M01_HOLD_ECHO"))
    {
        const FVector Echo = SectorPoint(EchoId);
        const bool bInside = Marine && !Marine->IsDead() && IsNear(MarinePos, Echo, EchoRadiusM);
        if (bInside)
        {
            PhaseTimer += Dt;
        }
        SetHint(bInside
            ? FString::Printf(TEXT("Hold Checkpoint Echo: %d s."), FMath::CeilToInt(FMath::Max(0.f, EchoHoldSeconds - PhaseTimer)))
            : FString(TEXT("You left Checkpoint Echo. Get back inside the area: the timer is paused.")));
        if (!bSecondWaveSent && PhaseTimer >= EchoHoldSeconds * 0.5f)
        {
            bSecondWaveSent = true;
            SpawnEnemies(Echo + FVector(5000.0, -3000.0, 0.0), EchoWaveSize + 1, 15.f, RaiderTag);
            ShowToast(TEXT("A second wave is coming from the east."), 5.f);
        }
        if (PhaseTimer >= EchoHoldSeconds)
        {
            Mission->HoldEcho();
            Step();
        }
    }
    else if (State == TEXT("M01_RIDGE_STATION"))
    {
        const double Away = Marine ? Horizontal(MarinePos, SectorPoint(RidgeId)) / 100.0 : 0.0;
        SetHint(Away <= InteractRadiusM ? FString(TEXT("Press E to set up the relay at Ridge Station."))
            : FString::Printf(TEXT("Walk to the relay site at Ridge Station (%d m)."), FMath::RoundToInt(Away)));
    }
    else if (State == TEXT("M01_FALCON_DOWN"))
    {
        PhaseTimer += Dt;
        SetHint(TEXT("Watch Falcon-2 go down."));
        if (PhaseTimer >= CrashBeatSeconds)
        {
            Mission->NotifyCrashSeen();
            Step();
        }
    }
    else if (State == TEXT("M01_CRASH_DECISION"))
    {
        SetHint(TEXT("Press 1: recover the crew (risky)   Press 2: abandon them and hold Ridge Station"));
    }
    else if (State == TEXT("M01_CREW_RECOVERY"))
    {
        const int32 Left = CountAlive(CrashTag);
        if (CrewCarried > 0)
        {
            SetHint(TEXT("Carry the crew to Ridge Station and press E to hand them over."));
        }
        else
        {
            SetHint(Left > 0 ? FString::Printf(TEXT("Secure grid R-17: %d hostiles left."), Left)
                : FString(TEXT("R-17 is clear. Press E at the wreck to pick up the crew.")));
        }
        if (Mission->GetCrewRecovered() >= Mission->GetCrewSize())
        {
            Step();
        }
    }
    else if (State == TEXT("M01_ABANDONED"))
    {
        Step();
    }
    else if (State == TEXT("M01_COUNTERATTACK"))
    {
        PhaseTimer += Dt;
        const int32 Left = CountAlive(RaiderTag);
        SetHint(FString::Printf(TEXT("Repel the counterattack: %d raiders left. Keep Ridge Station."), Left));
        if (Left == 0)
        {
            Mission->RepelCounterattack();
            Step();
        }
        else if (PhaseTimer >= CounterattackLimitSeconds)
        {
            ShowToast(TEXT("Ridge Station is overrun."), 6.f);
            Mission->LoseCheckpoint(); // fails to M01_OPERATION_FAILED; EndEpisode shows the result and the checkpoint restart
        }
    }
    else if (State == TEXT("M01_DEBRIEF"))
    {
        Mission->Finish();
    }
}

void AHavocEpisode01Flow::TickTutorial(float Dt)
{
    UHavocMountainShieldMission* Mission = GetMission();
    AHavocPlayerCharacter* Marine = GetMarine();
    const EHavocTutorialStep Current = Mission->GetTutorialStep();
    FString Prompt = UHavocMountainShieldMission::GetTutorialPrompt(Current);

    switch (Current)
    {
    case EHavocTutorialStep::Move:
        if (Marine)
        {
            const double Away = Horizontal(Marine->GetActorLocation(), MoveMarker) / 100.0;
            Prompt += FString::Printf(TEXT(" (%d m)"), FMath::RoundToInt(Away));
            if (Away <= MarkerRadiusM)
            {
                Mission->AdvanceTutorial(Current);
                ShowToast(TEXT("Good. Now the camera."), 3.f);
            }
        }
        break;
    case EHavocTutorialStep::Aim:
        if (Marine)
        {
            const FVector To = DummyPoint - Marine->GetActorLocation();
            const double Wanted = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
            const double Diff = FMath::Abs(FMath::FindDeltaAngleDegrees(Marine->GetControlRotation().Yaw, Wanted));
            if (Diff <= AimToleranceDegrees)
            {
                Mission->AdvanceTutorial(Current);
                ShowToast(TEXT("On target. Next: the Raven drone."), 3.f);
            }
        }
        break;
    case EHavocTutorialStep::LaunchRaven:
        break; // DeployRaven advances
    case EHavocTutorialStep::MoveRaven:
        if (Raven)
        {
            const double Away = Horizontal(Raven->GetActorLocation(), RavenMarker) / 100.0;
            Prompt += FString::Printf(TEXT(" (%d m)"), FMath::RoundToInt(Away));
            if (Away <= RavenMarkerRadiusM)
            {
                ObserveTimer = 0.f;
                Mission->AdvanceTutorial(Current);
            }
        }
        else
        {
            Prompt = TEXT("The Raven is gone. Press R to launch a fresh one.");
        }
        break;
    case EHavocTutorialStep::ObserveContact:
        if (Raven)
        {
            if (Horizontal(Raven->GetActorLocation(), RavenMarker) / 100.0 <= RavenMarkerRadiusM)
            {
                ObserveTimer += Dt;
                if (ObserveTimer >= ObserveSeconds)
                {
                    Mission->AdvanceTutorial(Current);
                }
            }
            else
            {
                ObserveTimer = 0.f;
                Prompt = TEXT("Stay over the ridge marker and look down at the contact.");
            }
        }
        else
        {
            Prompt = TEXT("The Raven is gone. Press R to launch a fresh one.");
        }
        break;
    case EHavocTutorialStep::ConfirmContact:
        if (!Raven)
        {
            Prompt = TEXT("The Raven is gone. Press R to launch a fresh one.");
        }
        break; // HandleInteract advances
    case EHavocTutorialStep::ReturnToMarine:
        if (!Raven)
        {
            Mission->AdvanceTutorial(Current); // the Raven came home by itself
        }
        break;
    default:
        break;
    }
    SetHint(Prompt);

    if (Mission->GetTutorialStep() == EHavocTutorialStep::Complete)
    {
        ShowToast(TEXT("Tutorial complete."), 4.f);
        Mission->Progress(); // tutorial_complete is set: on to Checkpoint Echo
    }
}

// ---------------------------------------------------------------- world

AStaticMeshActor* AHavocEpisode01Flow::SpawnProp(const TCHAR* MeshPath, const FVector& GroundLocation, const FVector& SizeMeters, TArray<TObjectPtr<AActor>>& Into)
{
    UWorld* World = GetWorld();
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
    if (!World || !Mesh)
    {
        return nullptr;
    }
    // The engine shapes are 100 cm, so the scale is the size in metres.
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Prop = World->SpawnActor<AStaticMeshActor>(GroundLocation + FVector(0.0, 0.0, SizeMeters.Z * 50.0), FRotator::ZeroRotator, Params);
    if (!Prop)
    {
        return nullptr;
    }
    Prop->SetMobility(EComponentMobility::Movable);
    Prop->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    Prop->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Prop->SetActorScale3D(SizeMeters);
    Into.Add(Prop);
    return Prop;
}

void AHavocEpisode01Flow::BuildGreybox()
{
    if (Greybox.Num() > 0)
    {
        return;
    }
    UE_LOG(LogHavocFlow01, Log, TEXT("No Paktia sector volumes in the level: building the greybox at (%s)."), *GreyboxOriginCm.ToString());
    UWorld* World = GetWorld();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (World && Cube)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // A 900 x 600 m slab whose top face is at the origin height.
        AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(GreyboxOriginCm + FVector(25000.0, 0.0, -100.0), FRotator::ZeroRotator, Params);
        if (Floor)
        {
            Floor->SetMobility(EComponentMobility::Movable);
            Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
            Floor->SetActorScale3D(FVector(900.0, 600.0, 2.0));
            Greybox.Add(Floor);
        }
    }
    // A pillar marks Echo, Ridge Station and the crash grid.
    for (const FName& Id : { EchoId, RidgeId, CrashId })
    {
        SpawnProp(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), SectorPoint(Id), FVector(3.0, 3.0, 8.0), Greybox);
    }
}

void AHavocEpisode01Flow::BuildTutorialProps()
{
    for (AActor* Prop : TutorialProps)
    {
        if (Prop) { Prop->Destroy(); }
    }
    TutorialProps.Reset();

    const FVector Origin = SectorPoint(StagingId);
    MoveMarker = GroundAt(Origin + FVector(2500.0, 0.0, 0.0));
    DummyPoint = GroundAt(Origin + FVector(2500.0, 3000.0, 0.0));
    RavenMarker = GroundAt(Origin + FVector(11000.0, 7000.0, 0.0));

    SpawnProp(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), MoveMarker, FVector(MarkerRadiusM * 2.0, MarkerRadiusM * 2.0, 0.3), TutorialProps);
    SpawnProp(TEXT("/Engine/BasicShapes/Cube.Cube"), DummyPoint, FVector(1.0, 1.0, 2.0), TutorialProps);          // target dummy
    SpawnProp(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), RavenMarker, FVector(3.0, 3.0, 15.0), TutorialProps); // ridge marker / contact
}

void AHavocEpisode01Flow::SpawnEnemies(const FVector& Centre, int32 Count, float RadiusM, FName Tag)
{
    UWorld* World = GetWorld();
    const UGameInstance* GI = GetGameInstance();
    const UHavocDataSubsystem* Data = GI ? GI->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    if (!World || !Data || Count <= 0)
    {
        UE_LOG(LogHavocFlow01, Warning, TEXT("Cannot spawn enemies for '%s' (world or data missing)"), *Tag.ToString());
        return;
    }
    const TArray<FString> Factions = { TEXT("isis"), TEXT("houthi") }; // AI-only hostiles, abstract data/units ratings
    TArray<AHavocEnemyCharacter*> Spawned;
    for (int32 i = 0; i < Count; ++i)
    {
        TArray<FHavocUnitDef> Units = Data->GetInfantryUnits(Factions[i % Factions.Num()]);
        if (Units.Num() == 0)
        {
            UE_LOG(LogHavocFlow01, Warning, TEXT("No infantry units for faction '%s' in data/units"), *Factions[i % Factions.Num()]);
            continue;
        }
        const FHavocUnitDef& Unit = Units[FMath::RandRange(0, Units.Num() - 1)];
        const double Angle = FMath::FRandRange(0.0, 2.0 * PI);
        const double Reach = FMath::Sqrt(FMath::FRand()) * RadiusM * 100.0;
        const FVector Around = Centre + FVector(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach, 0.0);
        const FVector Spot = GroundAt(Around) + FVector(0.0, 0.0, 110.0);
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
        if (AHavocAIController* AI = Cast<AHavocAIController>(Enemy->GetController()))
        {
            AI->NotifyShotBy(GetMarine()); // raiders and the crash garrison already know where the Marine is
        }
    }
    if (UHavocSquadSubsystem* Squads = World->GetSubsystem<UHavocSquadSubsystem>())
    {
        Squads->RegisterSquad(EHavocTeam::Opposing, Tag, EHavocSquadPosture::Assault, Spawned);
    }
}

// ---------------------------------------------------------------- marine

void AHavocEpisode01Flow::TeleportMarine(const FVector& GroundLocation, const FVector& FaceToward)
{
    AHavocPlayerCharacter* Marine = GetMarine();
    if (!Marine)
    {
        return;
    }
    const FVector Dir = FaceToward - GroundLocation;
    const FRotator Facing(0.f, Dir.IsNearlyZero() ? 0.f : FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f);
    Marine->TeleportTo(GroundLocation + FVector(0.0, 0.0, 120.0), Facing);
    if (AController* C = Marine->GetController())
    {
        C->SetControlRotation(Facing);
    }
}

void AHavocEpisode01Flow::PossessMarine()
{
    AHavocPlayerController* PC = GetPC();
    AHavocPlayerCharacter* Marine = GetMarine();
    if (PC && Marine && PC->GetPawn() != Marine)
    {
        PC->Possess(Marine);
    }
}

void AHavocEpisode01Flow::OnPlayerDied(FName EventName, const FString& Payload)
{
    if (Phase == EHavocFlowPhase::Playing && !bRespawnPending)
    {
        BeginRespawn();
    }
}

void AHavocEpisode01Flow::BeginRespawn()
{
    bRespawnPending = true;
    ShowToast(TEXT("Marine down. Restoring the last checkpoint..."), 4.f);
    GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        RespawnFromCheckpoint();
    }), 3.f, false);
}

void AHavocEpisode01Flow::RespawnFromCheckpoint()
{
    UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner())
    {
        bRespawnPending = false;
        return;
    }
    UHavocMissionRunner* Runner = Mission->GetRunner();
    ClearWorld();
    if (Runner->HasCheckpoint())
    {
        Runner->RestoreCheckpoint(); // state, flags, history, tutorial step, Ridge Station, crew
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

void AHavocEpisode01Flow::LaunchRaven()
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
    const FVector Base = SectorPoint(StagingId);
    const FVector Home = Base + FVector(0.0, 0.0, 3000.0); // 30 m up

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Raven = World->SpawnActor<AHavocRavenDrone>(AHavocRavenDrone::StaticClass(), FTransform(Home), Params);
    if (!Raven)
    {
        return;
    }
    // The drone links to a hidden relay marker over the staging area (the whole tutorial is in its range).
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

void AHavocEpisode01Flow::RecoverRaven(const TCHAR* Reason)
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

void AHavocEpisode01Flow::TickRaven()
{
    if (!Raven)
    {
        return;
    }
    switch (Raven->GetDroneState())
    {
    case EHavocDroneState::Recovered:
    case EHavocDroneState::SignalLost:
    case EHavocDroneState::Disabled:
    case EHavocDroneState::Destroyed:
        RecoverRaven(TEXT("Raven is down or has returned. Press R to launch a fresh one."));
        break;
    default:
        break;
    }
}

void AHavocEpisode01Flow::DeployRaven()
{
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending)
    {
        return;
    }
    UHavocMountainShieldMission* Mission = GetMission();
    if (!Mission || LastState != TEXT("M01_DRONE_TUTORIAL"))
    {
        ShowToast(TEXT("The Raven is only used in the tutorial in this mission."), 4.f);
        return;
    }
    const EHavocTutorialStep Step = Mission->GetTutorialStep();
    if (Step < EHavocTutorialStep::LaunchRaven)
    {
        ShowToast(TEXT("Finish the current step first."), 3.f);
        return;
    }
    if (Raven)
    {
        if (AHavocPlayerController* PC = GetPC())
        {
            PC->Possess(Raven);
        }
    }
    else if (Step <= EHavocTutorialStep::ConfirmContact)
    {
        LaunchRaven();
    }
    if (Raven)
    {
        Mission->AdvanceTutorial(EHavocTutorialStep::LaunchRaven); // no-op unless this is the launch step
    }
}

void AHavocEpisode01Flow::ReturnToMarine()
{
    if (Phase != EHavocFlowPhase::Playing || !IsPiloting())
    {
        return;
    }
    PossessMarine();
    UHavocMountainShieldMission* Mission = GetMission();
    if (Mission && LastState == TEXT("M01_DRONE_TUTORIAL") && Mission->GetTutorialStep() == EHavocTutorialStep::ReturnToMarine)
    {
        Mission->AdvanceTutorial(EHavocTutorialStep::ReturnToMarine);
        RecoverRaven(nullptr); // the Raven is no longer needed
        return;
    }
    ShowToast(TEXT("Back on the Marine. R flies the Raven again."), 3.f);
}

void AHavocEpisode01Flow::CallFalcon()
{
    if (Phase == EHavocFlowPhase::Playing)
    {
        ShowToast(TEXT("No Falcon support in this mission: Falcon-2 is the one that went down."), 4.f);
    }
}

// ---------------------------------------------------------------- player actions

void AHavocEpisode01Flow::ChooseBranch(bool bFirst)
{
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending)
    {
        return;
    }
    if (LastState != TEXT("M01_CRASH_DECISION"))
    {
        ShowToast(TEXT("The crew decision comes after Falcon-2 goes down."), 3.f);
        return;
    }
    if (UHavocMountainShieldMission* Mission = GetMission())
    {
        // The runner moves on; Tick sees the new state and OnEnterState sets the scene.
        Mission->ChooseCrew(bFirst);
    }
}

void AHavocEpisode01Flow::HandleInteract()
{
    UHavocMountainShieldMission* Mission = GetMission();
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending || !Mission)
    {
        return;
    }
    if (IsPiloting())
    {
        if (LastState == TEXT("M01_DRONE_TUTORIAL") && Mission->GetTutorialStep() == EHavocTutorialStep::ConfirmContact)
        {
            Mission->AdvanceTutorial(EHavocTutorialStep::ConfirmContact);
            ShowToast(TEXT("Contact marked."), 3.f);
        }
        return;
    }
    // On foot.
    AHavocPlayerCharacter* Marine = GetMarine();
    if (!Marine || Marine->IsDead())
    {
        return;
    }
    const FVector Here = Marine->GetActorLocation();

    if (LastState == TEXT("M01_RIDGE_STATION"))
    {
        const double Away = Horizontal(Here, SectorPoint(RidgeId)) / 100.0;
        if (Away > InteractRadiusM)
        {
            ShowToast(FString::Printf(TEXT("Ridge Station is %d m away."), FMath::RoundToInt(Away)), 3.f);
        }
        else if (Mission->SetUpRidgeStation())
        {
            ShowToast(TEXT("Relay set up. Ridge Station is captured and Aegis is online."), 6.f);
        }
        return;
    }
    if (LastState == TEXT("M01_CREW_RECOVERY"))
    {
        const double AtCrash = Horizontal(Here, SectorPoint(CrashId)) / 100.0;
        const double AtRidge = Horizontal(Here, SectorPoint(RidgeId)) / 100.0;
        if (CrewCarried > 0 && AtRidge <= InteractRadiusM)
        {
            for (int32 i = 0; i < CrewCarried; ++i)
            {
                Mission->RecoverCrewMember();
            }
            CrewCarried = 0;
            ShowToast(FString::Printf(TEXT("Crew handed over: %d of %d home."), Mission->GetCrewRecovered(), Mission->GetCrewSize()), 5.f);
            return;
        }
        if (CrewCarried == 0 && AtCrash <= InteractRadiusM)
        {
            if (CountAlive(CrashTag) > 0)
            {
                ShowToast(TEXT("Clear the crash site first."), 3.f);
            }
            else
            {
                CrewCarried = Mission->GetCrewSize() - Mission->GetCrewRecovered();
                ShowToast(TEXT("Crew picked up. Carry them to Ridge Station and press E."), 6.f);
            }
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
