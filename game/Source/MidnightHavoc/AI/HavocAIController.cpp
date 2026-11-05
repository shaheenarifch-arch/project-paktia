#include "AI/HavocAIController.h"

#include "AI/HavocEnemyCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Characters/HavocCharacter.h"
#include "Combat/HavocWeaponComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Player/HavocPlayerCharacter.h"
#include "TimerManager.h"

AHavocAIController::AHavocAIController()
{
    SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception")));

    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 5500.f;
    SightConfig->LoseSightRadius = 6200.f;
    SightConfig->PeripheralVisionAngleDegrees = 100.f;
    SightConfig->SetMaxAge(4.f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 4500.f;
    HearingConfig->SetMaxAge(5.f);
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

    PerceptionComponent->ConfigureSense(*SightConfig);
    PerceptionComponent->ConfigureSense(*HearingConfig);
    PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

    bAllowStrafe = true;
}

bool AHavocAIController::IsHostileTarget(const AActor* Actor)
{
    const AHavocPlayerCharacter* Player = Cast<AHavocPlayerCharacter>(Actor);
    return Player && !Player->IsDead();
}

void AHavocAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (!InPawn)
    {
        return;
    }
    HomeLocation = InPawn->GetActorLocation();

    if (const AHavocEnemyCharacter* Enemy = Cast<AHavocEnemyCharacter>(InPawn))
    {
        SightConfig->SightRadius = Enemy->SightRadiusCm;
        SightConfig->LoseSightRadius = Enemy->SightRadiusCm + 700.f;
        PerceptionComponent->ConfigureSense(*SightConfig);
    }
    PerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AHavocAIController::OnTargetPerceptionUpdated);

    if (BehaviorTreeAsset)
    {
        RunBehaviorTree(BehaviorTreeAsset);
        return;
    }
    if (UWorld* World = GetWorld())
    {
        // Random start offset so squads do not think in lock-step.
        World->GetTimerManager().SetTimer(ThinkTimer, this, &AHavocAIController::Think, ThinkInterval, true, FMath::FRandRange(0.f, ThinkInterval));
    }
}

void AHavocAIController::OnUnPossess()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ThinkTimer);
    }
    if (PerceptionComponent)
    {
        PerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(this, &AHavocAIController::OnTargetPerceptionUpdated);
    }
    Super::OnUnPossess();
}

void AHavocAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!IsHostileTarget(Actor) || !Stimulus.WasSuccessfullySensed())
    {
        return;
    }
    Target = Actor;
    LastKnownLocation = Stimulus.StimulusLocation;
    if (const UWorld* World = GetWorld())
    {
        LastSeenTime = World->GetTimeSeconds();
    }
}

void AHavocAIController::NotifyShotBy(AActor* Shooter)
{
    if (!IsHostileTarget(Shooter))
    {
        return;
    }
    Target = Shooter;
    LastKnownLocation = Shooter->GetActorLocation();
    if (const UWorld* World = GetWorld())
    {
        LastSeenTime = World->GetTimeSeconds();
    }
}

float AHavocAIController::HealthFraction() const
{
    const AHavocCharacter* Self = Cast<AHavocCharacter>(GetPawn());
    return Self && Self->MaximumHealth > 0.f ? Self->Health / Self->MaximumHealth : 1.f;
}

bool AHavocAIController::HasLineOfSight(const AActor* Other) const
{
    const APawn* Self = GetPawn();
    if (!Self || !Other || !GetWorld())
    {
        return false;
    }
    const FVector From = Self->GetActorLocation() + FVector(0.f, 0.f, Self->BaseEyeHeight);
    const FVector To = Other->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HavocAILos), false, Self);
    Params.AddIgnoredActor(Other);
    return !GetWorld()->LineTraceTestByChannel(From, To, ECC_Visibility, Params);
}

