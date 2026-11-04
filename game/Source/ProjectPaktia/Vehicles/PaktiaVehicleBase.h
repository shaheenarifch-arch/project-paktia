#pragma once

// Ports: data/vehicles/warden-tank.json and data/vehicles/nomad-transport.json
// Stub: data-driven vehicle stats (Warden, Nomad) will be loaded here.

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PaktiaVehicleBase.generated.h"

UCLASS()
class PROJECTPAKTIA_API APaktiaVehicleBase : public APawn
{
    GENERATED_BODY()
};
