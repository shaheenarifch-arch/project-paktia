#include "Networking/HavocGameState.h"
#include "Net/UnrealNetwork.h"

AHavocGameState::AHavocGameState()
{
    MatchPhase = EHavocMatchPhase::Waiting;
    AttackerScore = 0;
    DefenderScore = 0;
}

void AHavocGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AHavocGameState, MatchPhase);
    DOREPLIFETIME(AHavocGameState, AttackerScore);
    DOREPLIFETIME(AHavocGameState, DefenderScore);
}

void AHavocGameState::SetMatchPhase(
    EHavocMatchPhase NewPhase
)
{
    if (!HasAuthority())
    {
        return;
    }

    MatchPhase = NewPhase;
    OnRep_MatchPhase();
}

void AHavocGameState::OnRep_MatchPhase()
{
    // UI, audio, and mission presentation may react here.
    // Authoritative phase changes are executed by the server.
}
