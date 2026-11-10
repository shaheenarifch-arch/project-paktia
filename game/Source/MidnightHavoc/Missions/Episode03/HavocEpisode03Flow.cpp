#include "Missions/Episode03/HavocEpisode03Flow.h"

#include "AI/HavocAIController.h"
#include "AI/HavocEnemyCharacter.h"
#include "AI/HavocSquadSubsystem.h"
#include "Audio/HavocAudioSubsystem.h"
#include "Combat/HavocWeaponComponent.h"
#include "Components/BoxComponent.h"
#include "Cutscene/HavocCutscene.h"
#include "Cutscene/HavocFlowCutscene.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HavocEventSubsystem.h"
#include "Data/HavocDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Missions/Episode03/HavocBrokenCitadelMission.h"
#include "Missions/HavocMissionJson.h"
#include "Missions/HavocMissionRunner.h"
#include "Missions/HavocMissionSubsystem.h"
#include "Player/HavocPlayerCharacter.h"
#include "Player/HavocPlayerController.h"
#include "TimerManager.h"
#include "World/HavocSectorVolume.h"

DEFINE_LOG_CATEGORY_STATIC(LogHavocFlow03, Log, All);

namespace
{
    const FName Counterattack(TEXT("counterattack"));
    const float GreyboxHalfSectorM = 120.f;

    struct FObjectiveSector { const TCHAR* Objective; const TCHAR* Sector; };

    // Phase order from data/missions/operation-broken-citadel.json; sectors from docs/gardun-map.md.
    const TArray<FObjectiveSector>& ObjectiveTable()
    {
        static const TArray<FObjectiveSector> Table = {
            { TEXT("capture_airfield"), TEXT("airfield") },
            { TEXT("secure_industrial_depot"), TEXT("industrial") },
            { TEXT("west_ridge_radio"), TEXT("west_ridge") },
            { TEXT("government_relay"), TEXT("government") },
            { TEXT("market_square"), TEXT("old_gardun") },
            { TEXT("medical_station"), TEXT("old_gardun") },
            { TEXT("municipal_ruins"), TEXT("old_gardun") },
            { TEXT("mountain_fort"), TEXT("mountain_fort") },
            { TEXT("east_power_station"), TEXT("east_power") },
            { TEXT("outer_gate"), TEXT("citadel") },
            { TEXT("inner_courtyard"), TEXT("citadel") },
            { TEXT("command_hall"), TEXT("citadel") },
            { TEXT("operations_room"), TEXT("citadel") },
        };
        return Table;
    }

    FString Pretty(const FString& Id)
    {
        return Id.Replace(TEXT("_"), TEXT(" "));
    }

}

AHavocEpisode03Flow::AHavocEpisode03Flow()
    : MissionId(TEXT("episode_03_broken_citadel"))
{
    PrimaryActorTick.bCanEverTick = true;
    EnemyFactions = { TEXT("iran"), TEXT("houthi") };
}

// ---------------------------------------------------------------- pure helpers

FName AHavocEpisode03Flow::GetSectorForObjective(const FString& ObjectiveId)
{
    for (const FObjectiveSector& Row : ObjectiveTable())
    {
        if (ObjectiveId == Row.Objective)
        {
            return FName(Row.Sector);
        }
    }
    return NAME_None;
}

FVector2D AHavocEpisode03Flow::GetGreyboxSectorCentre(FName SectorId)
{
    // Game-space metres (x east, z north), following docs/gardun-map.md north to south. Old Gardun
    // is centred on the medical station's protected zone (data/missions/episode-03-broken-citadel.json).
    struct FPlace { const TCHAR* Id; float X; float Z; };
    static const FPlace Places[] = {
        { TEXT("mountain_fort"), 1180.f, 2150.f },
        { TEXT("west_ridge"), 400.f, 1320.f },
        { TEXT("old_gardun"), 1180.f, 1320.f },
        { TEXT("east_power"), 1960.f, 1320.f },
        { TEXT("citadel"), 1180.f, 880.f },
        { TEXT("government"), 1180.f, 500.f },
        { TEXT("airfield"), 400.f, 230.f },
        { TEXT("riverbed"), 1180.f, 230.f },
        { TEXT("industrial"), 1960.f, 230.f },
        { TEXT("south_gate"), 1180.f, -100.f },
    };
    for (const FPlace& P : Places)
    {
        if (SectorId == FName(P.Id))
        {
            return FVector2D(P.X, P.Z);
        }
    }
    return FVector2D::ZeroVector;
}

