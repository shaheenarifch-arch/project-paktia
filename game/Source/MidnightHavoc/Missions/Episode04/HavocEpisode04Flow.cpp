#include "Missions/Episode04/HavocEpisode04Flow.h"

#include "AI/HavocAIController.h"
#include "AI/HavocEnemyCharacter.h"
#include "AI/HavocSquadSubsystem.h"
#include "Aircraft/HavocFalconHelicopter.h"
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
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Input/HavocPlayInput.h"
#include "Kismet/GameplayStatics.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Missions/Episode04/HavocOpenStraitMission.h"
#include "Missions/Episode04/HavocShippingLanes.h"
#include "Missions/Episode04/HavocTankerEscort.h"
#include "Player/HavocPlayerCharacter.h"
#include "Player/HavocPlayerController.h"
#include "TimerManager.h"
#include "World/HavocLaneHazard.h"
#include "World/HavocSectorVolume.h"
#include "World/HavocTankerSpline.h"

DEFINE_LOG_CATEGORY_STATIC(LogHavocFlow, Log, All);

namespace
{
    const FName NorthBeach(TEXT("kharg_north_beach"));
    const FName WestBeach(TEXT("kharg_west_beach"));
    const FName Airstrip(TEXT("kharg_airstrip"));
    const FName OilTerminal(TEXT("kharg_oil_terminal"));
    const FName Counterattack(TEXT("counterattack"));

    bool IsXYInside(const FBox& Box, const FVector& P)
    {
        return P.X >= Box.Min.X && P.X <= Box.Max.X && P.Y >= Box.Min.Y && P.Y <= Box.Max.Y;
    }

    /** Attaches a visual for an asset role (skeletal or static mesh) to Parent. Returns the component, or null if the role has no mesh. */
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
        if (UStaticMesh* Static = Cast<UStaticMesh>(Asset))
        {
            UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
            Comp->SetStaticMesh(Static);
            Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Comp->SetupAttachment(Parent);
            Comp->RegisterComponent();
            return Comp;
        }
        return nullptr;
    }
}

// =========================================================== tanker vessel

AHavocTankerVessel::AHavocTankerVessel()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

bool AHavocTankerVessel::Advance(float DeltaSeconds, float SpeedCmPerSec)
{
    if (!Route)
    {
        return true;
    }
    DistanceCm += SpeedCmPerSec * DeltaSeconds;
    const float Length = Route->GetRouteLength();
    const FVector Here = Route->GetLocationAtDistance(FMath::Min(DistanceCm, Length));
    const FVector Ahead = Route->GetLocationAtDistance(FMath::Min(DistanceCm + 500.f, Length));
    SetActorLocation(Here);
    if (!(Ahead - Here).IsNearlyZero())
    {
        SetActorRotation((Ahead - Here).GetSafeNormal2D().Rotation());
    }
    return DistanceCm >= Length;
}

// =========================================================== flow

AHavocEpisode04Flow::AHavocEpisode04Flow()
    : MissionId(TEXT("episode_04_open_strait"))
{
    PrimaryActorTick.bCanEverTick = true;
}

void AHavocEpisode04Flow::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AHavocSectorVolume> It(GetWorld()); It; ++It)
    {
        Sectors.Add(It->SectorId, *It);
    }
    if (Sectors.Num() == 0)
    {
        UE_LOG(LogHavocFlow, Error, TEXT("No AHavocSectorVolume in the level. Build the Kharg level first (game/docs/kharg-level-guide.md); Episode 04 cannot progress without sectors."));
    }
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Subscribe(TEXT("player:died"), this, GET_FUNCTION_NAME_CHECKED(AHavocEpisode04Flow, OnPlayerDied));
        }
    }
}

void AHavocEpisode04Flow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Unsubscribe(TEXT("player:died"), this, GET_FUNCTION_NAME_CHECKED(AHavocEpisode04Flow, OnPlayerDied));
        }
    }
    Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------- lookup

UHavocOpenStraitMission* AHavocEpisode04Flow::GetMission() const
{
    const UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    return Missions ? Missions->GetOpenStrait() : nullptr;
}

AHavocPlayerController* AHavocEpisode04Flow::GetPC()
{
    if (!CachedPC)
    {
        CachedPC = Cast<AHavocPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    }
    return CachedPC;
}

AHavocPlayerCharacter* AHavocEpisode04Flow::GetMarine()
{
    if (!CachedMarine)
    {
        if (AHavocPlayerController* PC = GetPC())
        {
            CachedMarine = Cast<AHavocPlayerCharacter>(PC->GetPawn());
        }
    }
    return CachedMarine;
}

AHavocSectorVolume* AHavocEpisode04Flow::FindSector(FName SectorId)
{
    const TObjectPtr<AHavocSectorVolume>* Found = Sectors.Find(SectorId);
    return Found ? Found->Get() : nullptr;
}

bool AHavocEpisode04Flow::IsInside(FName SectorId, const FVector& Location)
{
    const AHavocSectorVolume* Volume = FindSector(SectorId);
    return Volume && IsXYInside(Volume->GetComponentsBoundingBox(true), Location);
}

int32 AHavocEpisode04Flow::CountAlive(FName SectorId) const
{
    int32 N = 0;
    for (const AHavocEnemyCharacter* Enemy : Enemies)
    {
        if (Enemy && !Enemy->IsDead() && Enemy->SectorId == SectorId)
        {
            ++N;
        }
    }
    return N;
}

int32 AHavocEpisode04Flow::GetGarrisonFor(FName SectorId) const
{
    if (SectorId == NorthBeach) { return NorthDefenders; }
    if (SectorId == WestBeach) { return WestDefenders; }
    if (SectorId == Airstrip) { return AirstripGarrison; }
    if (SectorId == OilTerminal) { return TerminalGarrison; }
    return 0;
}

