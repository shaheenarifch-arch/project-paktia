#pragma once

#include "CoreMinimal.h"
#include "HavocUITypes.generated.h"

UENUM(BlueprintType)
enum class EHavocMenuMode : uint8
{
    Main,
    Pause,
    Settings,
    Results
};