void AHavocAIController::Think()
{
    AHavocCharacter* Self = Cast<AHavocCharacter>(GetPawn());
    const UWorld* World = GetWorld();
    if (!Self || Self->IsDead() || !World)
    {
        return;
    }
    const double Now = World->GetTimeSeconds();

    if (Target.IsValid() && !IsHostileTarget(Target.Get()))
    {
        Target.Reset();
    }
    if (State == EHavocAIState::Patrol && Target.IsValid())
    {
        EnterState(EHavocAIState::Engage);
    }

    switch (State)
    {
    case EHavocAIState::Patrol: DoPatrol(Now); break;
    case EHavocAIState::Engage: DoEngage(Now); break;
    case EHavocAIState::Cover: DoCover(Now); break;
    case EHavocAIState::Search: DoSearch(Now); break;
    }
}

void AHavocAIController::EnterState(EHavocAIState NewState)
{
    State = NewState;
    StateEnteredTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    bHasPatrolPoint = false;
    if (NewState != EHavocAIState::Engage)
    {
        ClearFocus(EAIFocusPriority::Gameplay);
    }
}

void AHavocAIController::DoPatrol(double Now)
{
    if (!bHasPatrolPoint)
    {
        if (Now < NextPatrolTime || !PickPatrolPoint(PatrolPoint))
        {
            return;
        }
        bHasPatrolPoint = true;
    }
    const APawn* Self = GetPawn();
    if (FVector::DistSquared2D(Self->GetActorLocation(), PatrolPoint) < FMath::Square(150.f))
    {
        bHasPatrolPoint = false;
        NextPatrolTime = Now + FMath::FRandRange(1.f, 3.f);
        return;
    }
    SetFocalPoint(PatrolPoint);
    MoveOrSteer(PatrolPoint, 100.f);
}

void AHavocAIController::DoEngage(double Now)
{
    AActor* Foe = Target.Get();
    AHavocEnemyCharacter* Self = Cast<AHavocEnemyCharacter>(GetPawn());
    if (!Foe || !Self)
    {
        EnterState(EHavocAIState::Patrol);
        return;
    }

    const bool bSees = HasLineOfSight(Foe)
        && FVector::Dist(Self->GetActorLocation(), Foe->GetActorLocation()) <= Self->SightRadiusCm;
    if (bSees)
    {
        LastSeenTime = Now;
        LastKnownLocation = Foe->GetActorLocation();
    }

    // Hurt or reloading: get behind cover first.
    if ((HealthFraction() < 0.4f || Self->Weapon->bReloading) && Now - StateEnteredTime > 1.f)
    {
        FVector Point;
        if (FindCoverPoint(Foe, Point))
        {
            CoverPoint = Point;
            EnterState(EHavocAIState::Cover);
            return;
        }
    }

    if (!bSees)
    {
        if (Now - LastSeenTime > 5.0)
        {
            EnterState(EHavocAIState::Search);
            return;
        }
        MoveOrSteer(LastKnownLocation, 200.f);
        return;
    }

    SetFocus(Foe, EAIFocusPriority::Gameplay);
    const float Dist = FVector::Dist(Self->GetActorLocation(), Foe->GetActorLocation());
    if (Dist > Self->EngageRangeCm)
    {
        MoveOrSteer(Foe->GetActorLocation(), Self->EngageRangeCm * 0.7f);
    }
    else
    {
        StopMovement();
    }

    // Bursts: about a second of fire, then a short pause.
    const double Phase = FMath::Fmod(Now, 2.2);
    if (Phase < 1.0)
    {
        const FVector Origin = Self->GetActorLocation() + FVector(0.f, 0.f, Self->BaseEyeHeight);
        const FRotator Aim = (Foe->GetActorLocation() + FVector(0.f, 0.f, 40.f) - Origin).Rotation();
        static int64 EnemySequence = 1;
        Self->Weapon->TryFireFromAim(Origin, Aim.Yaw, Aim.Pitch, EnemySequence++);
    }
}

void AHavocAIController::DoCover(double Now)
{
    const APawn* Self = GetPawn();
    const bool bArrived = FVector::DistSquared2D(Self->GetActorLocation(), CoverPoint) < FMath::Square(150.f);
    if (!bArrived && Now - StateEnteredTime < 6.0)
    {
        MoveOrSteer(CoverPoint, 100.f);
        return;
    }
    StopMovement();
    // Stay behind cover a few seconds (reload, recover nerve), then peek again.
    if (Now - StateEnteredTime > 4.0)
    {
        EnterState(Target.IsValid() ? EHavocAIState::Engage : EHavocAIState::Search);
    }
}