bool AHavocEpisode04Flow::IsPiloting() const
{
    const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    return Pawn && (Pawn == Raven.Get() || Pawn == Falcon.Get());
}

void AHavocEpisode04Flow::Publish(const TCHAR* EventName, const FString& Payload) const
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Publish(EventName, Payload);
        }
    }
}

FString AHavocEpisode04Flow::GetResultTitle() const
{
    return Outcome == TEXT("SUCCESS") ? TEXT("STRAIT OPEN - MISSION COMPLETE")
        : Outcome == TEXT("PARTIAL") ? TEXT("STRAIT OPEN - PARTIAL SUCCESS")
        : TEXT("OPERATION FAILED");
}

FString AHavocEpisode04Flow::GetSupportStatus() const
{
    const UHavocOpenStraitMission* Mission = GetMission();
    if (!Mission || !Mission->HasAirSupport())
    {
        return TEXT("Falcon: not available (secure the airstrip)");
    }
    if (SupportRemaining > 0.f)
    {
        return FString::Printf(TEXT("Falcon: on station %ds"), FMath::CeilToInt(SupportRemaining));
    }
    if (SupportCooldown > 0.f)
    {
        return FString::Printf(TEXT("Falcon: rearming %ds"), FMath::CeilToInt(SupportCooldown));
    }
    return TEXT("Falcon: ready (G)");
}

// ---------------------------------------------------------------- episode control

void AHavocEpisode04Flow::StartEpisode()
{
    UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    if (!Missions || !Missions->StartMission(MissionId))
    {
        ShowToast(TEXT("Episode 04 could not start. Check that the data folder is packaged beside the game (game/README.md)."), 10.f);
        UE_LOG(LogHavocFlow, Error, TEXT("Mission '%s' failed to start"), *MissionId.ToString());
        return;
    }
    ClearWorld();
    Phase = EHavocFlowPhase::Playing;
    LastState.Empty();
    LastCheckpointState.Empty();
    DeathPenalty = 0;
    bRespawnPending = false;
    ResupplyLeft = ResupplyCharges;
    SupportRemaining = 0.f;
    SupportCooldown = 0.f;
    ResupplyCooldown = 0.f;
    Outcome.Empty();
    ResultSummary.Empty();

    if (AHavocPlayerCharacter* Marine = GetMarine())
    {
        InitialMarineTransform = Marine->GetActorTransform();
        CheckpointTransform = InitialMarineTransform;
        bHaveInitialTransform = true;
        if (Marine->IsDead())
        {
            Marine->Revive(InitialMarineTransform);
        }
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
    // Briefing -> beach recon (the runner stops there until the beaches are scouted).
    if (UHavocOpenStraitMission* Mission = GetMission())
    {
        Mission->Progress();
    }
}

void AHavocEpisode04Flow::RestartFromCheckpoint()
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
    RespawnFromCheckpoint(false);
}

void AHavocEpisode04Flow::ClearWorld()
{
    for (AHavocEnemyCharacter* Enemy : Enemies)
    {
        if (Enemy)
        {
            Enemy->Destroy();
        }
    }
    Enemies.Reset();
    for (AHavocTankerVessel* Vessel : Vessels)
    {
        if (Vessel)
        {
            Vessel->Destroy();
        }
    }
    Vessels.Reset();
    PendingClearance.Reset();
    PendingHazards.Reset();
    ThreatCountdown = -1.f;
    RecoverRaven(nullptr);
    EndFalconSupport();
}

void AHavocEpisode04Flow::EndEpisode()
{
    UHavocOpenStraitMission* Mission = GetMission();
    if (!Mission)
    {
        return;
    }
    Phase = EHavocFlowPhase::Results;
    Outcome = Mission->GetRunner()->GetOutcome();
    RecoverRaven(nullptr);
    EndFalconSupport();

    const FHavocEpisode04Result R = Mission->GetResult();
    ResultSummary = FString::Printf(
        TEXT("Terminal integrity: %d%%\nTankers through: %d   lost: %d\nSupply flow: %d%%\nReinforcements left: %d"),
        FMath::RoundToInt(R.TerminalIntegrity * 100.0), R.TankersThrough, R.TankersLost,
        FMath::RoundToInt(R.SupplyFlow * 100.0), Mission->GetReinforcements());
    Publish(TEXT("episode:results"), Mission->GetSummaryJson());

    if (AHavocPlayerController* PC = GetPC())
    {
        PC->ShowMenu(EHavocMenuMode::Results);
    }
}

// ---------------------------------------------------------------- tick

void AHavocEpisode04Flow::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ToastTime = FMath::Max(0.f, ToastTime - DeltaSeconds);

    if (Phase == EHavocFlowPhase::Menu)
    {
        if (!bMenuShown)
        {
            if (AHavocPlayerController* PC = GetPC())
            {
                PC->ShowMenu(EHavocMenuMode::Main);
                bMenuShown = true;
            }
        }
        return;
    }
    if (Phase != EHavocFlowPhase::Playing)
    {
        return;
    }

    UHavocOpenStraitMission* Mission = GetMission();
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

    TickFalcon(DeltaSeconds);
    TickRaven(DeltaSeconds);
    TickState(State, DeltaSeconds);
}

