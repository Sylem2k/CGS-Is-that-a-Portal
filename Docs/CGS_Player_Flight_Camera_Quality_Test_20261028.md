# Player flight camera quality — Workbench probe (2026-10-28)

**Status: source-reviewed, NOT Workbench-compiled or played. This PR measures camera motion only; it does NOT smooth player movement.**

## Why I am running this test

My Blue/Orange Portal Gun now performs repeatable offline floor→floor gravity loops. I verified PR #68 with 108/108 accepted owner-local adaptive flight pulses, 7 automatic near-surface return transfers, and stable portal PIP camera slots after PR #67. The log confirms ACTUAL upward motion, but the player still moves in `SCR_Global.TeleportLocalPlayer` requests, not continuous CharacterController velocity. The video before this test looked visibly stepped.

This is a measured investigation before I change the player-view or try another movement mode. I want to know:

1. Does the **native first-person camera jump exactly when the character's position jumps**?
2. Does the camera lag the character or produce additional discontinuities?
3. How many physics/update frames actually contain upward character movement, versus stationary frames between ~40ms pulses?
4. Can I solve some visible stutter with a future **camera-only interpolation** test, without introducing errors into the known-good collision/portal controller?

## What changes in this PR

- `Scripts/Game/CGS_Portal/CGS_PortalComponent.c`: adds offline **`m_bFlightCameraQualityProbe`** editor checkbox. The default is OFF outside these two gun portal prefabs.
- `Prefabs/Weapons/Core/Blue_Portal.et` and `Orange_Portal.et`: set `m_bFlightCameraQualityProbe 1` for a test run. No model, GUID, Rpl or physics changes.
- `Scripts/Game/CGS_Portal/CGS_PortalTransferComponent.c`: forwards the opt-in flag along with the existing floor-loop settings. It only runs when the current floor loop already qualifies for **OFFLINE** mode.
- `Scripts/Game/CGS_Portal/CGS_PortalMomentumService.c`: adds **read-only** `SampleFloorFlightCamera` and an apex summary:
  - Reads actual player origin via `IEntity.GetOrigin()`.
  - Reads the actual local camera matrix via `BaseWorld.GetCurrentCamera()`.
  - Uses the local player's owner check before sampling and skips missing camera.
  - Counts frames with upward character motion, upward camera motion, and virtually stationary player frames.
  - Measures maximum single-frame vertical player step, camera step, and difference between the two.
  - Omits long editor stalls (>150ms) from the largest-step measurements.
  - One detailed `[CGS FLIGHT VIEW]` message around every 180ms and one `[CGS FLIGHT QUALITY]` summary at each Apex.
  - **No calls to camera setters, TeleportPlayer, SetOrigin, Physics.SetVelocity, CharacterController setters, RPCs or input handlers.**
- Existing proven movement is unchanged: adaptive 40ms pulse minimum, collision sweep, asynchronous move acknowledgment, fallback to 75ms and the existing near-surface return gate.

## Workbench test

1. Pull the branch, compile scripts and load offline `unnamed.ent`. Place two UP-facing flat floor portals under open sky.
2. Enter a portal and release WASD. Repeat for 3–5 full automatic Blue↔Orange loops. The existing transfer sequence remains `Pending arrival → Start AFTER ARRIVAL → [CGS FLOOR TP PULSE] → [CGS FLOOR TP VERIFY] moved=1 → Apex → NEAR SURFACE → FALL_ZONE`.
3. Expect `[CGS FLIGHT VIEW START]` immediately after a confirmed teleport, and `[CGS FLIGHT VIEW]` for ~180ms-spaced samples.
4. Every apex should include one `[CGS FLIGHT QUALITY] frames=... charUpFrames=... cameraUpFrames=... charStallFrames=... maxCharStepY=... maxCameraStepY=... maxViewMismatchY=... adaptive=1`.
5. Send the Workbench log and, if available, **a 5–15 second first-person screen recording**. Logs can quantify transform motion, but ONLY the video can prove the view looks smoother or worse.
6. Confirm no `FLOOR LOOP Cancel`, no `CGS FLOOR ADAPTIVE Fallback`, same real ascent and same near-surface return depth ~0.8–1.1m, both PIP live views, and F-grab / V-throw cube unaffected.
7. If Workbench has a compile error, send the exact line and message. **This PR was statically reviewed only and is not claimed to compile yet.**
8. To disable the probe while retaining all flight behavior, uncheck `m_bFlightCameraQualityProbe` on both portal prefab instances. It is observation-only.

## Interpreting the results

- **maxCharStepY ≈ maxCameraStepY and maxViewMismatchY small**: native camera mostly follows the character's discontinuous teleports. Smoothness requires either a supported native movement mode (ideal) or a carefully tested **visual-only** interpolation overlay for first-person view. Neither is implemented in this PR.
- **maxCameraStepY much larger than maxCharStepY**: investigate camera transitions/mount, PIP/exposure or view stabilization; it is not simply an upward physics problem.
- **charStallFrames high** relative to frames: shows why motion looks stepped even when every 40ms request is accepted.
- **zero camera samples** but normal `[CGS FLOOR TP VERIFY]`: camera manager did not expose a local active camera to this server-side offline observer. Move the same read-only sampling onto the local player-controller callback in a *separate* PR; do not guess that no jitter occurred.
- **Camera data is noisy because the camera is head-bob animated**: compare repeated arcs at the same speed and inspect vertical-only results, not absolute camera offset.

## What I will do after results

I still want a real Portal-style experience, with continuously integrated 3D velocity, natural floor→floor/floor→wall/wall→wall momentum, camera orientation transitions, and an optional **portal-aperture pre-contact crossing** that simulates a hole in otherwise solid terrain. But the native Reforger player controller is non-dynamic in my Workbench tests, and `Physics.SetVelocity`, `BaseGameEntity.Teleport` microsteps and `IEntity.SetWorldTransform` on the player previously failed. Do not silently re-enable those paths. Keep the verified cube travel and offline loop as a fallback; dedicated multiplayer remains postponed.
