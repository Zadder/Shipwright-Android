#ifndef SOH_MANUAL_JUMP_H
#define SOH_MANUAL_JUMP_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// Called only after native low-obstacle vault/short step-up eligibility checks pass.
// Automatic mode and a disabled Manual Jump enhancement always allow vanilla.
int ManualJump_ShouldTraverseObstacle(Player* player, PlayState* play, Input* input);

#ifdef __cplusplus
}
#endif

#endif