void AHavocEpisode04Flow::OnEnterState(const FString& State, bool bRestored)
{
    UHavocOpenStraitMission* Mission = GetMission();
    UHavocMissionRunner* Runner = Mission->GetRunner();
    LastState = State;

    AHavocPlayerCharacter* Marine = GetMarine();
    const bool bCheckpointState = Runner->HasCheckpoint() && Runner->GetCheckpoint().State == State;
    const bool bLanding = State.EndsWith(TEXT("_LANDING"), ESearchCase::CaseSensitive);
    if (bCheckpointState && Marine && !bLanding && !bRestored)
    {
        CheckpointTransform = Marine->GetActorTransform();
    }

    if (State == TEXT("E04_BEACH_RECON"))
    {
        bNorthSeen = bWestSeen = bScoutedDone = false;
        if (CountAlive(NorthBeach) + CountAlive(WestBeach) == 0)
        {
            SpawnGarrison(NorthBeach, NorthDefenders);
            SpawnGarrison(WestBeach, WestDefenders);
        }
        LaunchRaven(ForwardLaunchPoint(), false);
        ShowToast(TEXT("Raven is airborne. Fly over both landing beaches to mark the defences."));
    }
    else if (State == TEXT("E04_CHOOSE_BEACH"))
    {
        RecoverRaven(nullptr);
        ShowToast(TEXT("Choose the landing beach."));
    }
    else if (bLanding)
    {
        const FName Beach = State.Contains(TEXT("NORTH")) ? NorthBeach : WestBeach;
        if (CountAlive(Beach) == 0)
        {
            SpawnGarrison(Beach, GetGarrisonFor(Beach)); // after a checkpoint restore the defenders are respawned
        }
        TeleportMarineToBeach(Beach);
        ShowToast(TEXT("Marines are ashore. Clear the beach defenders."));
    }
    else if (State.Contains(TEXT("TERMINAL")))
    {
        if (CountAlive(OilTerminal) == 0)
        {
            SpawnGarrison(OilTerminal, TerminalGarrison);
        }
        ShowToast(TEXT("Secure the oil terminal. Stray fire damages the jetties, tanks and pumps."));
    }
    else if (State.Contains(TEXT("AIRSTRIP")))
    {
        if (CountAlive(Airstrip) == 0)
        {
            SpawnGarrison(Airstrip, AirstripGarrison);
        }
        ShowToast(TEXT("Take the airstrip so Falcon helicopters can operate from the island."));
    }
    else if (State == TEXT("E04_ISLAND_SECURED"))
    {
        HoldRemaining = HoldSeconds;
        WaveTimer = WaveIntervalSeconds * 0.5f;
        ShowToast(TEXT("Island secured. Hold the oil terminal while the fleet moves to the strait."));
    }
    else if (State == TEXT("E04_LANE_CLEARANCE"))
    {
        for (TActorIterator<AHavocLaneHazard> It(GetWorld()); It; ++It)
        {
            It->ResetForCheckpoint();
        }
        PendingClearance.Reset();
        PendingHazards.Reset();
        // Withdraw leftover counter-attackers.
        for (AHavocEnemyCharacter* Enemy : Enemies)
        {
            if (Enemy && Enemy->SectorId == Counterattack)
            {
                Enemy->Destroy();
            }
        }
        LaunchRaven(LaneLaunchPoint(), true);
        ShowToast(TEXT("Fly Raven along both shipping lanes. Hazards are revealed as you pass."));
    }
    else if (State == TEXT("E04_TANKER_ESCORT"))
    {
        RecoverRaven(nullptr);
        NextTankerIndex = 0;
        LaunchTimer = 3.f;
        ThreatTimer = ThreatIntervalSeconds;
        ThreatCountdown = -1.f;
        ShowToast(TEXT("Lanes are open. The tankers are moving; protect them."));
    }
}

void AHavocEpisode04Flow::Step()
{
    if (UHavocOpenStraitMission* Mission = GetMission())
    {
        Mission->GetRunner()->Advance();
    }
}

void AHavocEpisode04Flow::TickState(const FString& State, float Dt)
{
    UHavocOpenStraitMission* Mission = GetMission();
    const bool bLanding = State.EndsWith(TEXT("_LANDING"), ESearchCase::CaseSensitive);

    if (State == TEXT("E04_BEACH_RECON"))
    {
        TickRecon();
    }
    else if (State == TEXT("E04_CHOOSE_BEACH"))
    {
        SetHint(FString::Printf(TEXT("Press 1: NORTH beach (%d defenders, close to the terminal)   Press 2: WEST beach (%d defenders, airstrip first)"),
            CountAlive(NorthBeach), CountAlive(WestBeach)));
    }
    else if (bLanding)
    {
        const FName Beach = State.Contains(TEXT("NORTH")) ? NorthBeach : WestBeach;
        const int32 Left = CountAlive(Beach);
        SetHint(FString::Printf(TEXT("Clear the beach: %d hostiles left"), Left));
        if (Left == 0)
        {
            Mission->SecureBeachhead();
            Step();
            ShowToast(TEXT("Beachhead secure."));
        }
    }
    else if (State.Contains(TEXT("TERMINAL")))
    {
        const int32 Left = CountAlive(OilTerminal);
        SetHint(FString::Printf(TEXT("Secure the oil terminal: %d hostiles left. Integrity %d%%. Watch your fire."),
            Left, FMath::RoundToInt(100.0 * Mission->GetResult().TerminalIntegrity)));
        if (Left == 0)
        {
            Mission->SecureTerminal();
            Step();
            ShowToast(TEXT("Oil terminal secured."));
        }
    }
    else if (State.Contains(TEXT("AIRSTRIP")))
    {
        const int32 Left = CountAlive(Airstrip);
        SetHint(FString::Printf(TEXT("Take the airstrip: %d hostiles left"), Left));
        if (Left == 0)
        {
            Mission->SecureAirstrip();
            Step();
            ShowToast(TEXT("Airstrip secured. Falcon support is now available."));
        }
    }
    else if (State == TEXT("E04_ISLAND_SECURED"))
    {
        AHavocPlayerCharacter* Marine = GetMarine();
        const bool bAtTerminal = Marine && IsInside(OilTerminal, Marine->GetActorLocation());
        if (bAtTerminal)
        {
            HoldRemaining -= Dt;
            SetHint(FString::Printf(TEXT("Hold the oil terminal: %d s. Counter-attacks incoming."), FMath::CeilToInt(FMath::Max(0.f, HoldRemaining))));
        }
        else
        {
            SetHint(TEXT("Return to the oil terminal and hold it!"));
        }
        WaveTimer -= Dt;
        if (WaveTimer <= 0.f && HoldRemaining > 0.f)
        {
            WaveTimer = WaveIntervalSeconds;
            const TArray<FString> Raiders = { TEXT("isis"), TEXT("houthi") };
            SpawnGarrison(OilTerminal, WaveSize, Counterattack, 4000.f, &Raiders);
            ShowToast(TEXT("Counter-attack on the terminal!"));
        }
        if (HoldRemaining <= 0.f)
        {
            Mission->Progress(); // -> lane clearance (seeds the lanes)
        }
    }
    else if (State == TEXT("E04_LANE_CLEARANCE"))
    {
        TickClearance(Dt);
    }
    else if (State == TEXT("E04_TANKER_ESCORT"))
    {
        TickEscort(Dt);
    }
    else if (State == TEXT("E04_DEBRIEF"))
    {
        Mission->Finish();
    }
    else if (State == TEXT("E04_BRIEFING"))
    {
        Mission->Progress();
    }
}