int32 AHavocEpisode03Flow::GetGarrisonFor(const FString& ObjectiveId, int32 AssaultPhase) const
{
    if (GetSectorForObjective(ObjectiveId).IsNone() || ObjectiveId == TEXT("medical_station"))
    {
        return 0; // the medical station is a protected location: hold it, no garrison
    }
    return GarrisonPerObjective + (AssaultPhase == 5 ? CitadelExtraGarrison : 0);
}

// ---------------------------------------------------------------- lifecycle

void AHavocEpisode03Flow::BeginPlay()
{
    Super::BeginPlay();
    BuildGreyboxIfNeeded();
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Subscribe(TEXT("player:died"), this, GET_FUNCTION_NAME_CHECKED(AHavocEpisode03Flow, OnPlayerDied));
        }
    }
}

void AHavocEpisode03Flow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Unsubscribe(TEXT("player:died"), this, GET_FUNCTION_NAME_CHECKED(AHavocEpisode03Flow, OnPlayerDied));
        }
    }
    Super::EndPlay(EndPlayReason);
}

void AHavocEpisode03Flow::BuildGreyboxIfNeeded()
{
    UWorld* World = GetWorld();
    for (TActorIterator<AHavocSectorVolume> It(World); It; ++It)
    {
        Sectors.Add(It->SectorId, *It);
    }
    if (Sectors.Contains(TEXT("citadel")))
    {
        return; // a built Gardun level
    }
    Sectors.Reset();
    bGreybox = true;
    UE_LOG(LogHavocFlow03, Log, TEXT("No Gardun sector volumes in the level: building the greybox Gardun at (%s)."), *GreyboxOriginCm.ToString());

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

    // Floor: a 3 km square slab whose top face is at the origin height.
    if (Cube)
    {
        const FVector Centre = GameToWorld(1180.0, 1100.0) - FVector(0.f, 0.f, 100.f);
        AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(Centre, FRotator::ZeroRotator, Params);
        if (Floor)
        {
            Floor->SetMobility(EComponentMobility::Movable);
            Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
            Floor->SetActorScale3D(FVector(3000.f, 3000.f, 2.f)); // cube is 1 m: 3 km x 3 km x 2 m
            GreyboxActors.Add(Floor);
        }
    }
    else
    {
        UE_LOG(LogHavocFlow03, Warning, TEXT("Engine cube mesh not found: the greybox has no floor."));
    }

    static const TCHAR* const SectorIds[] = { TEXT("mountain_fort"), TEXT("west_ridge"), TEXT("old_gardun"), TEXT("east_power"), TEXT("citadel"),
        TEXT("government"), TEXT("airfield"), TEXT("riverbed"), TEXT("industrial"), TEXT("south_gate") };
    for (const TCHAR* Id : SectorIds)
    {
        const FVector2D C = GetGreyboxSectorCentre(FName(Id));
        AHavocSectorVolume* Volume = World->SpawnActor<AHavocSectorVolume>(GameToWorld(C.X, C.Y) + FVector(0.f, 0.f, 20000.f), FRotator::ZeroRotator, Params);
        if (!Volume)
        {
            continue;
        }
        Volume->SectorId = FName(Id);
        if (UBoxComponent* Box = Cast<UBoxComponent>(Volume->GetCollisionComponent()))
        {
            Box->SetBoxExtent(FVector(GreyboxHalfSectorM * 100.f, GreyboxHalfSectorM * 100.f, 20000.f));
        }
        Sectors.Add(Volume->SectorId, Volume);
        GreyboxActors.Add(Volume);
    }

    // A pillar marks every objective point.
    if (Cylinder)
    {
        for (const FObjectiveSector& Row : ObjectiveTable())
        {
            AStaticMeshActor* Pillar = World->SpawnActor<AStaticMeshActor>(ObjectivePoint(Row.Objective) + FVector(0.f, 0.f, 400.f), FRotator::ZeroRotator, Params);
            if (Pillar)
            {
                Pillar->SetMobility(EComponentMobility::Movable);
                Pillar->GetStaticMeshComponent()->SetStaticMesh(Cylinder);
                Pillar->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Pillar->SetActorScale3D(FVector(3.f, 3.f, 8.f));
                GreyboxActors.Add(Pillar);
            }
        }
    }
}

