#include "AI/HavocSquadSubsystem.h"

#include "AI/HavocAIController.h"
#include "AI/HavocEnemyCharacter.h"
#include "Core/HavocEventSubsystem.h"
#include "Data/HavocDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "World/HavocSectorVolume.h"
#include "World/HavocWorldLog.h"

bool UHavocSquadSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UHavocSquadSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UHavocSquadSubsystem, STATGROUP_Tickables);
}

void UHavocSquadSubsystem::Deinitialize()
{
    Squads.Empty();
    Super::Deinitialize();
}

void UHavocSquadSubsystem::EnsureConfig() const
{
    if (bConfigLoaded)
    {
        return;
    }
    bConfigLoaded = true;
    const UWorld* World = GetWorld();
    const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    const UHavocDataSubsystem* Data = GI ? GI->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    if (!Data)
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Squads: no data subsystem; using default squad tuning"));
        return;
    }
    const FString Path = FPaths::Combine(Data->GetResolvedDataDirectory(), TEXT("ai/squad-behaviour.json"));
    FString Text, Error;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Squads: could not read %s; using default squad tuning"), *Path);
        return;
    }
    if (!HavocSquad::ParseConfig(Text, Config, Error))
    {
        UE_LOG(LogHavocWorld, Error, TEXT("Squads: %s: %s; using default squad tuning"), *Path, *Error);
        Config = HavocSquad::FConfig();
    }
}

const HavocSquad::FTuning& UHavocSquadSubsystem::GetTuning() const
{
    EnsureConfig();
    return Config.Tuning;
}

UHavocSquadSubsystem::FRuntimeSquad* UHavocSquadSubsystem::Find(int32 SquadId)
{
    return Squads.FindByPredicate([SquadId](const FRuntimeSquad& S) { return S.Id == SquadId; });
}

const UHavocSquadSubsystem::FRuntimeSquad* UHavocSquadSubsystem::Find(int32 SquadId) const
{
    return Squads.FindByPredicate([SquadId](const FRuntimeSquad& S) { return S.Id == SquadId; });
}

int32 UHavocSquadSubsystem::RegisterSquad(EHavocTeam Team, FName SectorId, EHavocSquadPosture Posture, const TArray<AHavocEnemyCharacter*>& Members, bool bOwnedByPhase)
{
    const UWorld* World = GetWorld();
    if (!World || Members.Num() == 0)
    {
        return -1;
    }
    FRuntimeSquad Squad;
    Squad.Id = NextSquadId++;
    Squad.Team = Team;
    Squad.Sector = SectorId;
    Squad.bOwnedByPhase = bOwnedByPhase;
    Squad.Model.Posture = Posture;
    Squad.Model.ModeEnteredTime = World->GetTimeSeconds();
    for (AHavocEnemyCharacter* Member : Members)
    {
        if (Member)
        {
            Member->SquadId = Squad.Id;
            Squad.Members.Add(Member);
        }
    }
    Squad.Model.Size = Squad.Members.Num();
    Squads.Add(Squad);
    Publish(Squad);
    return Squad.Id;
}

bool UHavocSquadSubsystem::FindLandSpot(FName SectorId, FVector& OutSpot) const
{
    UWorld* World = GetWorld();
    const AHavocSectorVolume* Volume = nullptr;
    for (TActorIterator<AHavocSectorVolume> It(World); It; ++It)
    {
        if (It->SectorId == SectorId)
        {
            Volume = *It;
            break;
        }
    }
    if (!Volume)
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Squads: no sector volume '%s' placed; cannot deploy"), *SectorId.ToString());
        return false;
    }
    const FBox Box = Volume->GetComponentsBoundingBox(true);
    for (int32 Attempt = 0; Attempt < 12; ++Attempt)
    {
        const float X = FMath::FRandRange(Box.Min.X + Box.GetExtent().X * 0.2f, Box.Max.X - Box.GetExtent().X * 0.2f);
        const float Y = FMath::FRandRange(Box.Min.Y + Box.GetExtent().Y * 0.2f, Box.Max.Y - Box.GetExtent().Y * 0.2f);
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, FVector(X, Y, Box.Max.Z + 30000.f), FVector(X, Y, Box.Min.Z - 30000.f), ECC_WorldStatic))
        {
            continue;
        }
        // Keep units on land: skip the water surface and steep ground.
        if ((Hit.GetActor() && Hit.GetActor()->GetClass()->GetName().Contains(TEXT("Water"))) || Hit.ImpactNormal.Z < 0.6f || Hit.ImpactPoint.Z < 50.f)
        {
            continue;
        }
        OutSpot = Hit.ImpactPoint + FVector(0.f, 0.f, 110.f);
        return true;
    }
    return false;
}

