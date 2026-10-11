# Manual Jump: Android 9.3 handoff

Base: `linkzenic/Shipwright-Android`, release `v9.3.0-android.1`, commit
`547a7fd2013c2236c28ee5bfc15f4e13815f4552`. The release is on the
`android-9.3-integration` branch; `main` still contains the older 9.2.3 port.

This is an optional source enhancement, off by default. The Android APK is the
ready-to-play distribution.
Settings > Controls contains the Manual Jump checkbox and Manual Jump Bindings
shortcut. The shortcut opens the existing controller editor, selects Link (P1),
and expands Manual Jump. Its Jump row uses the normal keyboard/SDL mapper.
Bindings persist while disabled. Unchecking the setting restores default jumps.

Vaulting Mode appears immediately below the Jump binding and is saved as
`gEnhancements.ManualJumpVaultMode`: 0 = Automatic, 1 = Manual (default).
Existing saved choices are respected; the fallback for an unset mode is Manual.
Automatic preserves the native low-obstacle action and short step-ups. Manual
requires moving into a native low vault or short step-up candidate and pressing
Jump. This includes the short step-up used at the training fence by child Link.
Explicit A-button climbing, tall climbs, water exits, ladders and ledge grabs
retain their original behavior. The existing player
handler retains its collision classification, ground requirement, approach
timer, animation, vertical impulse, forward speed and subsequent movement.
Tall ledge climbing and grabs are not gated. Turning Manual Jump off always
restores automatic vaults, regardless of the saved mode.

A 180 ms buffer applies only while moving into an already detected low obstacle or short step-up
before its native approach timer completes. It never creates a vault candidate.
If the timer does not complete before expiry, the press resolves as a normal
jump. Leaving the obstacle or entering an ineligible state cancels it. Buffer
time uses SDL's monotonic millisecond clock with unsigned wraparound arithmetic.

Only normal untargeted grounded idle/walking/running can start a manual jump.
The enhancement calls the existing vanilla jump initializer and sets a minimum
upward velocity of 6.0 for this manual action only. It does not change horizontal
velocity or yaw. Subsequent frames use normal air control and collision handling.
Only the normal auto ledge-jump trigger is gated; falling, grabs, scripted actions
and the special water dive retain the existing implementation.

The two GameInteractor hooks are appended to GIVanillaBehavior. Jump occupies
the unused bit 24 in the controller action mask. PadMgr press/release casts now
use CONTROLLERBUTTONS_T, preserving high action bits rather than truncating them.
No engine-submodule changes are part of this feature. Android builds still need
the repository's existing libultraship Android patch, as its workflow specifies.

## Play the Android APK

Install `ManualJump-9.3.apk` on an arm64 Android device. It is a separate local
test build (`com.linkzenic.soh.localtest`), using `SOH93-Test` for game data.
Select your own supported legally dumped ROM during setup, then open
Settings > Controls, enable Manual Jump, and use Manual Jump Bindings to assign
Jump. The APK does not supply a ROM or extracted game archive. Uncheck Manual
Jump to restore the normal jump behavior. Vaulting Mode defaults to Manual;
choose Automatic to allow native low-obstacle vaults from movement alone.

## Apply and build upstream

The supplied patch is relative to the release commit above. On a clean checkout:

```sh
git checkout -b manual-jump v9.3.0-android.1
git apply --check /path/to/manual-jump-9.3.patch
git apply /path/to/manual-jump-9.3.patch
git submodule update --init --recursive
git -C libultraship apply ../CMake/patches/libultraship-android.patch
python tests/manual_jump/run.py -- c++
```

Use the existing `.github/workflows/android-standard-release.yml` build steps to
generate the matching 9.3 support archive and assemble the release APK. The
maintainer's workflow signs with the original release key and verifies its
certificate fingerprint. An upstream pull request should target
`android-9.3-integration`; merging/releasing remains the maintainer's decision.
The patch contains no signing key, game ROM, game archive, or dependencies.

For a separate local build, use the repository's `assembleDebug` target. Upstream
already gives it package `com.linkzenic.soh.localtest` and data folder `SOH93-Test`.
This cannot update the official release, which is `com.linkzenic.soh`. Replacing
the official app requires its original signing key, not simply its package name.

## Validation and limits

The focused C++ harness extracts the current production policy, player jump
initializer, action dispatch, ledge branch, and input edges directly from source.
It passes on this 9.3 port for stationary/moving takeoff, high input bits, disabled
vanilla behavior, fall/grab dispatch, water dives, forbidden states, and no held
repeat or second airborne jump. Engine services and geometry are test doubles.
The same harness now extracts the actual native obstacle/climb handler and
checks the unset Manual default, explicit Automatic and disabled automatic vaults, movement-only suppression in Manual
mode, native action priority and impulses, buffer acceptance/expiry/cancellation,
stationary/away/targeting rejection, normal-jump fallback and preserved tall,
A-button and water climbing.

The 9.3 APK was installed and run on an MI 8 with the user's supported NTSC 1.2
dump extracted through the app. In Kokiri Forest, adult Link stayed behind a
native low fence with movement alone in Manual mode, then crossed it using
movement plus the touch-overlay Y Jump binding. A second fence blocked movement
in Manual mode and allowed movement-only vaulting after selecting Automatic.
With Manual saved and Manual Jump unchecked, movement-only vaulting worked again.
The saved user fence is a short step-up for child Link. The first implementation
left that automatic climbing unchanged, which did not meet the user's expected
Manual behavior at this fence. The corrected version gates movement-triggered
native step-up types 2/3 below the age's tall-climb threshold, while preserving
explicit A climbing and tall/water actions. Mode and binding persistence are
checked separately from gameplay. These initial vault checks preceded the fix
and the change of the unset default
from Automatic to Manual. The rebuilt APK was installed again: after deleting only
the vault-mode CVar for verification, its UI showed Manual and retained the Y
binding. That earlier build's automatic child fence climbing motivated the
subsequent step-up correction. The player was returned to the saved fence position.
The production harness also covers the unset default and saved-choice behavior.

The corrected APK was rebuilt and installed on 2026-10-11. At the user's saved
Kokiri training fence with child Link, two movement-only attempts remained
outside in Manual mode. Moving forward and pressing the touch-overlay Y Jump
binding triggered the native step-up onto the fence. Selecting Automatic
allowed movement-only climbing at the same location. With Manual saved but
Manual Jump unchecked, movement-only climbing also worked. Manual Jump was
then enabled again, Manual mode and the Y binding retained, and child Link
returned to the saved position. Screenshots are in `phone-checks/fence-fix`.

The previous 9.2.3 implementation was also built and tested in actual gameplay on
an MI 8: SDL touch-overlay Y triggered a vertical stationary jump, moving takeoff
kept speed/yaw, holding through landing did not repeat, an airborne press did not
jump again, and disabling the feature suppressed manual jumping. This prior
hardware evidence must not be presented as exhaustive 9.3 testing.

Real ledge geometry, both ages, physical GameSir hardware, boots, platforms and
interactions with other movement enhancements still warrant broader gameplay QA.
This contribution is a handoff, not a commitment to maintain a separate fork.
