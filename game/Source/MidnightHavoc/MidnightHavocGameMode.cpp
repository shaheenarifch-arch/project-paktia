#include "MidnightHavocGameMode.h"

#include "Characters/HavocCharacter.h"
#include "Networking/HavocGameState.h"

AMidnightHavocGameMode::AMidnightHavocGameMode()
{
    bUseSeamlessTravel = true;

    GameStateClass = AHavocGameState::StaticClass();
    DefaultPawnClass = AHavocCharacter::StaticClass();
}
