#include <libultraship/bridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "ManualJump.h"
#include <SDL.h>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"

// Existing player actions/initializer; keep their implementation and physics in z_player.c.
void Player_Action_Idle(Player* player, PlayState* play);
void Player_Action_80842180(Player* player, PlayState* play); // untargeted walking/running
s32 func_8083A4A8(Player* player, PlayState* play);
s32 Player_ActionHandler_12(Player* player, PlayState* play);
int Player_IsZTargeting(Player* player);
}

#define CVAR_MANUAL_JUMP CVAR_ENHANCEMENT("ManualJump")
#define CVAR_VAULT_MODE CVAR_ENHANCEMENT("ManualJumpVaultMode")

static Player* sBufferedPlayer = nullptr;
static PlayState* sBufferedPlay = nullptr;
static uint32_t sVaultPressTime = 0;
static bool sVaultPressPending = false;
static constexpr uint32_t VAULT_BUFFER_MS = 180;

static void ClearVaultPress() {
    sVaultPressPending = false;
    sBufferedPlayer = nullptr;
    sBufferedPlay = nullptr;
}

static bool ManualVaultMode() {
    return CVarGetInteger(CVAR_MANUAL_JUMP, 0) && CVarGetInteger(CVAR_VAULT_MODE, 1) == 1;
}

static bool CanManualJump(Player* player, PlayState* play, Input* input) {
    if (player != GET_PLAYER(play) || input == nullptr ||
        (player->actionFunc != Player_Action_Idle && player->actionFunc != Player_Action_80842180)) {
        return false;
    }

    // An allowlist excludes rolls, attacks, climbing, landing, swimming and scripted actions.
    // Check mode flags as well: ordinary actions can remain installed during transitions.
    constexpr uint32_t blockedState1 =
        PLAYER_STATE1_LOADING | PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING |
        PLAYER_STATE1_START_CHANGING_HELD_ITEM | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_CARRYING_ACTOR |
        PLAYER_STATE1_CHARGING_SPIN_ATTACK | PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_SWINGING_BOTTLE |
        PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_CLIMBING_LADDER |
        PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_SHIELDING | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_DAMAGED |
        PLAYER_STATE1_IN_WATER | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_FLOOR_DISABLED;
    constexpr uint32_t blockedState2 =
        PLAYER_STATE2_GRABBED_BY_ENEMY | PLAYER_STATE2_MOVING_DYNAPOLY | PLAYER_STATE2_GRABBING_DYNAPOLY |
        PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING | PLAYER_STATE2_FROZEN | PLAYER_STATE2_PAUSE_MOST_UPDATING |
        PLAYER_STATE2_CRAWLING | PLAYER_STATE2_HOPPING | PLAYER_STATE2_OCARINA_PLAYING | PLAYER_STATE2_FORCED_VOID_OUT;

    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && player->actor.parent == nullptr &&
           !(player->stateFlags1 & blockedState1) && !(player->stateFlags2 & blockedState2) &&
           !(player->stateFlags3 & (PLAYER_STATE3_MIDAIR | PLAYER_STATE3_PAUSE_ACTION_FUNC |
                                    PLAYER_STATE3_FLYING_WITH_HOOKSHOT | PLAYER_STATE3_FORCE_PULL_OCARINA)) &&
           player->meleeWeaponState == 0 && player->itemAction == player->heldItemAction &&
           player->av2.actionVar2 == 0 && player->unk_6AD == 0 && !Player_IsZTargeting(player) &&
           !CHECK_BTN_ANY(input->cur.button, BTN_Z | BTN_R) && !Player_InBlockingCsMode(play, player) &&
           play->csCtx.state == CS_STATE_IDLE && play->pauseCtx.state == 0 && gSaveContext.health > 0;
}

