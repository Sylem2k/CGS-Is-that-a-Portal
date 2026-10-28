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
