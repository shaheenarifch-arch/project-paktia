#include "ProjectPaktiaGameMode.h"

#include "Characters/PaktiaCharacter.h"
#include "Networking/PaktiaGameState.h"

AProjectPaktiaGameMode::AProjectPaktiaGameMode()
{
    bUseSeamlessTravel = true;

    GameStateClass = APaktiaGameState::StaticClass();
    DefaultPawnClass = APaktiaCharacter::StaticClass();
}