// ---------------------------------------------------------------- lookup

UHavocBrokenCitadelMission* AHavocEpisode03Flow::GetMission() const
{
    const UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    return Missions ? Missions->GetBrokenCitadel() : nullptr;
}

AHavocPlayerController* AHavocEpisode03Flow::GetPC()
{
    if (!CachedPC)
    {
        CachedPC = Cast<AHavocPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    }
    return CachedPC;
}

AHavocPlayerCharacter* AHavocEpisode03Flow::GetMarine()
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

AHavocSectorVolume* AHavocEpisode03Flow::FindSector(FName SectorId) const
{
    const TObjectPtr<AHavocSectorVolume>* Found = Sectors.Find(SectorId);
    return Found ? Found->Get() : nullptr;
}

int32 AHavocEpisode03Flow::CountAlive(FName Tag) const
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

FVector AHavocEpisode03Flow::GameToWorld(double X, double Z) const
{
    return FVector(GreyboxOriginCm.X + X * 100.0, GreyboxOriginCm.Y + Z * 100.0, GreyboxOriginCm.Z);
}

FVector AHavocEpisode03Flow::WorldToGame(const FVector& Location) const
{
    return FVector((Location.X - GreyboxOriginCm.X) / 100.0, (Location.Z - GreyboxOriginCm.Z) / 100.0, (Location.Y - GreyboxOriginCm.Y) / 100.0);
}

FVector AHavocEpisode03Flow::ObjectivePoint(const FString& ObjectiveId) const
{
    const FName SectorId = GetSectorForObjective(ObjectiveId);
    const AHavocSectorVolume* Volume = FindSector(SectorId);
    if (!Volume)
    {
        return GreyboxOriginCm;
    }
    // Objectives sharing a sector sit on a ring around its centre.
    int32 Index = 0;
    int32 Total = 0;
    for (const FObjectiveSector& Row : ObjectiveTable())
    {
        if (FName(Row.Sector) == SectorId)
        {
            if (ObjectiveId == Row.Objective)
            {
                Index = Total;
            }
            ++Total;
        }
    }
    const FBox Box = Volume->GetComponentsBoundingBox(true);
    FVector Centre = Box.GetCenter();
    Centre.Z = Volume->GetActorLocation().Z;
    if (bGreybox)
    {
        Centre.Z = GreyboxOriginCm.Z;
    }
    if (Total > 1)
    {
        const float Radius = 0.35f * FMath::Min(Box.GetExtent().X, Box.GetExtent().Y);
        const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(Total);
        Centre += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
    }
    return Centre;
}

void AHavocEpisode03Flow::ShowToast(const FString& Text, float Seconds)
{
    Toast = Text;
    ToastTime = Seconds;
}

void AHavocEpisode03Flow::Publish(const TCHAR* EventName, const FString& Payload) const
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        if (UHavocEventSubsystem* Events = GI->GetSubsystem<UHavocEventSubsystem>())
        {
            Events->Publish(EventName, Payload);
        }
    }
}

FString AHavocEpisode03Flow::GetObjectiveText() const
{
    const UHavocBrokenCitadelMission* Mission = GetMission();
    return Mission && Mission->GetRunner() ? Mission->GetRunner()->GetObjective() : FString();
}

FString AHavocEpisode03Flow::GetSupportStatus() const
{
    const UHavocBrokenCitadelMission* Mission = GetMission();
    if (!Mission || !Mission->HasAirSupport())
    {
        if (Mission && Mission->GetUnlocks().Contains(TEXT("falcon_air_support")))
        {
            return TEXT("Falcon: no crew (Episode 02 did not deliver it)");
        }
        return TEXT("Falcon: not available (take the airfield)");
    }
    if (FalconCooldown > 0.f)
    {
        return FString::Printf(TEXT("Falcon: rearming %ds"), FMath::CeilToInt(FalconCooldown));
    }
    return TEXT("Falcon: ready (G)");
}

