#pragma once

// AI controller for Episode 04 enemy infantry (Iran, Houthi, ISIS from data/units).
//
// Perception: sight and hearing (UAIPerceptionComponent). Gunshots call UAISense_Hearing noise events
// and hits call NotifyShotBy, so units react to being shot from outside their view.
//
// Behaviour: patrol -> engage -> cover -> search, plus fallback for squads, ticked every ThinkInterval
// seconds. It is implemented in C++ because behaviour tree, StateTree and EQS assets are binary editor
// assets that cannot be committed from the repo. Squad members follow the squad mode set by
// UHavocSquadSubsystem (ApplySquadMode); units outside a squad keep acting alone.
//
// Designer-made logic: add a StateTreeAIComponent to a Blueprint child of this controller and assign a
// StateTree asset (preferred, see game/docs/ai.md), or assign BehaviorTreeAsset. The built-in logic then
// stays off; the StateTree drives the unit with the Blueprint-callable commands below. Help:
//   https://dev.epicgames.com/documentation/en-us/unreal-engine/state-tree-in-unreal-engine
//   https://dev.epicgames.com/documentation/en-us/unreal-engine/behavior-trees-in-unreal-engine
//   https://dev.epicgames.com/documentation/en-us/unreal-engine/ai-perception-in-unreal-engine

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/HavocSquadTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "HavocAIController.generated.h"

class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UBehaviorTree;
class UEnvQuery;
struct FEnvQueryResult;

UENUM(BlueprintType)
enum class EHavocAIState : uint8
{
    Patrol,
    Engage,
    Cover,
    Search,
    /** Squad fallback: run to cover away from the threat and regroup. */
    Fallback
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocAIController : public AAIController
{
    GENERATED_BODY()

public:
    AHavocAIController();

    /** Optional designer behaviour tree. When set, the built-in state machine does not run. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|AI")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|AI")
    float ThinkInterval = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Havoc|AI")
    float PatrolRadiusCm = 1800.f;

    UPROPERTY(BlueprintReadOnly, Category = "Havoc|AI")
    EHavocAIState State = EHavocAIState::Patrol;

    /** What the unit's squad is doing (Patrol when not in a squad). A designer StateTree reads this. */
    UPROPERTY(BlueprintReadOnly, Category = "Havoc|AI")
    EHavocSquadMode SquadMode = EHavocSquadMode::Patrol;

    /** A shooter hit this unit: turn toward it even if it is out of sight. */
    void NotifyShotBy(AActor* Shooter);

    /** Squad subsystem: the squad's mode changed (or, in Engage, the shared threat is refreshed). Member follows. */
    UFUNCTION(BlueprintCallable, Category = "Havoc|AI")
    void ApplySquadMode(EHavocSquadMode Mode, AActor* SharedThreat);

    /** The actor this unit is currently fighting or hiding from, or null. */
    UFUNCTION(BlueprintPure, Category = "Havoc|AI")
    AActor* GetThreat() const { return Target.Get(); }

    /** True for a living character on the other team (the player's Marine is Allied). */
    UFUNCTION(BlueprintPure, Category = "Havoc|AI")
    bool IsHostileToMe(const AActor* Actor) const;

    // Commands for a designer StateTree / Blueprint (they do the same as the built-in states).
    UFUNCTION(BlueprintCallable, Category = "Havoc|AI") void CommandPatrol() { EnterState(EHavocAIState::Patrol); }
    UFUNCTION(BlueprintCallable, Category = "Havoc|AI") void CommandEngage() { EnterState(EHavocAIState::Engage); }
    UFUNCTION(BlueprintCallable, Category = "Havoc|AI") void CommandFallback() { EnterState(EHavocAIState::Fallback); }

    // Exposed for the automation tests.
    static bool IsHostileTarget(const AActor* Actor);

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    UFUNCTION()
    void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
    void Think();
    void EnterState(EHavocAIState NewState);
    void DoPatrol(double Now);
    void DoEngage(double Now);
    void DoCover(double Now);
    void DoSearch(double Now);
    void DoFallback(double Now);
    void ReportThreatToSquad();
    void StartCoverQuery();
    void OnCoverQueryDone(TSharedPtr<FEnvQueryResult> Result);
    void ResolveCoverWithoutEqs();

    bool HasLineOfSight(const AActor* Target) const;
    bool FindCoverPoint(const AActor* Threat, FVector& OutPoint) const;
    bool PickPatrolPoint(FVector& OutPoint) const;
    void MoveOrSteer(const FVector& Destination, float AcceptRadius);
    float HealthFraction() const;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

    UPROPERTY()
    TObjectPtr<UEnvQuery> CoverQuery;

    TWeakObjectPtr<AActor> Target;
    EHavocSquadMode LastSquadMode = EHavocSquadMode::Patrol;
    /** False when a StateTree component or behaviour tree drives the unit instead. */
    bool bBuiltInLogic = false;
    bool bCoverReady = false;
    bool bCoverPending = false;
    FVector LastKnownLocation = FVector::ZeroVector;
    FVector HomeLocation = FVector::ZeroVector;
    FVector CoverPoint = FVector::ZeroVector;
    FVector PatrolPoint = FVector::ZeroVector;
    bool bHasPatrolPoint = false;
    double LastSeenTime = -1000.0;
    double StateEnteredTime = 0.0;
    double NextPatrolTime = 0.0;
    FVector LastMoveDestination = FVector::ZeroVector;
    double LastMoveRequestTime = -1000.0;
    bool bSteering = false;
    FTimerHandle ThinkTimer;
};
