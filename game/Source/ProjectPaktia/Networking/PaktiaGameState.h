#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PaktiaGameState.generated.h"

UENUM(BlueprintType)
enum class EPaktiaMatchPhase : uint8
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
class PROJECTPAKTIA_API APaktiaGameState
    : public AGameStateBase
{
    GENERATED_BODY()

public:
    APaktiaGameState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UPROPERTY(
        BlueprintReadOnly,
        ReplicatedUsing = OnRep_MatchPhase,
        Category = "Project Paktia|Match"
    )
    EPaktiaMatchPhase MatchPhase;

    UPROPERTY(
        BlueprintReadOnly,
        Replicated,
        Category = "Project Paktia|Match"
    )
    int32 AttackerScore;

    UPROPERTY(
        BlueprintReadOnly,
        Replicated,
        Category = "Project Paktia|Match"
    )
    int32 DefenderScore;

    void SetMatchPhase(EPaktiaMatchPhase NewPhase);

protected:
    UFUNCTION()
    void OnRep_MatchPhase();
};