FString AHavocEpisode03Flow::GetStatusBlock() const
{
    const UHavocBrokenCitadelMission* Mission = GetMission();
    if (!Mission)
    {
        return FString();
    }
    FString Out;
    if (Mission->GetPhase() > 0)
    {
        Out += FString::Printf(TEXT("PHASE %d / 5\n"), Mission->GetPhase());
    }
    Out += FString::Printf(TEXT("Reinforcements: %d\n"), Mission->GetReinforcements());
    const TArray<FString> Open = Mission->GetOpenObjectives();
    if (Open.Num() > 0)
    {
        TArray<FString> Names;
        for (const FString& Id : Open) { Names.Add(Pretty(Id)); }
        Out += TEXT("Open: ") + FString::Join(Names, TEXT(", ")) + TEXT("\n");
    }
    Out += FString::Printf(TEXT("Enemy counterattacks: %d%%\n"), FMath::RoundToInt(100.0 * Mission->GetCounterattackStrength()));
    Out += FString::Printf(TEXT("Aegis: %s\n"), Mission->IsAegisOnline() ? TEXT("online") : TEXT("offline"));
    if (Mission->IsMedicalStationProtected())
    {
        Out += TEXT("Medical station: protected zone\n");
    }
    if (Mission->GetSurprise().IsActive())
    {
        Out += FString::Printf(TEXT("EVENT: %s\n"), *Pretty(Mission->GetSurprise().GetActive()));
    }
    Out += GetSupportStatus();
    return Out;
}

// ---------------------------------------------------------------- episode control

