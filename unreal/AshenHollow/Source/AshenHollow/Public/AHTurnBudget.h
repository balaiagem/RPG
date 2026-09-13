#pragma once
#include "CoreMinimal.h"

/** Centimeters. Resources are renewed only by the encounter's turn transition. */
struct FAHTurnBudget
{
    float Movement = 900.f;
    bool bAction = true;
    bool bBonus = true;
    void Reset() { Movement = 900.f; bAction = bBonus = true; }
    bool SpendAction() { if (!bAction) return false; bAction = false; return true; }
    bool SpendBonus() { if (!bBonus) return false; bBonus = false; return true; }
    void Travel(float Centimeters) { Movement = FMath::Max(0.f, Movement - FMath::Max(0.f, Centimeters)); }
};