// This is only a buffer/intent check. The player handler still decides whether
// the obstacle is actually vaultable using its original collision and timer checks.
static bool MovingIntoLowObstacle(Player* player) {
    const s16 wallYawDiff = player->yaw - (player->actor.wallYaw + 0x8000);
    // Types 2/3 are the existing short step-ups: the same fence can be a
    // low vault for adult Link and a step-up for child Link. Type 4 and
    // swimming exits retain their original climbing behavior.
    const bool lowObstacle = player->ledgeClimbType == 1 ||
                             ((player->ledgeClimbType == 2 || player->ledgeClimbType == 3) &&
                              player->yDistToLedge < player->ageProperties->unk_14 &&
                              !(player->stateFlags1 & PLAYER_STATE1_IN_WATER));
    return lowObstacle && player->linearVelocity > 0.0f &&
           (player->actor.bgCheckFlags & BGCHECKFLAG_PLAYER_WALL_INTERACT) && ABS(wallYawDiff) < 0x3000;
}

extern "C" int ManualJump_ShouldTraverseObstacle(Player* player, PlayState* play, Input* input) {
    if (!ManualVaultMode()) {
        ClearVaultPress();
        return true;
    }
    if (!CanManualJump(player, play, input) || !MovingIntoLowObstacle(player)) {
        ClearVaultPress();
        return false;
    }
    const bool buffered = sVaultPressPending && sBufferedPlayer == player && sBufferedPlay == play &&
                          uint32_t(SDL_GetTicks() - sVaultPressTime) <= VAULT_BUFFER_MS;
    if (CHECK_BTN_ALL(input->press.button, BTN_CUSTOM_JUMP) || buffered) {
        ClearVaultPress();
        return true;
    }
    return false;
}

static bool TryManualJump(Player* player, PlayState* play, Input* input) {
    if (CanManualJump(player, play, input)) {
        const bool pressed = CHECK_BTN_ALL(input->press.button, BTN_CUSTOM_JUMP);
        if (ManualVaultMode() && MovingIntoLowObstacle(player)) {
            // Idle's ordinary handler list omits handler 12. Reuse that
            // same native handler here rather than duplicating vault logic.
            if (Player_ActionHandler_12(player, play)) {
                return true;
            }
            if (pressed) {
                sBufferedPlayer = player;
                sBufferedPlay = play;
                sVaultPressTime = SDL_GetTicks();
                sVaultPressPending = true;
            }
            if (sVaultPressPending && sBufferedPlayer == player && sBufferedPlay == play) {
                if (uint32_t(SDL_GetTicks() - sVaultPressTime) <= VAULT_BUFFER_MS) {
                    return false;
                }
                // No native vault became eligible: resolve the deferred
                // press as a normal jump instead of swallowing it.
            } else if (!pressed) {
                return false;
            }
        } else {
            ClearVaultPress();
            if (!pressed) {
                return false;
            }
        }
        ClearVaultPress();
        // The vanilla initializer selects the animation and vertical impulse using linearVelocity.
        // It does not impose a minimum horizontal speed or change yaw: zero stays zero.
        // Subsequent frames use Player_Action_8084411C and unmodified vanilla air control.
        player->fallStartHeight = player->actor.world.pos.y;
        player->fallDistance = 0;
        func_8083A4A8(player, play);
        // The normal-boots formula gives only 1.25 upward speed at rest. Use the
        // existing 6.0 jump impulse as a manual-only minimum, never a horizontal boost.
        if (player->actor.velocity.y < 6.0f) {
            player->actor.velocity.y = 6.0f;
        }
        return true;
    } else {
        ClearVaultPress();
    }
    return false;
}

static void RegisterManualJump() {
    ClearVaultPress();
    COND_VB_SHOULD(VB_PLAYER_AUTO_LEDGE_JUMP, CVarGetInteger(CVAR_MANUAL_JUMP, 0), {
        if (CVarGetInteger(CVAR_MANUAL_JUMP, 0)) {
            *should = false;
        }
    });
    COND_VB_SHOULD(VB_PLAYER_TRY_MANUAL_JUMP, CVarGetInteger(CVAR_MANUAL_JUMP, 0), {
        Player* player = va_arg(args, Player*);
        PlayState* play = va_arg(args, PlayState*);
        Input* input = va_arg(args, Input*);
        if (!*should && CVarGetInteger(CVAR_MANUAL_JUMP, 0)) {
            *should = TryManualJump(player, play, input);
        } else {
            ClearVaultPress();
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterManualJump, { CVAR_MANUAL_JUMP, CVAR_VAULT_MODE });
