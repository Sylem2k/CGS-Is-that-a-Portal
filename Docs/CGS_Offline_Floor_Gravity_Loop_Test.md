# Offline gravity loop — first controlled player test

**STATUS: draft, not Workbench-compiled or play-tested. Do not enable on multiplayer until separately reviewed.**

## Purpose

When **both portals face upward** (flat floor or reasonably level upward-facing ground), entering one should launch the player upward from the other, then ordinary gravity should bring them back down toward the exit portal.

This improves the *player* floor-to-floor experience **without re-enabling the broken all-axis player momentum experiment**. The existing grabbable cube remains native dynamic physics, and its working Blue/Orange travel is untouched.

## Strict scope and settings

- Only when both portal normals have world Y > 0.75.
- Only in offline Workbench / singleplayer: `!Replication.IsRunning()`.
- Both Blue and Orange prefabs opt in with `m_bFloorGravityLoop 1`.
- Normal wall portals, other orientations, remote MP clients and servers: original momentum-free teleport.
- Minimum upward speed: **4.5 m/s** (slow walking entry gives a modest short hop).
- Maximum upward speed: **7.0 m/s** (real falls carry extra speed but are capped).
- New movement assist: **vertical correction only**, maximum 0.16m per server frame, with a 1.5s safety timeout; it stops earlier at the apex. It cannot change horizontal position/yaw or bank speed.
- Upward motion uses a short collision sweep before each incremental character transform, then lets native gravity take over at the apex.
- Floor exit is protected during initial ascent. When descending and near the opening again (depth <= 1.25m; at least 850ms after exit), it can re-enter. 1.5s fallback handles stale native vertical velocity.
- Character remains upright: no roll/camera inversion. True camera orientation flips for wall/floor or ceiling exits are a separate future feature.

## Oct 28 regression fix — wait for player teleport to complete

The user's attached offline Workbench log `Pasted text(20261028-055104).txt`
shows the loop attempt was triggering and then canceling almost
immediately:

- `[CGS FLOOR LOOP] Armed ... launchUp=4.5m/s`.
- `[CGS PORTAL HOST TELEPORT] ... accepted=1 ... after=<old source position>`
  shows the engine had accepted the teleport but the character transform
  had not been updated **yet**.
- `[CGS FLOOR LOOP] Start` immediately recorded that **source** position.
- On the next frame, the player appeared near the exit, several meters
  away from the recorded start, and the helper reported
  `[CGS FLOOR LOOP] Cancel: player moved away from exit.`.
- Around 350ms later `[CGS PORTAL MP CONFIRM]` verified the character
  near the intended destination.

**Fix:** `StartFloorLoop` now receives the authorized exit position and
holds the vertical correction **pending** for up to 650ms until the actual
player transform is near that position (X/Z within 0.85m; Y within
1.65m). It requires two near-exit frame samples, then records the real
arrival position as the sideways-escape anchor and starts the full lift
duration from that moment. No position corrections run at the old
portal. A missed or failed teleport cancels without moving anyone.

### Specific log checks

1. Walk into either floor portal while both faces point UP.
2. Expect `[CGS FLOOR LOOP] Pending arrival ... old=... expectedExit=...`.
3. Then `[CGS FLOOR LOOP] Start AFTER ARRIVAL ... origin=... expected=...`.
4. At this point expect lift movement followed by
   `[CGS FLOOR LOOP] Apex ...`, not a premature escape cancel.
5. `[CGS FLOOR LOOP] Cancel: teleport arrival timeout` indicates a
   teleport never visibly settled near the requested exit.
6. Test walking >3m SIDEWAYS **after arrival**; only then should
   `Cancel: player moved away from exit` appear.
7. Confirm player can walk, look, fall, and re-enter without freezing.
   Send the full `[CGS FLOOR LOOP]` and `[CGS PORTAL MP CONFIRM]`
   lines if any step differs.

This fixes the confirmed **launch-start timing**, not yet complete
three-axis portal momentum or camera orientation. The cube's working
native-physics transfer remains unchanged.

