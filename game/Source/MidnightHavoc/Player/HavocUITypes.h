#pragma once

#include "CoreMinimal.h"
#include "HavocUITypes.generated.h"

UENUM(BlueprintType)
enum class EHavocMenuMode : uint8
{
    Main,
    Pause,
    Settings,
    Results,
    Campaign,  // episode select: Main > Campaign > Start
    HowToPlay  // every key binding, read from the live bindings (Main and Pause menus)
};

/** First-time hints: shown once the first time the player takes control of each kind of machine. */
UENUM(BlueprintType)
enum class EHavocHintKind : uint8
{
    Drone,
    Vehicle,
    Helicopter
};