int32 UHavocSquadSubsystem::SpawnDeployment(const HavocSquad::FDeployment& Deployment, bool bOwnedByPhase)
{
    UWorld* World = GetWorld();
    const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    const UHavocDataSubsystem* Data = GI ? GI->GetSubsystem<UHavocDataSubsystem>() : nullptr;
    if (!Data || (World && World->GetNetMode() == NM_Client))
    {
        return -1;
    }
    const TArray<FHavocUnitDef> Units = Data->GetInfantryUnits(Deployment.Faction);
    if (Units.Num() == 0)
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Squads: no infantry units for faction '%s' in data/units"), *Deployment.Faction);
        return -1;
    }
    FVector Anchor;
    if (!FindLandSpot(Deployment.Sector, Anchor))
    {
        UE_LOG(LogHavocWorld, Warning, TEXT("Squads: no land found in sector '%s' (is the landscape built?)"), *Deployment.Sector.ToString());
        return -1;
    }

    TArray<AHavocEnemyCharacter*> Spawned;
    for (int32 i = 0; i < Deployment.Size; ++i)
    {
        // Squad stays together: members spawn in a loose cluster around the anchor.
        const FVector2D Offset = FMath::RandPointInCircle(350.f);
        const FTransform Where(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Anchor + FVector(Offset.X, Offset.Y, 0.f));
        AHavocEnemyCharacter* Unit = World->SpawnActorDeferred<AHavocEnemyCharacter>(
            AHavocEnemyCharacter::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!Unit)
        {
            continue;
        }
        Unit->ApplyUnitDef(Units[i % Units.Num()], Deployment.Sector);
        UGameplayStatics::FinishSpawningActor(Unit, Where);
        Spawned.Add(Unit);
    }
    return RegisterSquad(HavocSquad::TeamForFaction(Deployment.Faction), Deployment.Sector, Deployment.Posture, Spawned, bOwnedByPhase);
}

void UHavocSquadSubsystem::RemovePhaseSquads()
{
    for (int32 i = Squads.Num() - 1; i >= 0; --i)
    {
        if (Squads[i].bOwnedByPhase)
        {
            for (const TWeakObjectPtr<AHavocEnemyCharacter>& Member : Squads[i].Members)
            {
                if (Member.IsValid())
                {
                    Member->Destroy();
                }
            }
            Squads.RemoveAt(i);
        }
    }
}

int32 UHavocSquadSubsystem::ApplyPhase(const FString& State)
{
    EnsureConfig();
    CurrentPhase = State;
    RemovePhaseSquads();
    int32 Spawned = 0;
    for (const HavocSquad::FDeployment& Deployment : Config.ForState(State))
    {
        if (SpawnDeployment(Deployment, /*bOwnedByPhase*/ true) >= 0)
        {
            ++Spawned;
        }
    }
    UE_LOG(LogHavocWorld, Log, TEXT("Squads: phase '%s': %d squad(s) deployed"), *State, Spawned);
    return Spawned;
}

void UHavocSquadSubsystem::ReportThreat(int32 SquadId, AActor* Threat)
{
    const UWorld* World = GetWorld();
    FRuntimeSquad* Squad = Find(SquadId);
    if (Squad && World && Threat)
    {
        Squad->Threat = Threat;
        Squad->Model.ReportThreat(World->GetTimeSeconds());
    }
}

EHavocSquadMode UHavocSquadSubsystem::GetSquadMode(int32 SquadId) const
{
    const FRuntimeSquad* Squad = Find(SquadId);
    return Squad ? Squad->Model.Mode : EHavocSquadMode::Patrol;
}

int32 UHavocSquadSubsystem::GetAliveCount(int32 SquadId) const
{
    const FRuntimeSquad* Squad = Find(SquadId);
    return Squad ? CountAlive(*Squad) : 0;
}

int32 UHavocSquadSubsystem::CountAlive(const FRuntimeSquad& Squad) const
{
    int32 Alive = 0;
    for (const TWeakObjectPtr<AHavocEnemyCharacter>& Member : Squad.Members)
    {
        Alive += (Member.IsValid() && !Member->IsDead()) ? 1 : 0;
    }
    return Alive;
}

void UHavocSquadSubsystem::PushMode(FRuntimeSquad& Squad) const
{
    AActor* Threat = Squad.Threat.Get();
    for (const TWeakObjectPtr<AHavocEnemyCharacter>& Member : Squad.Members)
    {
        if (!Member.IsValid() || Member->IsDead())
        {
            continue;
        }
        if (AHavocAIController* AI = Cast<AHavocAIController>(Member->GetController()))
        {
            AI->ApplySquadMode(Squad.Model.Mode, Threat);
        }
    }
}

void UHavocSquadSubsystem::Publish(const FRuntimeSquad& Squad) const
{
    const UWorld* World = GetWorld();
    const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UHavocEventSubsystem* Events = GI ? GI->GetSubsystem<UHavocEventSubsystem>() : nullptr;
    if (!Events)
    {
        return;
    }
    const TCHAR* Mode = Squad.Model.Mode == EHavocSquadMode::Engage ? TEXT("engage") : (Squad.Model.Mode == EHavocSquadMode::Fallback ? TEXT("fallback") : TEXT("patrol"));
    Events->Publish(TEXT("squad:mode"), FString::Printf(TEXT("{\"squad\":%d,\"team\":\"%s\",\"sector\":\"%s\",\"mode\":\"%s\"}"),
        Squad.Id, Squad.Team == EHavocTeam::Allied ? TEXT("allied") : TEXT("opposing"), *Squad.Sector.ToString(), Mode));
}

void UHavocSquadSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    const UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }
    TickAccumulator += DeltaTime;
    const HavocSquad::FTuning& Tuning = GetTuning();
    if (TickAccumulator < Tuning.SquadTickSeconds)
    {
        return;
    }
    TickAccumulator = 0.f;

    const double Now = World->GetTimeSeconds();
    for (int32 i = Squads.Num() - 1; i >= 0; --i)
    {
        FRuntimeSquad& Squad = Squads[i];
        const int32 Alive = CountAlive(Squad);
        if (Alive == 0)
        {
            Squads.RemoveAt(i); // wiped out (or the units were removed)
            continue;
        }
        if (Squad.Model.Update(Tuning, Now, Alive))
        {
            Publish(Squad);
        }
        PushMode(Squad);
    }
}