## Oct 28 06:04 follow-up: transform control after arrival

The latest Workbench log proves the PR #62 timing correction worked,
but **the actual upward launch did not**.

- `[CGS FLOOR LOOP] Start AFTER ARRIVAL` occurs repeatedly.
- `[CGS FLOOR LOOP] Apex: ... ticks=22` through `ticks=35` also occurs.
- Example: exit requested at `Y=6.48501`, confirmed player after 350ms
  at `Y=6.411` — **lower**, despite a requested `4.5m/s` launch.
- The previously used `BaseGameEntity.Teleport` incremental correction
  did not produce sustained upward travel on the locally predicted character.
  This is a movement-controller problem, NOT an entry/exit routing problem.
- Both portals are present; cube teleportation continues to work.

### New controlled experiment

This branch keeps arrival waiting from PR #62 and **changes only the
offline floor flight position driver**, not ordinary player teleport:

1. Store the actual arrival position as the launch origin.
2. Calculate the intended height on every frame from
   `height = originY + initialUpwardSpeed * time - 0.5 * gravity * time^2`.
3. Move **upward only**, at most **0.28m** per frame, with a
   0.30m-radius character-torso collision sweep. Keep X/Z and yaw free.
4. Apply the vertical position using `IEntity.SetWorldTransform` on the
   same locally predicted character, rather than the prior
   `BaseGameEntity.Teleport` micro-corrections.
5. Measure real current/after position and rate-limit
   `[CGS FLOOR FLIGHT] tick=... plannedY=... beforeY=... afterY=...` logs.
6. At the apex, report
   `[CGS FLOOR LOOP] Apex: ... startY=... peakObservedY=... expectedApexY=...`
   so we can compare **actual height gained**, not just number of ticks.
7. Abort if movement is refused on four consecutive attempted steps;
   do not repeatedly push into a blocked character/controller.

### Verify in the actual game

1. Fire both portals onto separate clear FLAT roads; stand clear of obstacles
   and walk into one portal without pressing movement keys after transfer.
2. Look for `[CGS FLOOR LOOP] Pending arrival`, then
   `Start AFTER ARRIVAL`, then at least 2 `[CGS FLOOR FLIGHT]`
   entries with `afterY > beforeY`.
3. At `Apex`, confirm `peakObservedY > startY` by roughly **0.5 to 1.1m**
   at 4.5m/s. Theoretical rise is ~1.03m. Player should be visibly
   airborne; gravity should subsequently bring them down.
4. If `afterY` rises but the next frame's `beforeY` snaps back,
   the native character controller is overwriting direct transforms.
   Capture those `FLOOR FLIGHT` lines: we will need to drive a dedicated
   character-controller movement mode rather than keep applying transforms.
5. Confirm the character can walk, exit the loop, survive contact, and
   re-enter the other portal. Don't test multiplayer yet.
6. If `[CGS FLOOR LOOP] Cancel: character transform refused movement`,
   the engine is rejecting direct transform changes altogether.
7. The existing `Not_Companion_Cube` transfer, weapons, MP modes,
   and previous portal momentum settings are untouched.

**This branch is a source-reviewed, uncompiled Workbench experiment.**
Do not present real ballistic flight as verified until player origin logs
and visible motion agree.

## Oct 28, 06:10 follow-up — native character transform is rejected

The uploaded `Pasted text(20261028-061015).txt` proves PR #63's
direct character transform approach does NOT work:

- Entry and exit both work, and `Start AFTER ARRIVAL` is logged.
- `[CGS FLOOR FLIGHT] tick=1 ... plannedY=7.32161 beforeY=7.20754
  afterY=7.20754 step=0.114072` confirms **zero actual movement** despite
  requested rise.
- Followed by `[CGS FLOOR LOOP] Cancel: character transform refused
  movement`.
- The same failure reproduces for the reverse portal direction.
- The cube still passes through portals, confirming only player
  character movement requires a different engine path.

### This branch's OFFLINE experiment