void AHavocEpisode03Flow::StartEpisode()
{
    UHavocMissionSubsystem* Missions = GetWorld() ? GetWorld()->GetSubsystem<UHavocMissionSubsystem>() : nullptr;
    if (!Missions || !Missions->StartMission(MissionId))
    {
        ShowToast(TEXT("Episode 03 could not start. Check that the data folder is packaged beside the game (game/README.md)."), 10.f);
        UE_LOG(LogHavocFlow03, Error, TEXT("Mission '%s' failed to start"), *MissionId.ToString());
        return;
    }
    ClearWorld();
    Phase = EHavocFlowPhase::Playing;
    LastState.Empty();
    LastSurprise.Empty();
    DeathPenalty = 0;
    bRespawnPending = false;
    ResupplyLeft = ResupplyCharges;
    FalconCooldown = 0.f;
    ResupplyCooldown = 0.f;
    CaptureProgress.Reset();
    Outcome.Empty();
    ResultSummary.Empty();

    if (AHavocPlayerCharacter* Marine = GetMarine())
    {
        if (bGreybox)
        {
            const FVector2D C = GetGreyboxSectorCentre(TEXT("south_gate"));
            TeleportMarine(GameToWorld(C.X, C.Y) + FVector(0.f, 0.f, 200.f));
        }
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
    // Intro fly-over of the Gardun approach and the citadel, then briefing -> staging -> phase 1.
    // Tick pauses the state machine while the cutscene plays, so nothing happens behind it.
    TWeakObjectPtr<AHavocEpisode03Flow> Self(this);
    PlayCutscene(true, [Self]()
    {
        if (AHavocEpisode03Flow* Flow = Self.Get())
        {
            if (UHavocBrokenCitadelMission* Mission = Flow->GetMission())
            {
                Mission->BeginAssault();
            }
        }
    });
}

// ---------------------------------------------------------------- cutscenes

TArray<FName> AHavocEpisode03Flow::CutsceneSectorIds(bool bIntro)
{
    return bIntro ? TArray<FName>{ TEXT("south_gate"), TEXT("old_gardun"), TEXT("citadel") }
                  : TArray<FName>{ TEXT("citadel"), TEXT("old_gardun") };
}

TArray<FVector> AHavocEpisode03Flow::BuildGreyboxCutsceneTargets(const FVector& OriginCm, bool bIntro)
{
    TArray<FVector> Targets;
    for (const FName& Id : CutsceneSectorIds(bIntro))
    {
        const FVector2D C = GetGreyboxSectorCentre(Id);
        Targets.Add(FVector(OriginCm.X + C.X * 100.0, OriginCm.Y + C.Y * 100.0, OriginCm.Z));
    }
    return Targets;
}

TArray<FVector> AHavocEpisode03Flow::CutsceneTargets(bool bIntro)
{
    // The level's sector volumes (the built Gardun, or the greybox's own); a sector with no volume falls back to the greybox layout.
    const TArray<FVector> Fallback = BuildGreyboxCutsceneTargets(GreyboxOriginCm, bIntro);
    const TArray<FName> Ids = CutsceneSectorIds(bIntro);
    TArray<FVector> Targets;
    for (int32 i = 0; i < Ids.Num(); ++i)
    {
        const AHavocSectorVolume* Volume = FindSector(Ids[i]);
        Targets.Add(Volume ? Volume->GetComponentsBoundingBox(true).GetCenter() : Fallback[i]);
    }
    return Targets;
}

void AHavocEpisode03Flow::PlayCutscene(bool bIntro, TFunction<void()> Then)
{
    HavocFlowCutscene::Play(*this, Cutscene, GetPC(), TEXT("03"), bIntro, CutsceneTargets(bIntro),
                            HavocFlowCutscene::BuildLines(TEXT("03"), bIntro, Outcome), MoveTemp(Then));
}

void AHavocEpisode03Flow::SkipCutscene() { if (Cutscene) { Cutscene->Skip(); } }
bool AHavocEpisode03Flow::IsCutsceneActive() const { return Cutscene && Cutscene->IsPlaying(); }

void AHavocEpisode03Flow::RestartFromCheckpoint()
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

void AHavocEpisode03Flow::ClearWorld()
{
    for (AHavocEnemyCharacter* Enemy : Enemies)
    {
        if (Enemy)
        {
            Enemy->Destroy();
        }
    }
    Enemies.Reset();
    CaptureProgress.Reset();
}

void AHavocEpisode03Flow::EndEpisode()
{
    UHavocBrokenCitadelMission* Mission = GetMission();
    if (!Mission)
    {
        return;
    }
    Phase = EHavocFlowPhase::Results;
    Outcome = Mission->GetRunner()->GetOutcome();

    const FHavocEpisode03Result R = Mission->GetResult();
    ResultSummary = FString::Printf(
        TEXT("Phases reached: %d / 5\nObjectives taken: %d\nReinforcements left: %d\nAegis online: %s\nSurprise event: %s"),
        R.PhaseReached, R.ObjectivesCaptured, R.ReinforcementsLeft, R.bAegisOnline ? TEXT("yes") : TEXT("no"), *Pretty(R.SurpriseEvent));
    Publish(TEXT("episode:results"), Mission->GetSummaryJson());

    // Carryover: Episode 04 adds its bonus reinforcements when this is SUCCESS.
    if (UHavocMissionSubsystem* Missions = GetWorld()->GetSubsystem<UHavocMissionSubsystem>())
    {
        Missions->SetEpisode3Outcome(Outcome);
    }
    // Outro fly-over, then the results screen (skippable).
    TWeakObjectPtr<AHavocEpisode03Flow> Self(this);
    PlayCutscene(false, [Self]()
    {
        if (AHavocEpisode03Flow* Flow = Self.Get())
        {
            if (AHavocPlayerController* PC = Flow->GetPC())
            {
                PC->ShowMenu(EHavocMenuMode::Results);
            }
        }
    });
}

// ---------------------------------------------------------------- tick

void AHavocEpisode03Flow::Tick(float DeltaSeconds)
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
    if (Phase != EHavocFlowPhase::Playing || IsCutsceneActive())
    {
        return;
    }

    UHavocBrokenCitadelMission* Mission = GetMission();
    if (!Mission || !Mission->GetRunner())
    {
        return;
    }
    if (Mission->GetRunner()->IsComplete())
    {
        EndEpisode();
        return;
    }
    if (bRespawnPending)
    {
        return;
    }

    const FString State = Mission->GetState();
    if (State != LastState)
    {
        OnEnterState(State, false);
    }
    FalconCooldown = FMath::Max(0.f, FalconCooldown - DeltaSeconds);
    ResupplyCooldown = FMath::Max(0.f, ResupplyCooldown - DeltaSeconds);
    TickPhase(DeltaSeconds);
    TickWaves(DeltaSeconds);
    WatchSurprise();
}

