# CGS Portal Gun — Development Handoff (2026-10-28)

> **Current state:** My Portal Gun is functional in offline Workbench. Blue/Orange portal placement, basic player teleportation, F-to-grab/V-to-throw, and physics-cube portal travel work. **Offline player floor-to-floor gravity loops now work repeatedly**, but the movement is still visibly stepped/glitchy. I am pursuing the real *Portal* movement experience; **multiplayer is explicitly postponed** until the offline controller is stable.

## Where my work lives

- **Primary GitHub repository:** https://github.com/Sylem2k/CGS-Is-that-a-Portal
- **Main branch:** contains all merged PRs through **#66** as of my 2026-10-28 06:29 Workbench retest.
- **Current test branch:** `fix/pip-camera-slot-stability-20261028` (stable two-camera PIP slots; follow-up to successful PR #66)
- **Current draft PR:** #67 (PIP camera slot stabilization; not Workbench-tested as of writing). PR #66 was merged and its new near-surface logic HAS passed a real Workbench test.
- This Portal Gun is a separate project from my three Arma Reforger MechWarrior repositories; do not confuse or overwrite them.
- I use Arma Reforger / Enfusion Workbench **1.8.0.13** in the supplied logs, testing the offline `unnamed.ent` scenario.
- When editing scripts, **preserve original script filenames and resource GUIDs**. I expect complete scripts ready for copy/paste if I need to drag-and-drop files. GitHub commits/PRs are the normal delivery path. Write development notes in **first person**.

## New Workbench verification — 2026-10-28 06:29 (PR #66 success)

I tested PR #66 in Arma Reforger Workbench 1.8.0.13 and supplied
`Pasted text(20261028-062946).txt`, a 961-line offline log.

**The near-surface return fix is now confirmed working, not just source-reviewed:**

- **11 successful `entry=FALL_ZONE success=1` automatic return transfers.**
- **11 `[CGS FLOOR LOOP] Descent rearmed NEAR SURFACE`**, depths
  **0.878–1.012m**, average **0.939m** above each portal face.
  The old premature 2.6–2.7m return is gone in this test.
- **11 `Apex` events**, with **~0.98–2.00m actual height gained**.
- **83/83** `[CGS FLOOR TP VERIFY]` entries show `moved=1`;
  no `moved=0`, `accepted=0`, or `[CGS FLOOR LOOP] Cancel`.
- **No** false `Player left exit area` events, and no
  `Descent rearmed EARLY` events.
- Measured downward speed was **4.49–9.69m/s**; launch velocity was
  capped at **7m/s**. More than 7m/s falling is not a launch-cap bug:
  the character gains downward speed again from real gravity.
- The log ends when I reload Workbench, not due to a logged portal crash.
  No Game script compile error is visible in the supplied log.

**What is not solved:** the player still moves upward in small host-local
`SCR_Global.TeleportLocalPlayer` jumps at 75ms intervals, up to
0.28m per request. The elapsed trajectory can undershoot its ideal
ballistic apex (~2.0m observed versus ~2.5m expected at 7m/s).
It is **not native continuous character velocity**. I want a genuine
smooth flight controller in the next major phase; I do NOT want to
replace a verified loop with speculative untested velocity setters.

### Newly isolated portal PIP flicker (next draft PR #67)

My 06:29 log shows the PIP camera slots FLIP when Blue and Orange
swap which is nearer to the viewer:

```text
[CGS GUN PIP] LIVE owner=1 color=0 ... slot=27
[CGS GUN PIP] LIVE owner=1 color=1 ... slot=26
[CGS GUN PIP] STOP owner=1 color=1 released slot=26
[CGS GUN PIP] STOP owner=1 color=0 released slot=27
[CGS GUN PIP] LIVE owner=1 color=0 ... slot=26
[CGS GUN PIP] LIVE owner=1 color=1 ... slot=27
```

In `Scripts/Game/CGS_Portal/CGS_PortalPIPLiveComponent.c`,
`RefreshLocalRenderSelection()` sorts by distance every 450ms,
assigning camera slots **26** and **27** by rank. Crossing between the
same two portals reverses rank. It needlessly invokes `StopRender()`
for BOTH live cameras, deletes their render targets, and waits
`WARMUP_MS=1500` before recreating them. That is a concrete,
code-supported source of PIP blackouts/changes.

**Draft PR #67** keeps each slot unchanged when the **same two**
portals remain selected, even if their nearest/farthest order
reverses. I am **not** changing which two portals qualify to render
or altering any teleport/flight/cube code. This is source-reviewed
but awaits Workbench compilation and visual confirmation.

**Acceptance:** with both portals visible, run at least 3 full
floor loops. Expect Blue/Orange live feeds to retain their slot
assignments and have **no repeated `STOP`/`LIVE` pairs solely from
rank swapping**. Both feeds should still update with the correct
partner view. Replacing a portal, going out of render range, or
destroying it may legitimately stop a PIP pass. If screens still
flicker while camera slots stay fixed, investigate HDR matching,
layout/material reset, and 12 FPS capture separately.

## VERIFIED: PR #68 adaptive movement, Workbench 2026-10-28 06:42

I pulled and tested merged PR #68 using Arma Reforger Workbench
1.8.0.13, in the offline `unnamed.ent` scenario. My uploaded
`Pasted text(20261028-064258).txt` contains **981** log lines.

**Actual results (do not confuse with smoothness):**
- **108** `[CGS FLOOR TP PULSE]` movement requests, all `accepted=1`.
- **108/108** `[CGS FLOOR TP VERIFY] moved=1` with
  `adaptive=1`; **zero** moved=0, accepted=0, or automatic fallback.
- Owner-local movement-ack latency **18–50ms**, average **28.5ms**,
  median 27ms. Recorded requested step height ranges
  **0.06–0.26m**, mean ~0.149m.
- **8** `[CGS FLOOR LOOP] Apex` events, with actual height gained
  **0.88–2.06m** above each start position (not merely a virtual timer).
- **7** successful `entry=FALL_ZONE success=1` transfers and
  **7** `Descent rearmed NEAR SURFACE` events. Recorded trigger depth
  **0.833–1.098m** (mean **0.969m**).
- Observed down-speed at those re-arms **5.69–9.69m/s**, with
  exit launch deliberately capped at **7m/s**. This is still
  a controlled floor-loop speed cap, not arbitrary momentum stacking.
- **Zero** `[CGS FLOOR LOOP] Cancel`, no `[CGS FLOOR ADAPTIVE]`
  fallback message, and no obvious Game script compilation failure.
- **PIP stable:** exactly TWO `[CGS GUN PIP] LIVE` (slot 27
  Blue, slot 26 Orange), and TWO `STOP` lines only as Workbench
  explicitly reloads at the END (around log lines 945–948).
  PR #67 appears to have eliminated the frequent distance-rank
  camera stop/restart pattern in this test.
- The final apex at log line 944 is followed by **my manual
  Workbench reload**. This accounts for 8 apexes but only 7
  complete return transfers; it is not a recorded loop failure.

**Trajectory gap:** at 7m/s, the current adaptive assist repeatedly
shows **~2.02–2.06m actual rise against ~2.50m ideal rise** (occasionally
~1.88m versus 2.50m). This is a real, measurable **0.44–0.62m
undershoot** that likely arises from asynchronous character teleports
and native controller gravity between pulses; that causal explanation
is a hypothesis, not yet proven. New movement code must compare
real apex height, collision handling, and camera motion before replacing
the successful implementation.

**What this test does NOT establish:** I uploaded a LOG, not a new
screen recording. `adaptive=1` and `moved=1` show that the intended
movement occurs, not that the image is smooth. At 40ms (and observed
variable acknowledgments), the host still moves via independent
`SCR_Global.TeleportLocalPlayer` requests. Full cube-quality,
continuous player velocity, pitch/yaw rotation and midair steering
are NOT implemented. Do not claim the real Portal experience is finished.

### Safe priorities after this verification

1. Preserve all merged working features through **PR #68**, especially
   near-surface FALL_ZONE, owner-local adaptive movement, and stable PIP.
2. For the next actual SMOOTH-FLIGHT milestone, identify a supported
   **character-controller-owned continuous movement/flight mode**.
   Previous `Physics.SetVelocity`, `BaseGameEntity.Teleport` micro-moves
   and `IEntity.SetWorldTransform` were rejected by Workbench.
   The grappling hook in my Warfare/MechWarrior repo likewise offers a
   scripted-position fallback and does not establish a verified native
   flight controller API. Do NOT repeat those failed approaches.
3. Only after verifying an engine movement API, build an opt-in,
   collision-tested flight mode alongside the known-good adaptive
   fallback, using the SAME destination-arrival and descent-rearm logic.
4. My **'portal as water / real floor aperture'** idea remains
   separate: inside the oval, intercept plane crossing just BEFORE
   the backing RoadEntity collider, or investigate safe per-entity
   collision filters IF verified. Never disable global terrain/road
   collision, let unrelated players noclip, or claim a real hole
   exists under a road prefab.
5. Player view/camera continuity across portals and true 3D momentum
   redirection are subsequent phases. Multiplayer remains POSTPONED
   until stable offline behavior and no predicted-character glitches.

## Historical PR #68 development plan — now merged and tested

- **PR #67 is merged.** Its stable Blue/Orange PIP camera slots are in `main`; I still need an independent Workbench log proving the visible screen flicker is gone.
- My next goal is smooth PLAYER flight like the dynamic cube, without breaking the confirmed 11-loop near-surface travel from PR #66.
- This branch: `feature/offline-adaptive-flight-test-20261028`, new draft PR #68. It adds `m_bAdaptiveFloorFlight` to the two gun portal prefabs, opt-in **only** for offline floor→floor mode. If this flag is unchecked, original 75ms / 0.28m upward pulses continue unchanged.
- The experimental mode attempts speed-scaled smaller steps no more often than every 40ms, waits for real confirmed player motion before issuing another, and automatically drops back to the verified 75ms driver if its movement requests fail. **Not true native velocity; Workbench compilation/play test still pending**. I must compare actual peakY, responsiveness, camera shake, and `FALL_ZONE` returns against the old mode before merging.
- Details/test procedure in `Docs/CGS_Adaptive_Player_Flight_Experiment_20261028.md`. This includes rollback and exact `[CGS FLOOR TP PULSE]`, `[CGS FLOOR TP VERIFY]`, `[CGS FLOOR ADAPTIVE]` diagnostics.
- I also asked whether a portal could become like a water surface or a brief no-clip zone, letting characters cross the floor without hitting the static road collider. **Answer in principle: yes by a predicted, direction-specific pre-collision portal crossing/teleport trigger** (already resembles the cube's swept-entry technique); **not** by simply erasing a hole in terrain at runtime. Temporarily ignoring collisions only inside the oval might be possible if Enfusion offers safe per-character/per-surface collision filtering, **not yet verified**. I must not globally disable world-road collision or let the player noclip through unrelated ground. Build that crossing mode as a separate PR after the movement experiment.
- No work here touches `Not_Companion_Cube.et`, prop-transfer, gun input, the working floor re-arm gate, or dedicated multiplayer.

## What is proven working — do NOT regress

1. I can shoot **Blue** and **Orange** portals onto roads/floors and suitable other surfaces; each has an owner and a paired portal. PIP viewing works, though the feed can start/stop repeatedly.
2. **F held = grab** a cube; **release F = drop**; **tap V while still holding F = throw**. G remains assigned to the radio; Blue/Orange fire controls remain unchanged.
3. `Not_Companion_Cube.et` is an existing replicated **GenericEntity**, resource `{E4CC81B24CC91AB3}Prefabs/Props/Core/Not_Companion_Cube.et`, with a dynamic physics body. The crucial prop-transfer fix was **PR #60**, using `GenericEntity.Cast(prop)` and `genericProp.SetOrigin(exitPosition)` rather than silently failing `BaseGameEntity.Cast(prop)`. Cube travels in both directions and keeps physics; test with F+V. Keep the prefab and its GUID intact.
4. Standard player portal teleportation is reliable. It uses Reforger's `SCR_Global.TeleportLocalPlayer()`/owner-appropriate game APIs, and the character controller is **NON-DYNAMIC**. The formerly broken all-axis momentum/virtual banking was deliberately disabled. Normal player portal travel still uses the safe **no-momentum** path except the separately gated experimental offline floor-loop assist.
5. Both upward-facing floor portals can repeatedly transfer a player **Blue -> Orange -> Blue** as the player rises, reaches an apex, falls and gets caught in `FALL_ZONE`. Real measured upward movement is proven: `[CGS FLOOR TP VERIFY] ... moved=1`, real apexes above start positions, and subsequent measured falling speeds. This is a successful gameplay prototype, not true uninterrupted native character-controller physics.
6. The offline launch mode applies only when **BOTH normals point mostly up (Y > 0.75)**, portal prefab `m_bFloorGravityLoop 1`, and `!Replication.IsRunning()`. Minimum launch **4.5m/s**, maximum launch **7m/s**, gravity **9.81m/s²**, ~**1.5s** timeout. Existing player controls remain usable; moving laterally should end a loop. Do not turn the old unrestricted momentum system back on.

## Why earlier attempts failed (avoid repeating them)

- PRs #56-#59: thrown cube did not teleport for some floor/slope cases; the entry checks were made more tolerant and traceable. But the definitive failure after successful entry was a **silent `BaseGameEntity.Cast` mismatch** on the `GenericEntity` cube, fixed by PR #60. Do not chase cube Rpl/GUIDs when the transfer fails at the move/cast step.
- PR #61: initial offline player gravity loop was canceled as 'player moved away from exit' because the flight timer/escape anchor started at the **source** before the asynchronous player teleport reached its destination.
- PR #62: wait for **two samples near the expected exit** and then arm the flight timer/escape anchor. This works; do not undo it.
- PR #63: directly moving the player character with `BaseGameEntity.Teleport()` micro-steps or `IEntity.SetWorldTransform()` produced no real lift: `plannedY>beforeY` but `afterY==beforeY`, followed by `character transform refused movement`. The character controller owns movement; cube-like `Physics.SetVelocity()` cannot be used on this non-dynamic actor.
- PR #64: short owner-local **`SCR_Global.TeleportLocalPlayer()` pulses** finally moved the player. The server requests are accepted and the subsequent real Y positions increase. Approx. 75ms pulse interval, max 0.28m upward step, swept torso collision check, owner check, and asynchronous request verification. **This is a stepping proof of movement, not smooth native velocity.** Never claim it is fully physical.
- PR #65: `CharacterController.GetVelocity()[1]` was often **zero during actual freefall**. Switched to existing portal's 80+ms **observed displacement-based downward speed** to re-arm floor exits sooner, gated until after the active upward launch ends. This enabled many repeated automatic returns.

## Latest user evidence (2026-10-28)

Supplied locally in the preceding conversation:
- Workbench log: `Pasted text(20261028-062048).txt` (~2629 lines).
- User video: `20261028-0619-59.1364794.mp4` (~12.3 seconds, 784x470 at 30fps).
- User report: **'glitchy but getting there'**.
- Full diagnostic tally from the log: **43 `[CGS FLOOR LOOP] Apex` events**, **15 `Descent rearmed EARLY`**, **27 `Player left exit area`** events, **278 `FLOOR TP PULSE`**, **241 `FLOOR TP VERIFY`**, and **no `[CGS FLOOR LOOP] Cancel:`** in this session. Counts are log events, not an independent count of successful gameplay cycles.
- 15 early re-arms happened at portal-local depths **2.556–2.699m** (average ~2.643m). Example: `Descent rearmed EARLY ... depth=2.6537 downSpeed=4.28222 elapsed=887` immediately followed by a valid `entry=FALL_ZONE` teleport. This **cuts the return fall short ~2.6m above the actual opening**, causing abrupt mid-air jumps.
- Other arcs report `Player left exit area` immediately after `Apex` because the player rose **above** the catch prism. The old exit protection then cleared merely due to vertical height, allowing a later `FALL_ZONE` catch at its overly high first threshold.
- Later real fall speeds in the log include ~**5.65m/s**, **6.46m/s** and **6.54m/s**; upward launch follows carried measured speed but stays at or below the configured 7m/s cap.
- PIP: roughly 6 LIVE and 6 STOP log events in the session. Video frames alternate lighting/brightness during Workbench movement and the user reports jitter. This could involve character teleport pulses, PIP camera/HDR exposure, or Workbench transitions. **Do not claim the exact visual flicker mechanism is proven solely by this log/video.**
- No Game script compile errors or `CGS FLOOR LOOP Cancel` errors appeared in that supplied log. Its absence does not mean gameplay is smooth.

## Previous near-surface fix — PR #66 MERGED AND TESTED (historical design notes)

I deliberately scoped this change narrowly to the confirmed abrupt midair return, leaving the successful flight system unchanged.

1. **`Scripts/Game/CGS_Portal/CGS_PortalComponent.c`**: add `IsWithinExitCatchFootprint(worldPosition)`, checking lateral extents in the actual portal plane while **ignoring vertical depth**. Rising above the rescue prism is NOT equivalent to leaving the portal laterally.
2. **`Scripts/Game/CGS_Portal/CGS_PortalTransferComponent.c`**:
   - When offline floor-loop exit protection is active, cancel protection early for genuine **sideways escape**, not when the player merely rises above the top of the FALL_ZONE.
   - After the lift has completed, require measured falling speed (>0.65m/s) and wait until portal-local depth is around **1.10m** (between 0.35m and 1.10m) before clearing protection and allowing `FALL_ZONE`.
   - New success log: `[CGS FLOOR LOOP] Descent rearmed NEAR SURFACE`.
   - Preserve >=620ms initial waiting, world/character collision checks, 1600ms fallback, standard non-floor/MP behavior.
3. **Do NOT change** `CGS_PortalMomentumService.c` in this PR. The existing 75ms/0.28m owner-local pulse driver is the only player ascent path validated in Workbench so far.
4. This PR is **source-reviewed only**, needs Workbench compile and runtime testing. If the player lands on the backing road with this later catch, raise the threshold moderately (e.g. 1.25–1.40m), instead of restoring premature catches at 2.7m. Adjust with real timestamps/depths, not guesses.

## Source code map

| File | My usage / don't accidentally change |
|---|---|
| `Scripts/Game/CGS_Portal/CGS_PortalComponent.c` | Portal placement/basis, observed entry speed, `FALL_ZONE`, `WALK_ON`, wall/plane catch, rescue footprint, per-portal settings, fall-speed observer |
| `Scripts/Game/CGS_Portal/CGS_PortalTransferComponent.c` | Server transfer and paired destination, source/exit protection, offline floor-loop gating and late/early re-arm, player move request |
| `Scripts/Game/CGS_Portal/CGS_PortalMomentumService.c` | Deferred arrival, offline owner-local upward teleport pulses, ballistic target height, apex reporting; legacy unrestricted momentum must remain off |
| `Scripts/Game/CGS_Portal/CGS_PortalPropTransferService.c` | WORKING dynamic cube entry/crossing, safe exit and actual `GenericEntity` teleport |
| `Scripts/Game/CGS_Portal/CGS_GrabbableComponent.c` | Cube grab state, movement frame tracking, temporary approach memory/transfer cooldown |
| `Scripts/Game/CGS_Portal/CGS_GrabGunComponent.c` | F grab/V throw, follows recently thrown prop for 4s for server entry detection |
| `Scripts/Game/CGS_Portal/CGS_PortalManagerComponent.c` | Portal registry and Blue/Orange pairing per player |
| `Scripts/Game/CGS_Portal/CGS_PortalPlayerController.c` | Player input/owner RPC plumbing; avoid changes without dedicated testing |
| `Prefabs/Weapons/Core/Blue_Portal.et`, `Orange_Portal.et` | Portal GUIDs and default editor parameters: m_bFallCatchAssist, distinct rescue heights, floor-loop speed settings |
| `Prefabs/Props/Core/Not_Companion_Cube.et` | Verified working GenericEntity cube: leave model/mesh/GUID, Rpl, physics unchanged |
| `Docs/CGS_Offline_Floor_Gravity_Loop_Test.md` | Chronological Workbench investigations and test sequence |

## Important PR chronology

All **merged to main** at latest retest: **#55** grab/throw; **#56** prop portal prototype; **#57–#59** floor/slope prop crossing diagnostics; **#60** working GenericEntity cube transfer; **#61** offline floor bounce prototype; **#62** wait for owner/player arrival; **#63** direct transform experiment (failed in runtime); **#64** working owner-local teleport pulse flight; **#65** observed-fall-based exit re-arm; **#66** tested near-surface capture and lateral-only escape. **#67** is the pending isolated PIP stability proposal. Keep new work as **separate draft PR** until Workbench confirms it.

## Next Workbench checks

1. Pull the branch/PR and **compile scripts first**. With two upward-facing road portals, step into Blue, release WASD, and observe multiple complete Blue ↔ Orange loops.
2. The critical change is **late-but-safe** exit capture: look for `Descent rearmed NEAR SURFACE ... depth≈0.35–1.10` and then `[CGS PORTAL] ... entry=FALL_ZONE`, while the player actually comes close to the oval. Do **not** expect `Descent rearmed EARLY ... depth≈2.6`.
3. Confirm rising higher than the FALL_ZONE no longer prints `Player left exit area` unless the player **actually moves sideways** away from the portal. Strafing away should still allow manual loop exit.
4. Confirm no road collision/fall damage, no sudden teleports under solid terrain, and no mid-ascent ping-pong. Test 3+ consecutive loops in both directions.
5. Compare video motion: the current 75ms pulses cause visible stepping even when numerically correct. **Next substantial feature should be a genuinely smooth character-controller movement mode**, using ownership-safe APIs, not increasing teleport pulse frequency recklessly.
6. Investigate portal PIP/HDR toggles and camera darkening **separately**, only with reproduced evidence, so visual changes do not mask movement regressions.
7. Once floor→floor is smooth: work on three-axis momentum redirection (floor→wall, wall→floor, wall→wall) and camera/yaw reorientation, proper flight/midair steering, safe ceiling collision, and a deliberate escape action. These are **not implemented/verified**.
8. Multiplayer testing and replication work are postponed by choice. **Do not claim multiplayer floor-loop support** until a dedicated 2-client test succeeds.

## Guidelines for the next assistant / next chat

I want the **real Portal experience**, not a short fixed hop or endlessly repeated teleport hacks. Preserve my working build. Read this handoff FIRST, then the current `Docs/CGS_Offline_Floor_Gravity_Loop_Test.md`, then inspect latest GitHub main/PR and my next Workbench log. A valid code change should be tied to the **actual debug evidence** and should not regress the cube, basic portal teleport, input mapping, or safe no-momentum wall portals. Do not say a draft is 'working' before I compile and show its log. Report GitHub PR links for changes, original filenames and complete scripts if I need manual copy/paste.
