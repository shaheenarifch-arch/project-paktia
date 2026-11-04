#pragma once

// Native interface for world actors a drone sensor can detect (for example step 4's lane hazards).
// AHavocDrone calls OnDetectedByDrone and publishes "drone:detected" on UHavocEventSubsystem, so an
// actor can either implement this interface (and call its own Reveal() in OnDetectedByDrone) or just
// subscribe to the event and match on hazardId. Neither side depends on the other's headers.

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HavocDroneDetectable.generated.h"

UINTERFACE(MinimalAPI)
class UHavocDroneDetectable : public UInterface
{
    GENERATED_BODY()
};

class MIDNIGHTHAVOC_API IHavocDroneDetectable
{
    GENERATED_BODY()

public:
    /** Stable id used as hazardId in the drone:detected payload. */
    virtual FName GetDetectableId() const = 0;

    /** Maps to SensorCandidate.visible. */
    virtual bool IsDetectableVisible() const = 0;

    /** Maps to SensorCandidate.classification. */
    virtual FString GetDetectableClassification() const = 0;

    /** Called once per drone and detectable, the first time the sensor picks it up. */
    virtual void OnDetectedByDrone(FName DroneId) {}
};