void AHavocAIController::DoSearch(double Now)
{
    const APawn* Self = GetPawn();
    if (Now - StateEnteredTime > 10.0)
    {
        Target.Reset();
        EnterState(EHavocAIState::Patrol);
        return;
    }
    if (FVector::DistSquared2D(Self->GetActorLocation(), LastKnownLocation) > FMath::Square(250.f))
    {
        SetFocalPoint(LastKnownLocation);
        MoveOrSteer(LastKnownLocation, 150.f);
    }
    else
    {
        StopMovement();
    }
}

bool AHavocAIController::FindCoverPoint(const AActor* Threat, FVector& OutPoint) const
{
    const APawn* Self = GetPawn();
    UWorld* World = GetWorld();
    if (!Self || !Threat || !World)
    {
        return false;
    }
    const FVector Origin = Self->GetActorLocation();
    const FVector ThreatEye = Threat->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    float BestScore = TNumericLimits<float>::Max();
    bool bFound = false;
    for (const float Radius : { 600.f, 1200.f })
    {
        for (int32 i = 0; i < 12; ++i)
        {
            FVector Candidate = Origin + FVector(FMath::Cos(i * PI / 6.f), FMath::Sin(i * PI / 6.f), 0.f) * Radius;
            if (Nav)
            {
                FNavLocation Projected;
                if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(200.f, 200.f, 300.f)))
                {
                    continue;
                }
                Candidate = Projected.Location;
            }
            FCollisionQueryParams Params(SCENE_QUERY_STAT(HavocCover), false, Self);
            Params.AddIgnoredActor(Threat);
            // Cover = something solid between the candidate (chest height) and the threat.
            if (World->LineTraceTestByChannel(Candidate + FVector(0.f, 0.f, 90.f), ThreatEye, ECC_Visibility, Params))
            {
                const float Score = FVector::Dist(Origin, Candidate);
                if (Score < BestScore)
                {
                    BestScore = Score;
                    OutPoint = Candidate;
                    bFound = true;
                }
            }
        }
        if (bFound)
        {
            break;
        }
    }
    return bFound;
}

bool AHavocAIController::PickPatrolPoint(FVector& OutPoint) const
{
    UWorld* World = GetWorld();
    if (UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr)
    {
        FNavLocation Result;
        if (Nav->GetRandomReachablePointInRadius(HomeLocation, PatrolRadiusCm, Result))
        {
            OutPoint = Result.Location;
            return true;
        }
    }
    // No navmesh (see game/docs/kharg-level-guide.md): wander on the flat around home.
    const FVector2D Offset = FMath::RandPointInCircle(PatrolRadiusCm);
    OutPoint = HomeLocation + FVector(Offset.X, Offset.Y, 0.f);
    return true;
}

void AHavocAIController::MoveOrSteer(const FVector& Destination, float AcceptRadius)
{
    const UWorld* World = GetWorld();
    APawn* Self = GetPawn();
    if (!World || !Self)
    {
        return;
    }
    const double Now = World->GetTimeSeconds();
    const bool bSameGoal = FVector::DistSquared(Destination, LastMoveDestination) < FMath::Square(150.f);
    if (!bSteering && bSameGoal && Now - LastMoveRequestTime < 1.0 && GetMoveStatus() == EPathFollowingStatus::Moving)
    {
        return; // already walking there; do not restart the path every think
    }
    if (!bSteering || !bSameGoal)
    {
        const EPathFollowingRequestResult::Type Result = MoveToLocation(Destination, AcceptRadius, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ true, /*bCanStrafe*/ true);
        LastMoveDestination = Destination;
        LastMoveRequestTime = Now;
        bSteering = (Result == EPathFollowingRequestResult::Failed);
    }
    if (bSteering)
    {
        // No path (no navmesh or off the mesh): walk straight at it.
        Self->AddMovementInput((Destination - Self->GetActorLocation()).GetSafeNormal2D(), 1.f);
    }
}
