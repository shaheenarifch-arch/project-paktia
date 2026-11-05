#pragma once

// Shared enums for AI squads (see game/docs/ai.md).

#include "CoreMinimal.h"
#include "HavocSquadTypes.generated.h"

/** Who a unit fights for. The player's Marine is Allied. ISIS is always Opposing (AI-only). */
UENUM(BlueprintType)
enum class EHavocTeam : uint8
{
    Allied,
    Opposing
};

/** What a whole squad is doing. Members follow it (see AHavocAIController::ApplySquadMode). */
UENUM(BlueprintType)
enum class EHavocSquadMode : uint8
{
    Patrol,
    Engage,
    Fallback
};

/** Patrol: normal squad (can fall back when weak). Assault: always presses the attack, never falls back (counter-attack raiders). */
UENUM(BlueprintType)
enum class EHavocSquadPosture : uint8
{
    Patrol,
    Assault
};