// ---------------------------------------------------------------- enemies

TArray<FString> AHavocEpisode04Flow::FactionsFor(FName SectorId) const
{
    if (SectorId == NorthBeach) { return { TEXT("iran") }; }
    if (SectorId == WestBeach) { return { TEXT("houthi"), TEXT("isis") }; } // ISIS cell is AI-only
    return { TEXT("iran"), TEXT("houthi") };
}

void AHavocEpisode04Flow::SpawnGarrison(FName SectorId, int32 Count, FName Tag, float MinDistFromMarine, const TArray<FString>* FactionsOverride)
{
    UWorld* World = GetWorld();
    const AHavocSectorVolume* Volume = FindSector(SectorId);
    const UGameInstance* GI = GetGameInstance();
    const UHavocDataSubsystem* Data = GI ? GI->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    if (!World || !Volume || !Data || Count <= 0)
    {
        UE_LOG(LogHavocFlow, Warning, TEXT("Cannot spawn a garrison for '%s' (sector volume or data missing)"), *SectorId.ToString());
        return;
    }
    const TArray<FString> Factions = FactionsOverride ? *FactionsOverride : FactionsFor(SectorId);
    const FBox Box = Volume->GetComponentsBoundingBox(true);
    const FVector MarineLoc = GetMarine() ? GetMarine()->GetActorLocation() : FVector::ZeroVector;
    TArray<AHavocEnemyCharacter*> Spawned;

    for (int32 i = 0; i < Count; ++i)
    {
        TArray<FHavocUnitDef> Units = Data->GetInfantryUnits(Factions[i % Factions.Num()]);
        if (Units.Num() == 0)
        {
            UE_LOG(LogHavocFlow, Warning, TEXT("No infantry units for faction '%s' in data/units"), *Factions[i % Factions.Num()]);
            continue;
        }
        const FHavocUnitDef& Unit = Units[FMath::RandRange(0, Units.Num() - 1)];

        bool bPlaced = false;
        FVector Spot = FVector::ZeroVector;
        for (int32 Attempt = 0; Attempt < 12 && !bPlaced; ++Attempt)
        {
            const float X = FMath::FRandRange(Box.Min.X + Box.GetExtent().X * 0.2f, Box.Max.X - Box.GetExtent().X * 0.2f);
            const float Y = FMath::FRandRange(Box.Min.Y + Box.GetExtent().Y * 0.2f, Box.Max.Y - Box.GetExtent().Y * 0.2f);
            FHitResult Hit;
            const FVector Top(X, Y, Box.Max.Z + 30000.f);
            const FVector Bottom(X, Y, Box.Min.Z - 30000.f);
            if (!World->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_WorldStatic))
            {
                continue;
            }
            // Keep units on land: skip the water surface and steep ground.
            if (Hit.GetActor() && Hit.GetActor()->GetClass()->GetName().Contains(TEXT("Water")))
            {
                continue;
            }
            if (Hit.ImpactNormal.Z < 0.6f || Hit.ImpactPoint.Z < 50.f)
            {
                continue;
            }
            if (MinDistFromMarine > 0.f && FVector::Dist2D(Hit.ImpactPoint, MarineLoc) < MinDistFromMarine)
            {
                continue;
            }
            Spot = Hit.ImpactPoint + FVector(0.f, 0.f, 110.f);
            bPlaced = true;
        }
        if (!bPlaced)
        {
            UE_LOG(LogHavocFlow, Warning, TEXT("No land found in sector '%s' for %s (is the landscape built?)"), *SectorId.ToString(), *Unit.Id);
            continue;
        }

        const FTransform Where(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Spot);
        AHavocEnemyCharacter* Enemy = World->SpawnActorDeferred<AHavocEnemyCharacter>(
            AHavocEnemyCharacter::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Enemy)
        {
            continue;
        }
        Enemy->ApplyUnitDef(Unit, Tag.IsNone() ? SectorId : Tag);
        UGameplayStatics::FinishSpawningActor(Enemy, Where);
        Enemies.Add(Enemy);
        Spawned.Add(Enemy);

        if (!Tag.IsNone())
        {
            // Raiders head straight for the player.
            if (AHavocAIController* AI = Cast<AHavocAIController>(Enemy->GetController()))
            {
                AI->NotifyShotBy(GetMarine());
            }
        }
    }

    // The garrison is one squad: it shares threats and runs the patrol / engage / fallback cycle.
    // Raiders always press the attack (Assault); garrisons can fall back when weak.
    if (UHavocSquadSubsystem* Squads = World->GetSubsystem<UHavocSquadSubsystem>())
    {
        Squads->RegisterSquad(EHavocTeam::Opposing, Tag.IsNone() ? SectorId : Tag, Tag.IsNone() ? EHavocSquadPosture::Patrol : EHavocSquadPosture::Assault, Spawned);
    }
}

