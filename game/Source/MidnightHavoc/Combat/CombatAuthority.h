#pragma once

#include "CoreMinimal.h"

// Design contract for the Unreal implementation.
// The dedicated server owns damage, health, ammunition, weapon state,
// objective state, vehicle state, aircraft state, rewards, and match result.

namespace MidnightHavoc
{
    struct FFireIntent
    {
        int64 InputSequence;
        int32 WeaponSlot;
        float AimYaw;
        float AimPitch;
    };

    class ICombatAuthority
    {
    public:
        virtual ~ICombatAuthority() = default;
        virtual bool ValidateFireIntent(const FFireIntent& Intent) = 0;
        virtual void ApplyValidatedFireIntent(const FFireIntent& Intent) = 0;
    };
}