**Do NOT use `IEntity.SetWorldTransform` / `BaseGameEntity.Teleport`
for small player movement; neither produced sustained altitude.**

This branch uses `SCR_Global.TeleportLocalPlayer`, which previously
successfully moved the host-owned player in the main portal transfer.
It sends bounded, collision-swept, upward-only *local teleport pulses*
at **most once per 75 ms**. Each step is at most **0.28 meters**;
the actual engine-applied position is inspected at least 70ms after
the request, since teleport is asynchronous.

- This is a **player movement feasibility test**, NOT a fully
  physics-driven Portal momentum controller. It may be visibly stepped.
  Do not claim smooth ballistic physics until Workbench proves it.
- `[CGS FLOOR TP PULSE] n=... accepted=... actualBefore=... requested=...
  plannedY=...` is the outgoing host teleport.
- `[CGS FLOOR TP VERIFY] requestY=... startY=... actualY=... moved=...
  failures=...` shows whether the owned character really went UP.
- 3 consecutive ineffective pulses cancel the mode.
- When the loop reaches its apex, it still prints
  `[CGS FLOOR LOOP] Apex ... peakObservedY ... expectedApexY`.
- Retains PR #62 arrival wait, 3m lateral escape, 7m/s launch cap,
  solid obstacle sweep, and OFFLINE-only `!Replication.IsRunning()` guard.
- Native gravity resumes when upward motion ends; no change to
  camera roll, controller state or existing cube physics.

### Required Workbench retest

1. Sync this branch, compile Game scripts; two FLAT upward floor
   portals with at least 5m headroom.
2. Walk into Blue, stop moving. Confirm
   `[CGS FLOOR LOOP] Start AFTER ARRIVAL`.
3. Expect `[CGS FLOOR TP PULSE] accepted=1` and then
   `[CGS FLOOR TP VERIFY] moved=1` with increased actual Y.
4. Verify visually that the character rises. If `accepted=1`
   but `moved=0` repeatedly, the local-only helper is insufficient
   and we will use an engine-supported movement-mode integration.
5. At `Apex`, verify `peakObservedY - startY` is positive,
   ideally 0.5m or more for the 4.5m/s starter launch.
6. Check normal controls, falling back through the exit, sideways
   escape, both portal directions, F/V cube throw, wall portals.
7. **No multiplayer testing yet**; this mode never runs in MP.
   Report the PULSE / VERIFY logs if flight is not visible.

## Oct 28, 06:15 — successful launch, intermittently missed return

The supplied `Pasted text(20261028-061505).txt` establishes the first
**real observed upward player flight**:

- Host-local movement uses `SCR_Global.TeleportLocalPlayer` pulses.
- Repeated `[CGS FLOOR TP VERIFY] ... moved=1 failures=0`.
- `[CGS FLOOR LOOP] Apex` reports real ascent:
  `startY=7.18912 peakObservedY=8.16911 expectedApexY=8.22123`
  (+0.98m); after a 5.511m/s falling entry, another launch reaches
  `startY=7.18911 peakObservedY=8.68911 expectedApexY=8.73714`
  (+1.50m).
- Confirmed first automatic **FALL_ZONE** transfer (Blue -> Orange)
  after falling downward at 3.633m/s. A later return fall reaches
  5.511m/s and the new launch respects that speed.
- However **some** returns, especially above Orange, still reach the
  backing road without transferring. Those logs only show
  `Exit rearmed for descent ... depth=-0.04 elapsed=1500`.
  This is **too late**, because the player is already at ground level.

### Root cause and fix

`CGS_PortalTransferComponent.IsExitEntrySuppressed` relied on
`CharacterController.GetVelocity()[1] < -0.35`. The same log explicitly
shows `falling=1 controller=<0,0,0> observed=<0,-5.51112,0>`.
Thus the engine controller sometimes reports zero fall speed, even when
the portal's own multi-frame position observation proves freefall.

This PR:
1. Exposes `IsCharacterDescending` using the existing portal
   observed-position speed (80+ ms samples and short recent-fall cache).
