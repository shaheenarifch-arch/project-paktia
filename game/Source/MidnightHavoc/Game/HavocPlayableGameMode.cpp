#include "Game/HavocPlayableGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Missions/Episode04/HavocEpisode04Flow.h"
#include "Player/HavocPlayerCharacter.h"
#include "Player/HavocPlayerController.h"

AHavocPlayableGameMode::AHavocPlayableGameMode()
{
    DefaultPawnClass = AHavocPlayerCharacter::StaticClass();
    PlayerControllerClass = AHavocPlayerController::StaticClass();
}

void AHavocPlayableGameMode::StartPlay()
{
    Super::StartPlay();
    UWorld* World = GetWorld();
    for (TActorIterator<AHavocEpisode04Flow> It(World); It; ++It)
    {
        return; // one placed in the level already
    }
    World->SpawnActor<AHavocEpisode04Flow>(AHavocEpisode04Flow::StaticClass(), FTransform::Identity);
}