void AHavocEpisode03Flow::OnEnterState(const FString& State, bool bRestored)
{
    UHavocBrokenCitadelMission* Mission = GetMission();
    UHavocMissionRunner* Runner = Mission->GetRunner();
    LastState = State;

    AHavocPlayerCharacter* Marine = GetMarine();
    if (!bRestored && Marine && Runner->HasCheckpoint() && Runner->GetCheckpoint().State == State)
    {
        CheckpointTransform = Marine->GetActorTransform();
    }

    const int32 PhaseNumber = Mission->GetPhase();
    if (PhaseNumber == 0)
    {
        return;
    }
    // A new phase withdraws leftover counterattackers and garrisons the phase's open objectives.
    for (AHavocEnemyCharacter* Enemy : Enemies)
    {
        if (Enemy && Enemy->SectorId == Counterattack)
        {
            Enemy->Destroy();
        }
    }
    WaveTimer = WaveIntervalSeconds * 0.5f;
    for (const FString& Objective : Mission->GetOpenObjectives())
    {
        const FName Tag(*Objective);
        if (CountAlive(Tag) == 0)
        {
            SpawnGroup(ObjectivePoint(Objective), 3000.f, GetGarrisonFor(Objective, PhaseNumber), Tag, false);
        }
    }
    ShowToast(FString::Printf(TEXT("Phase %d: %s"), PhaseNumber, *Runner->GetObjective()), 8.f);
}

void AHavocEpisode03Flow::TickPhase(float Dt)
{
    UHavocBrokenCitadelMission* Mission = GetMission();
    AHavocPlayerCharacter* Marine = GetMarine();
    if (!Mission || !Marine || Mission->GetPhase() == 0)
    {
        return;
    }
    const FVector MarineLoc = Marine->GetActorLocation();
    const TArray<FString> Open = Mission->GetOpenObjectives();

    // Hint follows the nearest open objective.
    const FString* Nearest = nullptr;
    float NearestDist = TNumericLimits<float>::Max();
    for (const FString& Objective : Open)
    {
        const float D = FVector::Dist2D(ObjectivePoint(Objective), MarineLoc);
        if (D < NearestDist)
        {
            NearestDist = D;
            Nearest = &Objective;
        }
    }

    TArray<FString> Taken;
    for (const FString& Objective : Open)
    {
        const bool bInside = !Marine->IsDead() && FVector::Dist2D(ObjectivePoint(Objective), MarineLoc) <= CaptureRadiusCm;
        const int32 Left = CountAlive(FName(*Objective));
        if (bInside && Left == 0)
        {
            float& Progress = CaptureProgress.FindOrAdd(Objective);
            Progress += Dt;
            if (Progress >= CaptureSeconds)
            {
                Taken.Add(Objective);
            }
        }
        if (Nearest && &Objective == Nearest)
        {
            if (Left > 0)
            {
                Hint = FString::Printf(TEXT("%s: %d hostiles left%s"), *Pretty(Objective), Left, bInside ? TEXT("") : TEXT(" (move to the marker)"));
            }
            else if (bInside)
            {
                const float Held = CaptureProgress.FindRef(Objective);
                Hint = FString::Printf(TEXT("Holding %s: %d s"), *Pretty(Objective), FMath::CeilToInt(FMath::Max(0.f, CaptureSeconds - Held)));
            }
            else
            {
                Hint = FString::Printf(TEXT("Go to %s (%d m)"), *Pretty(Objective), FMath::RoundToInt(NearestDist / 100.f));
            }
        }
    }

    for (const FString& Objective : Taken)
    {
        if (Mission->GetPhase() == 0 || !Mission->GetOpenObjectives().Contains(Objective))
        {
            continue; // the phase finished on an earlier objective this frame
        }
        Mission->CaptureObjective(Objective);
        CaptureProgress.Remove(Objective);
        FString Message = FString::Printf(TEXT("%s taken."), *Pretty(Objective));
        const FString Unlock = Mission->GetUnlockFor(Objective);
        if (Unlock == TEXT("falcon_air_support"))
        {
            Message += Mission->HasAirSupport() ? TEXT(" Falcon air support unlocked (G).") : TEXT(" The airfield is ours, but no Falcon crew was delivered in Episode 02.");
        }
        else if (Unlock == TEXT("enemy_comms_degraded") && Mission->GetCounterattackStrength() < 1.0)
        {
            Message += TEXT(" Enemy communications are broken: counterattacks drop to 60%.");
        }
        else if (Unlock == TEXT("aegis_online"))
        {
            Message += TEXT(" Aegis support is online.");
        }
        else if (Unlock == TEXT("medical_station_protected"))
        {
            Message += TEXT(" The medical station is now a protected zone: no support may target it.");
        }
        ShowToast(Message, 7.f);
    }
}

