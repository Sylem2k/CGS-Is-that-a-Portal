# Adaptive Player Flight — Offline Workbench Experiment (2026-10-28)

## Runtime result — merged PR #68, verified 2026-10-28 06:42

**The code compiled and the experimental adaptive option ran successfully
in offline Workbench** using `Pasted text(20261028-064258).txt`
(981 lines). I recorded 108/108 accepted movement requests and
108/108 `moved=1` verifications, all `adaptive=1`, with **no**
automatic fallbacks, refusals, or floor-loop cancellations. The
confirmation latency was 18–50ms (28.5ms average), requested upward
steps 0.06–0.26m, 8 real ascent apexes and 7 consecutive successful
`FALL_ZONE` return transfers (0.833–1.098m from the portal face).
The last apex ends with a manual Workbench reload, not a failed return.
Both live PIP cameras remained active and in their original camera
slots until that reload, confirming the PR #67 rank-swap mitigation
in this test.

The 7m/s launch cap produces around **2.02–2.06m observed vertical
rise** rather than the **2.50m ballistic ideal**. This discrepancy
needs attention in the next movement architecture, without
destabilizing proven portal transfers.

**Do not equate logged movement success with a smooth camera.**
This log has no new gameplay recording, and even adaptive mode uses
separate local teleports. Native continuous character flight and a
water-like portal aperture are separate, unverified goals.

**Status: movement verified in offline Workbench after merge; visual smoothness unverified. NOT a native character-controller flight mode.**

## Goal

I want the floor-to-floor player bounce to feel like the cube: real gravity, real momentum, smooth upward arc, smooth camera, no teleport-step jitter. PR #66 proves the BASIC loop works (11 near-surface `FALL_ZONE` returns; 83/83 applied movement confirmations). PR #67 stabilized the live PIP camera slots. This test isolates only the remaining **player ascent step cadence**.

## Why this must be an experiment

Arma Reforger's player character uses a non-dynamic `CharacterControllerComponent`. In this project:
- `Physics.SetVelocity()` has not moved the non-dynamic player.
- `BaseGameEntity.Teleport()` incremental moves and `IEntity.SetWorldTransform()` direct moves produced `afterY==beforeY` in Workbench (PRs #61/#63).
- `SCR_Global.TeleportLocalPlayer()` successfully moves the local player (PR #64), but the existing **75ms / max 0.28m** pulses create obvious steps and camera movement.
- The normal `FALL_ZONE` return and fall-speed observer are working; **do not change** portal geometry, rearm behavior, cube, MP routing, or input while evaluating ascent feel.

## This branch: opt-in adaptive host flight

Files:
- `CGS_PortalComponent.c` adds `m_bAdaptiveFloorFlight` editor flag, **OFF by default on other prefabs**.
- Both existing `Prefabs/Weapons/Core/Blue_Portal.et` and `Orange_Portal.et` turn that flag ON for this offline-only A/B test. I can turn either portal's flag OFF in Workbench to restore prior flight on that exit.
- `CGS_PortalTransferComponent.c` passes the destination portal flag through the already-existing, strictly offline floor-loop gate.
- `CGS_PortalMomentumService.c`: when opted in, attempt at most one owner-local upward pulse each **40ms** (rather than 75ms), only after confirming the previous pulse really moved the character. Compute maximum step from the current upward trajectory speed: `clamp(vUp * 0.04 + 0.025, 0.06, 0.26)` meters. This uses shorter steps near the apex, while preserving stronger speed at the start.
- Asynchronous request verification: inspect the actual player height on later frames; accept a confirmed movement after at least 18ms. Wait at most 120ms for the result. **Do not overlap** pending teleport requests.
- **Automatic fallback**: if two adaptive movement requests fail to move, or if the engine refuses an adaptive teleport, switch back to the verified legacy **75ms / 0.28m** path for the remainder of THAT arc. Legacy still cancels after three subsequent failures. Any player can escape the loop laterally.
- This is still an **incremental teleport** approach and may be visually stepped; at 40ms (25 updates/sec) it is NOT true 60+FPS native character velocity. It could create more visible camera jitter or undershoot on some machines. This is why it is fully opt-in and reversible.

## Workbench test — verify, do not assume

1. Pull the branch and compile scripts, in offline `unnamed.ent` as before. Keep Blue and Orange both up-facing flat on road with clear sky.
2. Walk into one, release WASD. Look for `[CGS FLOOR LOOP] Pending arrival ... adaptive=1`, then `Start AFTER ARRIVAL`.
3. Collect `[CGS FLOOR TP PULSE] ... adaptive=1 interval=40` and `[CGS FLOOR TP VERIFY] ... moved=1 adaptive=1 latencyMs=...`. Confirm every attempted pulse is applied, no position snapping/character lock, and both PIP views stay alive.
4. Record `[CGS FLOOR LOOP] Apex` measured peakY vs expectedApexY; compare with previous known-good 7m/s flight (about 2.0m observed versus 2.5m expected). A smoother impression must **not** sacrifice most of the vertical motion.
5. Confirm at least three consecutive `[CGS FLOOR LOOP] Descent rearmed NEAR SURFACE ... depth≈0.9` and `[CGS PORTAL MP ATTEMPT] ... entry=FALL_ZONE success=1`.
6. Deliberately walk sideways during ascent to exit. Ensure the character can still walk, survive the ground, and use normal view rotation.
7. If `[CGS FLOOR ADAPTIVE] Fallback` is logged, note how many pulses succeeded before it and whether the loop remains playable in legacy mode.
8. **A/B test:** disable `m_bAdaptiveFloorFlight` on both portal prefabs/instances in Workbench to compare the EXACT old 75ms feel without removing scripts. If adaptive looks worse, leave it disabled.
9. Test F held to grab / V to throw cube through both portals, wall portal player teleport, and PIP camera selection. These scripts should be unchanged.
10. Multiplayer intentionally deferred; the adaptive path is prevented by `Replication.IsRunning()`.

## Related idea: a portal that feels like a real hole in the floor

My proposed *pass-through aperture* should be its own later, separate PR. A road `RoadEntity` remains physically solid even under the blue/orange screen. We may simulate passing through it by a small, direction-sensitive trigger in front of the portal that transfers the player just before character-vs-ground contact. We can consider safe collision filters/temporary ghosting **only after confirming the Enfusion character controller exposes a suitable collision API**. Do not claim the static terrain mesh can simply be cut at runtime. Never globally disable RoadEntity collision or enable unrestricted player no-clip; other people/AI could fall through or clip into nearby geometry. The goal is a visually seamless plane crossing with camera continuity while the rest of the road stays solid.

## Important limitation

If Workbench confirms smoother **but still jerky** motion, don't continue reducing the pulse interval blindly. The long-term implementation requires an **engine-authorized continuous movement mode** for player-owned character controllers and portal-space velocity/orientation, plus interpolation of camera motion. That movement API is not yet verified in this project's engine build. Record failed and successful API calls in the handoff. Do not promise native physics before testing it.
