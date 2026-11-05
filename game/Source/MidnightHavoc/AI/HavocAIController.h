#pragma once

// AI controller for Episode 04 enemy infantry (Iran, Houthi, ISIS from data/units).
//
// Perception: sight and hearing (UAIPerceptionComponent). Gunshots call UAISense_Hearing noise events
// and hits call NotifyShotBy, so units react to being shot from outside their view.
//
// Behaviour: patrol -> engage -> cover -> search, ticked every ThinkInterval seconds. This is the
// same patrol / cover / engage structure a behaviour tree would have; it is implemented in C++
// because behaviour tree and blackboard assets are binary editor assets that cannot be committed
// from the repo. To use a designer-made tree instead, assign BehaviorTreeAsset (e.g. in a Blueprint
// child); the built-in logic then stays off. Help:
//   https://dev.epicgames.com/documentation/en-us/unreal-engine/behavior-trees-in-unreal-engine
//   https://dev.epicgames.com/documentation/en-us/unreal-engine/ai-perception-in-unreal-engine

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "HavocAIController.generated.h"

class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UBehaviorTree;

UENUM(BlueprintType)
enum class EHavocAIState : uint8
{
    Patrol,
    Engage,
    Cover,
    Search
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

    /** A shooter hit this unit: turn toward it even if it is out of sight. */
    void NotifyShotBy(AActor* Shooter);

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

    bool HasLineOfSight(const AActor* Target) const;
    bool FindCoverPoint(const AActor* Threat, FVector& OutPoint) const;
    bool PickPatrolPoint(FVector& OutPoint) const;
    void MoveOrSteer(const FVector& Destination, float AcceptRadius);
    float HealthFraction() const;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

    TWeakObjectPtr<AActor> Target;
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