// ---------------------------------------------------------------- marine

void AHavocEpisode04Flow::TeleportMarineToBeach(FName BeachSectorId)
{
    AHavocPlayerCharacter* Marine = GetMarine();
    const AHavocSectorVolume* Volume = FindSector(BeachSectorId);
    if (!Marine || !Volume)
    {
        return;
    }
    const FVector Target = Volume->GetActorLocation();
    const APlayerStart* Best = nullptr;
    float BestDist = TNumericLimits<float>::Max();
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        const float D = FVector::DistSquared2D(It->GetActorLocation(), Target);
        if (D < BestDist)
        {
            BestDist = D;
            Best = *It;
        }
    }
    const FTransform Where = Best ? Best->GetActorTransform() : FTransform(Target + FVector(0.f, 0.f, 200.f));
    Marine->TeleportTo(Where.GetLocation() + FVector(0.f, 0.f, 100.f), Where.Rotator());
    if (AController* C = Marine->GetController())
    {
        C->SetControlRotation(Where.Rotator());
    }
    CheckpointTransform = Marine->GetActorTransform();
}

void AHavocEpisode04Flow::PossessMarine()
{
    AHavocPlayerController* PC = GetPC();
    AHavocPlayerCharacter* Marine = GetMarine();
    if (PC && Marine && PC->GetPawn() != Marine)
    {
        PC->Possess(Marine);
    }
}

void AHavocEpisode04Flow::OnPlayerDied(FName EventName, const FString& Payload)
{
    if (Phase == EHavocFlowPhase::Playing && !bRespawnPending)
    {
        BeginRespawn();
    }
}

void AHavocEpisode04Flow::BeginRespawn()
{
    bRespawnPending = true;
    ShowToast(TEXT("Marine down. Restoring the last checkpoint (costs reinforcements)..."), 4.f);
    GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        RespawnFromCheckpoint(true);
    }), 3.f, false);
}

void AHavocEpisode04Flow::RespawnFromCheckpoint(bool bPenalty)
{
    UHavocOpenStraitMission* Mission = GetMission();
    if (!Mission)
    {
        bRespawnPending = false;
        return;
    }
    UHavocMissionRunner* Runner = Mission->GetRunner();
    ClearWorld();
    if (bPenalty)
    {
        DeathPenalty += DeathReinforcementCost;
    }
    if (Runner->HasCheckpoint())
    {
        Runner->RestoreCheckpoint();
    }
    if (bPenalty && DeathPenalty > 0)
    {
        Mission->SpendReinforcements(DeathPenalty); // zero before the terminal is secure fails the operation
    }

    if (AHavocPlayerCharacter* Marine = GetMarine())
    {
        Marine->Revive(bHaveInitialTransform ? CheckpointTransform : Marine->GetActorTransform());
        PossessMarine();
    }
    bRespawnPending = false;
    if (!Runner->IsComplete())
    {
        OnEnterState(Runner->GetState(), true);
    }
}

// ---------------------------------------------------------------- Raven

FVector AHavocEpisode04Flow::ForwardLaunchPoint() const
{
    const TObjectPtr<AHavocSectorVolume>* N = Sectors.Find(NorthBeach);
    const TObjectPtr<AHavocSectorVolume>* W = Sectors.Find(WestBeach);
    if (!N || !W)
    {
        return FVector::ZeroVector;
    }
    const FBox A = (*N)->GetComponentsBoundingBox(true);
    const FBox B = (*W)->GetComponentsBoundingBox(true);
    const FVector CA = A.GetCenter();
    const FVector CB = B.GetCenter();
    // Nearest points of the two beach boxes: the launch point sits between them so both are within link range.
    const FVector PA(FMath::Clamp(CB.X, A.Min.X, A.Max.X), FMath::Clamp(CB.Y, A.Min.Y, A.Max.Y), 0.f);
    const FVector PB(FMath::Clamp(CA.X, B.Min.X, B.Max.X), FMath::Clamp(CA.Y, B.Min.Y, B.Max.Y), 0.f);
    FVector Mid = (PA + PB) * 0.5f;
    Mid.Z = 3000.f;
    return Mid;
}

FVector AHavocEpisode04Flow::LaneLaunchPoint() const
{
    FVector Sum = FVector::ZeroVector;
    int32 N = 0;
    for (TActorIterator<AHavocLaneHazard> It(GetWorld()); It; ++It)
    {
        Sum += It->GetActorLocation();
        ++N;
    }
    FVector Centre = N > 0 ? Sum / N : FVector::ZeroVector;
    Centre.Z = 3000.f;
    return Centre;
}

void AHavocEpisode04Flow::LaunchRaven(const FVector& Home, bool bFollowRelay)
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

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Raven = World->SpawnActor<AHavocRavenDrone>(AHavocRavenDrone::StaticClass(), FTransform(Home), Params);
    if (!Raven)
    {
        return;
    }
    AttachVisual(Raven, Raven->Mesh, TEXT("Drone.Raven"));
    Raven->Core.Definition.MaxSpeed *= RavenSpeedScale;

    bRelayFollows = bFollowRelay;
    if (bFollowRelay)
    {
        // A fleet relay keeps the link while the Raven sweeps the lanes (abstract; the drone's signal rules still run).
        AStaticMeshActor* RelayActor = World->SpawnActor<AStaticMeshActor>(Home, FRotator::ZeroRotator, Params);
        if (RelayActor)
        {
            RelayActor->SetMobility(EComponentMobility::Movable);
            RelayActor->SetActorHiddenInGame(true);
            Relay = RelayActor;
            Raven->RelayActor = Relay;
        }
    }
    Raven->Launch(Home);
    Raven->SetCameraActive(true);
    PC->Possess(Raven);
}

