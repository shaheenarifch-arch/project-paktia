#include "Networking/PaktiaGameState.h"
#include "Net/UnrealNetwork.h"

APaktiaGameState::APaktiaGameState()
{
    MatchPhase = EPaktiaMatchPhase::Waiting;
    AttackerScore = 0;
    DefenderScore = 0;
}

void APaktiaGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(APaktiaGameState, MatchPhase);
    DOREPLIFETIME(APaktiaGameState, AttackerScore);
    DOREPLIFETIME(APaktiaGameState, DefenderScore);
}

void APaktiaGameState::SetMatchPhase(
    EPaktiaMatchPhase NewPhase
)
{
    if (!HasAuthority())
    {
        return;
    }

    MatchPhase = NewPhase;
    OnRep_MatchPhase();
}

void APaktiaGameState::OnRep_MatchPhase()
{
    // UI, audio, and mission presentation may react here.
    // Authoritative phase changes are executed by the server.
}
