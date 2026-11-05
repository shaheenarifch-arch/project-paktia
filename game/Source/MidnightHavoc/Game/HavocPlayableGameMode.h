#pragma once

// Game mode for the playable Episode 04 level: third-person Marine, player controller with menus and
// HUD, and the playable loop actor (AHavocEpisode04Flow). Set as GlobalDefaultGameMode in
// Config/DefaultEngine.ini. Help:
//   https://dev.epicgames.com/documentation/en-us/unreal-engine/game-mode-and-game-state-in-unreal-engine

#include "CoreMinimal.h"
#include "MidnightHavocGameMode.h"
#include "HavocPlayableGameMode.generated.h"

UCLASS()
class MIDNIGHTHAVOC_API AHavocPlayableGameMode : public AMidnightHavocGameMode
{
    GENERATED_BODY()

public:
    AHavocPlayableGameMode();

    virtual void StartPlay() override;
};