void AHavocEpisode04Flow::RecoverRaven(const TCHAR* Reason)
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
    if (Relay)
    {
        Relay->Destroy();
        Relay = nullptr;
    }
    if (Reason)
    {
        ShowToast(Reason);
    }
}

void AHavocEpisode04Flow::DeployRaven()
{
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending)
    {
        return;
    }
    if (LastState == TEXT("E04_BEACH_RECON") || LastState == TEXT("E04_LANE_CLEARANCE"))
    {
        if (Raven)
        {
            if (AHavocPlayerController* PC = GetPC())
            {
                PC->Possess(Raven);
            }
            return;
        }
        LaunchRaven(LastState == TEXT("E04_BEACH_RECON") ? ForwardLaunchPoint() : LaneLaunchPoint(), LastState == TEXT("E04_LANE_CLEARANCE"));
    }
    else
    {
        ShowToast(TEXT("Raven is only used for the beach recon and the lane clearance."));
    }
}

void AHavocEpisode04Flow::ReturnToMarine()
{
    if (Phase == EHavocFlowPhase::Playing && IsPiloting())
    {
        PossessMarine();
        ShowToast(TEXT("Back on the Marine. T flies the Raven again."), 3.f);
    }
}

void AHavocEpisode04Flow::TickRaven(float Dt)
{
    if (!Raven)
    {
        return;
    }
    if (bRelayFollows && Relay)
    {
        const FVector Target = Raven->GetActorLocation();
        const FVector Now = Relay->GetActorLocation();
        const FVector Delta = Target - Now;
        const float Step = 3500.f * Dt; // the relay ship keeps pace with the drone
        Relay->SetActorLocation(Delta.Size() <= Step ? Target : Now + Delta.GetSafeNormal() * Step);
    }
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

void AHavocEpisode04Flow::TickRecon()
{
    UHavocOpenStraitMission* Mission = GetMission();
    if (bScoutedDone)
    {
        return;
    }
    if (Raven)
    {
        const FVector Where = Raven->GetActorLocation();
        bNorthSeen = bNorthSeen || IsInside(NorthBeach, Where);
        bWestSeen = bWestSeen || IsInside(WestBeach, Where);
    }
    SetHint(FString::Printf(TEXT("Raven recon: fly over both beaches (W/A/S/D, Space up, Ctrl down).  North: %s   West: %s.  T = Raven, Tab = back to Marine"),
        bNorthSeen ? TEXT("marked") : TEXT("not yet"), bWestSeen ? TEXT("marked") : TEXT("not yet")));
    if (bNorthSeen && bWestSeen)
    {
        const int32 N = CountAlive(NorthBeach);
        const int32 W = CountAlive(WestBeach);
        if (Mission->ScoutBeaches(N, W))
        {
            bScoutedDone = true;
            ShowToast(FString::Printf(TEXT("Recon complete: north beach %d defenders, west beach %d."), N, W), 8.f);
            Mission->Progress(); // -> choose beach
        }
    }
}

void AHavocEpisode04Flow::ChooseBeach(bool bNorth)
{
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending)
    {
        return;
    }
    if (LastState != TEXT("E04_CHOOSE_BEACH"))
    {
        ShowToast(TEXT("Pick a landing beach after the Raven has scouted both beaches."), 3.f);
        return;
    }
    if (UHavocOpenStraitMission* Mission = GetMission())
    {
        // The mission runner moves to *_LANDING; Tick sees the new state and OnEnterState lands the Marines.
        Mission->ChooseBeach(bNorth ? TEXT("north_beach") : TEXT("west_beach"));
    }
}

