#pragma once

#include "CoreMinimal.h"

namespace HavocE04
{
    /** Math.round(x * 1000) / 1000 from the TS (rounds .5 up). */
    inline double Round3(double Value)
    {
        return FMath::FloorToDouble(Value * 1000.0 + 0.5) / 1000.0;
    }
}