void AHavocEpisode03Flow::WatchSurprise()
{
    const UHavocBrokenCitadelMission* Mission = GetMission();
    if (!Mission)
    {
        return;
    }
    const FString Active = Mission->GetSurprise().GetActive();
    if (Active == LastSurprise)
    {
        return;
    }
    if (!Active.IsEmpty())
    {
        ShowToast(FString::Printf(TEXT("SURPRISE EVENT: %s"), *Pretty(Active)), 8.f);
        Publish(TEXT("surprise:event"), FString::Printf(TEXT("{\"event\":\"%s\",\"active\":true}"), *Active));
    }
    else
    {
        Publish(TEXT("surprise:event"), FString::Printf(TEXT("{\"event\":\"%s\",\"active\":false}"), *LastSurprise));
    }
    LastSurprise = Active;
}

// ---------------------------------------------------------------- enemies

void AHavocEpisode03Flow::SpawnGroup(const FVector& Centre, float RadiusCm, int32 Count, FName Tag, bool bRaiders)
{
    UWorld* World = GetWorld();
    const UGameInstance* GI = GetGameInstance();
    const UHavocDataSubsystem* Data = GI ? GI->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    if (!World || !Data || Count <= 0 || EnemyFactions.Num() == 0)
    {
        return;
    }
    TArray<AHavocEnemyCharacter*> Spawned;
    for (int32 i = 0; i < Count; ++i)
    {
        const FString& Faction = EnemyFactions[i % EnemyFactions.Num()];
        const TArray<FHavocUnitDef> Units = Data->GetInfantryUnits(Faction);
        if (Units.Num() == 0)
        {
            UE_LOG(LogHavocFlow03, Warning, TEXT("No infantry units for faction '%s' in data/units"), *Faction);
            continue;
        }
        const FHavocUnitDef& Unit = Units[FMath::RandRange(0, Units.Num() - 1)];

        bool bPlaced = false;
        FVector Spot = FVector::ZeroVector;
        for (int32 Attempt = 0; Attempt < 12 && !bPlaced; ++Attempt)
        {
            const float Angle = FMath::FRandRange(0.f, 2.f * PI);
            const float Dist = FMath::FRandRange(0.2f, 1.f) * RadiusCm;
            const float X = Centre.X + FMath::Cos(Angle) * Dist;
            const float Y = Centre.Y + FMath::Sin(Angle) * Dist;
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(Hit, FVector(X, Y, Centre.Z + 30000.f), FVector(X, Y, Centre.Z - 30000.f), ECC_WorldStatic))
            {
                continue;
            }
            if (Hit.ImpactNormal.Z < 0.6f)
            {
                continue;
            }
            Spot = Hit.ImpactPoint + FVector(0.f, 0.f, 110.f);
            bPlaced = true;
        }
        if (!bPlaced)
        {
            UE_LOG(LogHavocFlow03, Warning, TEXT("No ground found near %s for %s (is the level built?)"), *Centre.ToString(), *Unit.Id);
            continue;
        }

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
        if (bRaiders)
        {
            if (AHavocAIController* AI = Cast<AHavocAIController>(Enemy->GetController()))
            {
                AI->NotifyShotBy(GetMarine());
            }
        }
    }
    if (UHavocSquadSubsystem* Squads = World->GetSubsystem<UHavocSquadSubsystem>())
    {
        Squads->RegisterSquad(EHavocTeam::Opposing, Tag, bRaiders ? EHavocSquadPosture::Assault : EHavocSquadPosture::Patrol, Spawned);
    }
}

void AHavocEpisode03Flow::TickWaves(float Dt)
{
    UHavocBrokenCitadelMission* Mission = GetMission();
    AHavocPlayerCharacter* Marine = GetMarine();
    // Counterattacks start in phase 2; phase 1 is the approach.
    if (!Mission || !Marine || Marine->IsDead() || Mission->GetPhase() < 2)
    {
        return;
    }
    WaveTimer -= Dt;
    if (WaveTimer > 0.f)
    {
        return;
    }
    WaveTimer = WaveIntervalSeconds;
    const int32 Alive = CountAlive(Counterattack);
    const int32 Size = FMath::Max(1, FMath::CeilToInt(WaveSize * static_cast<float>(Mission->GetCounterattackStrength())));
    const int32 Count = FMath::Min(Size, MaxRaidersAlive - Alive);
    if (Count <= 0)
    {
        return;
    }
    const float Angle = FMath::FRandRange(0.f, 2.f * PI);
    const FVector Centre = Marine->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 7000.f;
    SpawnGroup(Centre, 1500.f, Count, Counterattack, true);
    ShowToast(Mission->GetCounterattackStrength() < 1.0 ? TEXT("Counterattack (weakened: enemy comms are broken)!") : TEXT("Enemy counterattack!"));
}

