#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "HavocGameState.generated.h"

UENUM(BlueprintType)
enum class EHavocMatchPhase : uint8
{
    Waiting,
    OpenCity,
    BreakCommunications,
    ControlOldGardun,
    NeutralizeHighGround,
    AssaultCitadel,
    Completed
};

UCLASS()
class MIDNIGHTHAVOC_API AHavocGameState
    : public AGameStateBase
{
    GENERATED_BODY()

public:
    AHavocGameState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(
        BlueprintReadOnly,
        ReplicatedUsing = OnRep_MatchPhase,
        Category = "Operation Midnight Havoc|Match"
    )
    EHavocMatchPhase MatchPhase;

    UPROPERTY(
        BlueprintReadOnly,
        Replicated,
        Category = "Operation Midnight Havoc|Match"
    )
    int32 AttackerScore;

    UPROPERTY(
        BlueprintReadOnly,
        Replicated,
        Category = "Operation Midnight Havoc|Match"
    )
    int32 DefenderScore;

    void SetMatchPhase(EHavocMatchPhase NewPhase);

protected:
    UFUNCTION()
    void OnRep_MatchPhase();
};