2. While `CGS_PortalMomentumService.IsFloorLoopActive` is true,
   **never** prematurely clear exit protection due to small
   downward corrections between upward pulses.
3. Once the launch stops and the player is actually descending faster
   than 0.65 m/s, rearm as they pass within the fall catch prism,
   a little below its top but safely above solid ground.
   Each portal retains its existing fall-zone height (Blue 2m, Orange 3.5m).
4. Keep an explicit 1600ms timeout only as a FAILSAFE,
   and label it `fall missed` rather than claiming a real loop.
5. Multiplayer remains unchanged because the branch is guarded by
   `!Replication.IsRunning()`. No edits to cube, input, gun or flight.

### Focused retest

1. Compile this branch; place two FLOOR portals pointing UP as before.
2. Walk onto Blue. Expect `[CGS FLOOR TP VERIFY] moved=1`, then
   `[CGS FLOOR LOOP] Apex`, then **`Descent rearmed EARLY`**
   while depth is still positive (ideally 1–2.7m, never -0.04m).
3. Expect automatic `[CGS PORTAL] ... entry=FALL_ZONE` before landing,
   then another upward launch from the partner.
4. Let go of WASD and observe two or three complete consecutive loops.
   Moving sideways intentionally should interrupt the loop naturally.
5. If the debug instead says `Exit rearmed by timeout ... (fall missed)`,
   attach that excerpt, including the prior Apex and current near-depth.
6. Throw the cube with F+V and test wall portals to confirm unchanged
   behavior. Dedicated MP intentionally deferred.

## Workbench test — player (not the cube)

1. Sync test branch, compile Game scripts and load the same `unnamed.ent` test world.
2. Fire **Blue onto flat road** (normal `<0,1,0>`) and **Orange onto another flat road area**; give each 4–6m of empty space overhead. These MUST BOTH face UP for the test.
3. Walk into Blue. Expected: transfer to Orange **with a short upward movement** rather than emerging frozen, then gravity takes over and the character falls.
4. Watch for `[CGS FLOOR LOOP] Start`, `[CGS FLOOR LOOP] Apex`, `[CGS FLOOR LOOP] Exit rearmed for descent` and the normal `[CGS PORTAL MP MOVE]`.
5. Continue standing centered above Orange: should descend back toward Orange and transfer to Blue. Walk sideways during ascent to EXIT the loop; the system must stop its correction and return normal player movement.
6. Repeat from Orange to Blue, and from a higher drop. Do not expect unlimited speed: the cap is intentionally 7m/s.
7. Change either portal to a wall or steep slope (normal Y <= 0.75): there must be **no loop assist**, only established simple teleport.
8. Re-test F grab and V cube throw between portals. Cube physics, pairing, model, GUID and transfer code were not changed.
9. Check for stuck movement, sudden launch, clipping into ceilings, or missed return entry. If any occur, capture `[CGS FLOOR LOOP]`, `[CGS PORTAL VERTICAL LOOP]`, and the adjacent transfer messages.

## Known limitations

This is a bounded script-assisted lift, **not true CharacterController.SetVelocity** (the engine's character controller lacks the direct velocity setter used by dynamic props). It may not appear perfectly ballistic in every animation/state, and using small character teleports in a frame loop could still reveal controller problems. The short duration, collision checks, apex stop and offline-only guard are specifically intended to contain those risks.

A ceiling obstruction cancels lift; the player will not phase through it. Camera roll and portal-space orientation flipping remain unchanged for now.

### Diagnostic interpretation

- `[CGS FLOOR LOOP] Start` but no ascent: vertical correction is being rejected/overwritten by player movement.
- `[CGS FLOOR LOOP] Cancel: upward path blocked`: overhead obstacle; try an open area.
- `[CGS FLOOR LOOP] Apex`: lift released to native gravity.
- `[CGS FLOOR LOOP] Exit rearmed for descent`: exit portal can receive a deliberate return fall.
- `[CGS PORTAL TELEPORT ONLY]` on wall portals is correct and expected.