void AHavocEpisode04Flow::HandleInteract()
{
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending)
    {
        return;
    }
    if (Falcon && GetPC() && GetPC()->GetPawn() == Falcon.Get())
    {
        return; // E is yaw on the Falcon
    }
    if (Raven && GetPC() && GetPC()->GetPawn() == Raven.Get())
    {
        if (LastState != TEXT("E04_LANE_CLEARANCE"))
        {
            return;
        }
        AHavocLaneHazard* Best = nullptr;
        float BestDist = 45000.f; // the Raven's sensor range, in cm
        for (TActorIterator<AHavocLaneHazard> It(GetWorld()); It; ++It)
        {
            if (!It->IsRevealed() || It->IsCleared() || PendingClearance.Contains(It->HazardId))
            {
                continue;
            }
            const float D = FVector::Dist(It->GetActorLocation(), Raven->GetActorLocation());
            if (D < BestDist)
            {
                BestDist = D;
                Best = *It;
            }
        }
        if (Best)
        {
            PendingClearance.Add(Best->HazardId, 4.f);
            PendingHazards.Add(Best->HazardId, Best);
            ShowToast(FString::Printf(TEXT("Clearance team dispatched to %s."), *Best->HazardId.ToString()), 4.f);
        }
        else
        {
            ShowToast(TEXT("No revealed hazard in range. Fly closer to the lane."), 3.f);
        }
        return;
    }
    // On foot: call a resupply drop.
    AHavocPlayerCharacter* Marine = GetMarine();
    if (!Marine || Marine->IsDead())
    {
        return;
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

void AHavocEpisode04Flow::TickClearance(float Dt)
{
    UHavocOpenStraitMission* Mission = GetMission();
    UHavocMissionRunner* Runner = Mission->GetRunner();

    // Clearance teams finish their work.
    for (auto It = PendingClearance.CreateIterator(); It; ++It)
    {
        It.Value() -= Dt;
        if (It.Value() <= 0.f)
        {
            if (AHavocLaneHazard* Hazard = PendingHazards.FindRef(It.Key()).Get())
            {
                const FVector At = Hazard->GetActorLocation();
                if (Hazard->Clear())
                {
                    Publish(TEXT("hazard:cleared"), FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f}"), At.X, At.Y, At.Z));
                    ShowToast(FString::Printf(TEXT("%s cleared."), *Hazard->HazardId.ToString()), 3.f);
                }
            }
            PendingHazards.Remove(It.Key());
            It.RemoveCurrent();
        }
    }

    int32 Hazards = 0, Revealed = 0, Cleared = 0;
    for (const FHavocLane& Lane : Mission->GetLanes()->GetLanes())
    {
        Hazards += Lane.Hazards;
        Revealed += Lane.Revealed;
        Cleared += Lane.Cleared;
    }
    SetHint(FString::Printf(TEXT("Lane clearance: %d found, %d cleared of %d.  Fly the Raven along both lanes, press E to mark a revealed hazard.  %s"),
        Revealed, Cleared, Hazards, Raven ? TEXT("") : TEXT("(T = launch Raven)")));

    if (Runner->HasFlag(TEXT("lanes_open")))
    {
        RecoverRaven(nullptr);
        ShowToast(TEXT("Both lanes are open."));
        Mission->Progress(); // -> tanker escort
    }
}

// ---------------------------------------------------------------- escort

void AHavocEpisode04Flow::SpawnTanker(const FString& TankerId, FName LaneId)
{
    AHavocTankerSpline* Spline = nullptr;
    for (TActorIterator<AHavocTankerSpline> It(GetWorld()); It; ++It)
    {
        if (It->LaneId == LaneId)
        {
            Spline = *It;
            break;
        }
    }
    if (!Spline)
    {
        UE_LOG(LogHavocFlow, Error, TEXT("No AHavocTankerSpline for lane '%s'"), *LaneId.ToString());
        return;
    }
    if (!Spline->BeginTransit(TankerId))
    {
        UE_LOG(LogHavocFlow, Warning, TEXT("Mission refused transit of %s on %s"), *TankerId, *LaneId.ToString());
        return;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AHavocTankerVessel* Vessel = GetWorld()->SpawnActor<AHavocTankerVessel>(AHavocTankerVessel::StaticClass(), FTransform(Spline->GetLocationAtDistance(0.f)), Params);
    if (!Vessel)
    {
        return;
    }
    Vessel->TankerId = TankerId;
    Vessel->LaneId = LaneId;
    Vessel->Route = Spline;
    AttachVisual(Vessel, Vessel->GetRootComponent(), TEXT("Vessel.Tanker"));
    Vessels.Add(Vessel);
    ShowToast(FString::Printf(TEXT("%s enters the %s lane."), *TankerId, *LaneId.ToString()), 4.f);
}

void AHavocEpisode04Flow::TickEscort(float Dt)
{
    UHavocOpenStraitMission* Mission = GetMission();
    const TArray<FHavocTanker> Tankers = Mission->GetEscort()->GetTankers();

    // Launch the next tanker.
    LaunchTimer -= Dt;
    if (NextTankerIndex < Tankers.Num() && LaunchTimer <= 0.f)
    {
        LaunchTimer = TankerLaunchIntervalSeconds;
        SpawnTanker(Tankers[NextTankerIndex].Id, (NextTankerIndex % 2 == 0) ? FName(TEXT("inbound")) : FName(TEXT("outbound")));
        ++NextTankerIndex;
    }

    // Move vessels and report arrivals. The mission may advance (and clear us) inside CompleteTransit.
    TArray<TObjectPtr<AHavocTankerVessel>> Snapshot = Vessels;
    for (AHavocTankerVessel* Vessel : Snapshot)
    {
        if (!Vessel)
        {
            continue;
        }
        const FHavocTanker* Info = Tankers.FindByPredicate([Vessel](const FHavocTanker& T) { return T.Id == Vessel->TankerId; });
        if (!Info || Info->State == EHavocTankerState::Lost || Info->State == EHavocTankerState::Through)
        {
            if (Info && Info->State == EHavocTankerState::Lost)
            {
                const FVector At = Vessel->GetActorLocation();
                Publish(TEXT("tanker:damaged"), FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f}"), At.X, At.Y, At.Z));
            }
            Vessels.Remove(Vessel);
            Vessel->Destroy();
            continue;
        }
        if (Vessel->Advance(Dt, TankerSpeedCmPerSec))
        {
            AHavocTankerSpline* Route = Vessel->Route;
            const FString Id = Vessel->TankerId;
            Vessels.Remove(Vessel);
            Vessel->Destroy();
            if (Route)
            {
                Route->CompleteTransit(Id);
            }
            if (GetMission() && GetMission()->GetRunner()->GetState() != TEXT("E04_TANKER_ESCORT"))
            {
                return; // escort settled, mission moved on
            }
        }
    }

    // Threats: warn first, then resolve unless the Falcon is on station.
    ThreatTimer -= Dt;
    if (ThreatCountdown < 0.f && ThreatTimer <= 0.f && Vessels.Num() > 0)
    {
        ThreatTimer = ThreatIntervalSeconds;
        const AHavocTankerVessel* Target = Vessels[FMath::RandRange(0, Vessels.Num() - 1)];
        if (Target)
        {
            ThreatTankerId = Target->TankerId;
            ThreatCountdown = ThreatWarningSeconds;
            ShowToast(FString::Printf(TEXT("CONTACT WARNING: hostile fast craft closing on %s. Call Falcon support (G)!"), *ThreatTankerId), ThreatWarningSeconds);
        }
    }
    else if (ThreatCountdown >= 0.f)
    {
        ThreatCountdown -= Dt;
        if (ThreatCountdown <= 0.f)
        {
            ThreatCountdown = -1.f;
            ResolveThreat();
        }
    }

    int32 Through = 0, Lost = 0;
    for (const FHavocTanker& T : Tankers)
    {
        Through += T.State == EHavocTankerState::Through ? 1 : 0;
        Lost += T.State == EHavocTankerState::Lost ? 1 : 0;
    }
    SetHint(FString::Printf(TEXT("Escort: %d through, %d lost, %d of %d launched.  %s"), Through, Lost, NextTankerIndex, Tankers.Num(), *GetSupportStatus()));
}

void AHavocEpisode04Flow::ResolveThreat()
{
    UHavocOpenStraitMission* Mission = GetMission();
    const AHavocTankerVessel* Target = nullptr;
    for (const AHavocTankerVessel* V : Vessels)
    {
        if (V && V->TankerId == ThreatTankerId)
        {
            Target = V;
        }
    }
    if (!Mission || !Target)
    {
        return;
    }
    if (SupportRemaining > 0.f)
    {
        ShowToast(FString::Printf(TEXT("Falcon drove off the attackers threatening %s."), *ThreatTankerId), 5.f);
        return;
    }
    const FVector At = Target->GetActorLocation();
    Mission->DamageTanker(ThreatTankerId, ThreatDamage);
    Publish(TEXT("tanker:damaged"), FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f}"), At.X, At.Y, At.Z));
    ShowToast(FString::Printf(TEXT("%s was hit! Call Falcon support earlier next time."), *ThreatTankerId), 5.f);
}

// ---------------------------------------------------------------- Falcon

void AHavocEpisode04Flow::CallFalcon()
{
    UHavocOpenStraitMission* Mission = GetMission();
    if (Phase != EHavocFlowPhase::Playing || !Mission || bRespawnPending)
    {
        return;
    }
    if (!Mission->HasAirSupport())
    {
        ShowToast(TEXT("Falcon support needs the airstrip secured first."), 4.f);
        return;
    }
    if (LastState != TEXT("E04_TANKER_ESCORT"))
    {
        ShowToast(TEXT("Falcon support is for the tanker escort."), 4.f);
        return;
    }
    if (SupportRemaining > 0.f)
    {
        ShowToast(TEXT("Falcon is already on station."), 3.f);
        return;
    }
    if (SupportCooldown > 0.f)
    {
        ShowToast(FString::Printf(TEXT("Falcon is rearming (%d s)."), FMath::CeilToInt(SupportCooldown)), 3.f);
        return;
    }
    StartFalconSupport();
}

void AHavocEpisode04Flow::StartFalconSupport()
{
    UWorld* World = GetWorld();
    AHavocPlayerController* PC = GetPC();
    SupportRemaining = FalconSupportSeconds;

    FVector Where = GetMarine() ? GetMarine()->GetActorLocation() : FVector::ZeroVector;
    for (const AHavocTankerVessel* V : Vessels)
    {
        if (V)
        {
            Where = V->GetActorLocation();
            break;
        }
    }
    Where.Z += 4000.f;
    Publish(TEXT("falcon:support"), FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f}"), Where.X, Where.Y, Where.Z));
    ShowToast(TEXT("Falcon on station. Fly it (Space = collective, WASD cyclic, Q/E yaw) or press Tab to stay on the Marine."), 8.f);

    const UGameInstance* GI = GetGameInstance();
    const UHavocPlayInput* Input = GI ? GI->GetSubsystem<UHavocPlayInput>() : nullptr;
    UStaticMesh* Proxy = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!World || !PC || !Input || !Proxy)
    {
        UE_LOG(LogHavocFlow, Warning, TEXT("Falcon support runs without a flyable pawn (input or collision proxy unavailable)."));
        return;
    }

    const FTransform Transform(FRotator::ZeroRotator, Where);
    Falcon = World->SpawnActorDeferred<AHavocFalconHelicopter>(AHavocFalconHelicopter::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Falcon)
    {
        return;
    }
    Falcon->MappingContext = Input->FalconContext;
    Falcon->MappingPriority = 1;
    Falcon->CollectiveAction = Input->CollectiveAction;
    Falcon->CyclicAction = Input->CyclicAction;
    Falcon->YawAction = Input->YawAction;
    // The physics body needs a collision shape; the item-3 Falcon role (a skeletal mesh) is the visual on top.
    Falcon->Body->SetStaticMesh(Proxy);
    Falcon->Body->SetWorldScale3D(FVector(6.f, 2.f, 2.f));
    Falcon->Body->SetVisibility(false, false);
    UGameplayStatics::FinishSpawningActor(Falcon, Transform);
    if (USceneComponent* Visual = AttachVisual(Falcon, Falcon->Body, TEXT("Aircraft.Falcon")))
    {
        Visual->SetRelativeScale3D(FVector(1.f / 6.f, 0.5f, 0.5f)); // undo the proxy's stretch
    }
    PC->Possess(Falcon);
}

void AHavocEpisode04Flow::EndFalconSupport()
{
    if (Falcon)
    {
        if (AHavocPlayerController* PC = GetPC())
        {
            if (PC->GetPawn() == Falcon.Get())
            {
                PossessMarine();
            }
        }
        Falcon->Destroy();
        Falcon = nullptr;
    }
    if (SupportRemaining > 0.f)
    {
        SupportRemaining = 0.f;
        SupportCooldown = FalconCooldownSeconds;
    }
}

void AHavocEpisode04Flow::TickFalcon(float Dt)
{
    ResupplyCooldown = FMath::Max(0.f, ResupplyCooldown - Dt);
    SupportCooldown = FMath::Max(0.f, SupportCooldown - Dt);
    if (SupportRemaining > 0.f)
    {
        SupportRemaining -= Dt;
        if (SupportRemaining <= 0.f)
        {
            SupportRemaining = 1.f; // so EndFalconSupport starts the cooldown
            EndFalconSupport();
            ShowToast(TEXT("Falcon is leaving station to rearm."), 4.f);
        }
    }
}