// ---------------------------------------------------------------- actions

void AHavocEpisode03Flow::HandleInteract()
{
    if (Phase != EHavocFlowPhase::Playing || bRespawnPending || IsCutsceneActive())
    {
        return;
    }
    AHavocPlayerCharacter* Marine = GetMarine();
    if (!Marine || Marine->IsDead())
    {
        return;
    }
    // A resupply drop, as in Episode 04.
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

void AHavocEpisode03Flow::CallFalcon()
{
    UHavocBrokenCitadelMission* Mission = GetMission();
    AHavocPlayerCharacter* Marine = GetMarine();
    if (Phase != EHavocFlowPhase::Playing || !Mission || !Marine || bRespawnPending || Marine->IsDead())
    {
        return;
    }
    if (!Mission->HasAirSupport())
    {
        ShowToast(Mission->GetUnlocks().Contains(TEXT("falcon_air_support"))
            ? TEXT("No Falcon crew: Episode 02 did not deliver it, so there is no air support.")
            : TEXT("Falcon support needs the airfield taken first."), 5.f);
        return;
    }
    if (FalconCooldown > 0.f)
    {
        ShowToast(FString::Printf(TEXT("Falcon is rearming (%d s)."), FMath::CeilToInt(FalconCooldown)), 3.f);
        return;
    }
    // Abstract strike: hostiles near the Marine go down, except anything inside a protected zone.
    int32 Hit = 0;
    int32 Spared = 0;
    for (AHavocEnemyCharacter* Enemy : Enemies)
    {
        if (!Enemy || Enemy->IsDead() || FVector::Dist2D(Enemy->GetActorLocation(), Marine->GetActorLocation()) > FalconStrikeRadiusCm)
        {
            continue;
        }
        const FVector G = WorldToGame(Enemy->GetActorLocation());
        FString ZoneId;
        if (Mission->IsInProtectedZone(G.X, G.Y, G.Z, ZoneId))
        {
            ++Spared;
            continue;
        }
        Enemy->ApplyAuthoritativeDamage(100000.f);
        ++Hit;
    }
    if (Hit == 0)
    {
        ShowToast(Spared > 0 ? TEXT("Falcon holds fire: the targets are inside a protected zone.") : TEXT("Falcon: no hostiles in range."), 4.f);
        return;
    }
    FalconCooldown = FalconCooldownSeconds;
    Publish(TEXT("falcon:support"), FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f}"), Marine->GetActorLocation().X, Marine->GetActorLocation().Y, Marine->GetActorLocation().Z));
    ShowToast(FString::Printf(TEXT("Falcon strike: %d hostiles down."), Hit), 5.f);
}

// ---------------------------------------------------------------- marine

void AHavocEpisode03Flow::TeleportMarine(const FVector& Where)
{
    if (AHavocPlayerCharacter* Marine = GetMarine())
    {
        Marine->TeleportTo(Where, Marine->GetActorRotation());
    }
}

void AHavocEpisode03Flow::OnPlayerDied(FName EventName, const FString& Payload)
{
    if (Phase == EHavocFlowPhase::Playing && !bRespawnPending)
    {
        BeginRespawn();
    }
}

void AHavocEpisode03Flow::BeginRespawn()
{
    bRespawnPending = true;
    ShowToast(TEXT("Marine down. Restoring the last checkpoint (costs reinforcements)..."), 4.f);
    GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        RespawnFromCheckpoint(true);
    }), 3.f, false);
}

void AHavocEpisode03Flow::RespawnFromCheckpoint(bool bPenalty)
{
    UHavocBrokenCitadelMission* Mission = GetMission();
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
        Mission->SpendReinforcements(DeathPenalty); // zero fails the assault
    }
    if (AHavocPlayerCharacter* Marine = GetMarine())
    {
        Marine->Revive(bHaveInitialTransform ? CheckpointTransform : Marine->GetActorTransform());
    }
    bRespawnPending = false;
    if (!Runner->IsComplete())
    {
        OnEnterState(Runner->GetState(), true);
    }
}
