# CGS Is That a Portal — Phase 1

Standalone Arma Reforger physical two-way portal project.

## Phase 1 status

The initial three component classes have been added as a **compile-first foundation**, not as working teleportation.

- `CGS_PortalComponent`: configurable portal identity, enabled state and future exit offset.
- `CGS_PortalManagerComponent`: reserved for pairing and server authority.
- `CGS_PortalTransferComponent`: reserved for safe character transfer.

## Next Workbench steps

1. Sync branch `feature/portal-phase1`.
2. Open the addon in Reforger Workbench and compile Game scripts.
3. Report any compiler errors before wiring replicated movement.
4. Build two portal prefabs and configure separate blue/orange portal IDs.
5. Add trigger/crossing detection, pairing, validation, cooldown, orientation and safe server-authoritative transfers.

No teleportation, physics, portal rendering or replication behavior is implemented yet.

## Phase 1.2 experimental directional crossing

Branch: `feature/portal-plane-crossing`

This mode is **not validated in Workbench or multiplayer**. Both prefabs enable `m_bUsePlaneCrossing 1`; disable that attribute to restore proximity-based activation.

A transfer is attempted only when a player's **entity origin** passes from the positive local Z side of a portal plane to the negative side, within portal half-width/half-height and the per-frame crossing step limit. It is a prototype; the player origin is not the character's head or center, and a solid floor may prevent crossing altogether. For testing a falling loop, place the floor portal in open air above the ground, and orient local Z upward; place the ceiling portal above it with local Z downward. Ensure there is enough room for the 1.5-meter exit offset and no intervening collision.

Current limitations: `SetOrigin` relocation still does **not** preserve character velocity or orientation. Therefore infinite falling cannot yet be promised. Test compilation, crossing detection and repeated transfers first. A 150-ms cooldown remains to guard jitter. No maximum distance between portals is imposed.


## Phase 1.3 — Experimental portal momentum (not merged)

Branch: `feature/portal-momentum-experiment`.

### What changed

- Uses the engine's `BaseGameEntity.Teleport()` path where supported, instead of only `SetOrigin()`.
- Reads incoming velocity from the character controller (or the physics body as fallback).
- Converts velocity from entrance-local axes to exit-local axes, so falling into a floor portal can theoretically emerge downward from a ceiling portal.
- Attempts `Physics.SetVelocity()` after teleport, **only when a physics body exists**.
- Preserves lateral entry offset across the portal; keeps the character upright.
- Makes exit offset 2.5 m; crossing-step tolerance 6 m. These are separate from unlimited portal travel distance.
- Logs registration and velocity diagnostics; individual successful-transfer logs are throttled to the first 3 and every 10th.
- The original `main` branch remains available as the working baseline.

### Workbench test

1. Switch to `feature/portal-momentum-experiment` and compile Game scripts. Share any compiler errors.
2. Place the floor portal with **local +Z UP**, and the ceiling portal with **local +Z DOWN**.
3. Leave adequate clear air between the portals. Prefer a tall test space without solid roof collision at the spawn point.
4. Enter the floor portal from above. Check whether the character exits below the ceiling portal, keeps falling and repeats.
5. Look for `[CGS PORTAL PHYSICS]` entries indicating incoming/outgoing velocities, `physicsApplied` and whether the physics body is dynamic.

### Editable portal settings

- `Preserve Momentum` enables/disables attempts to apply exit velocity.
- `Momentum Multiplier` defaults to 1.
- `Max Exit Speed` defaults to 45 m/s (set to 0 to remove the *speed* cap).
- `Exit Offset` defaults to 2.5 m.
- `Maximum Crossing Step` defaults to 6 m.
- `Front Catch Distance` remains 1 m.

### Known limits

**No real-time Workbench test has been run for this commit.** CharacterControllerComponent exposes velocity reads, but there is no corresponding public set-velocity method for the controller. If `GetPhysics()` is null or the controller overrides the physics body's velocity, this branch will *not* preserve player momentum; the debug message should show that. Orientation of the character/camera is intentionally unchanged. Fall/collision damage and clipping remain possible. This is a physics probe rather than a confirmed infinite-fall implementation.

Portal-to-portal *travel distance* has no added limit.


## Portal direction visualization (main branch)

Each stationary portal prefab now enables `m_bShowFacingArrow 1`.

- In a running Workbench test world, the **green arrow** points along the portal entity's world-space **local +Z** direction. This is the direction the current teleport code uses to place an exiting character.
- The **blue or yellow rectangle** marks the early-catch trigger plane 1 meter in front of that portal, not solid collision geometry. (Blue = portal ID 0, yellow = portal ID 1.)
- The server logs `[CGS PORTAL FACING]` once per portal at initialization, including the normal vector and the direction label `UP`, `DOWN` or `SIDEWAYS`.
- For the falling-loop test: **floor portal's green arrow must point UP**; **ceiling portal's green arrow must point DOWN**. Rotate the portal entity in World Editor, not its Blender mesh only. The green arrow follows the same entity matrix the teleport code reads.
- Arrows are debug `Shape` graphics and may require game/debug-shape rendering to be enabled in the test viewport; the initialization logs remain available even if shapes do not display.
- These markers do not repair camera alignment, collision damage, or physical character momentum. They are intended to make incorrect orientation obvious before physics tests.

If Workbench continues printing `[CGS PORTAL] SetOrigin executed`, an earlier script version is loaded. The current main branch instead reports `[CGS PORTAL PHYSICS]` and `[CGS PORTAL FACING]`. Confirm the addon/branch is up to date and reload Game scripts.


## Phase 1.4 — Select portal model-face axis (test branch)

Branch: `feature/portal-facing-axis`. Changes **not yet compiled in Workbench**.

### Why this exists

The current test portal meshes lie flat in local XZ. Enfusion world Y is vertical, so using local +Z as the portal face normal incorrectly launches players sideways. This change lets the component choose which local axis is the physical entrance and exit normal, applying the choice consistently to crossing detection, debug arrows, exit positioning, and velocity transformation.

### Face Axis property

| Value | Portal face normal |
| --- | --- |
| 0 | local +Z (legacy) |
| 1 | local +Y (flat mesh, entrance from above) |
| 2 | local -Y (flat mesh, entrance from below) |
| 3 | local -Z |
| 4 | local +X |
| 5 | local -X |

The prefabs are configured **Orange = 1 (+Y, floor)** and **Blue = 2 (-Y, ceiling)**. Rotate/position the whole entity as needed; these axes remain *local* to each prefab.

The console now reports `[CGS PORTAL FACING] ID=... faceAxis=... normal=... facing=UP/DOWN/SIDEWAYS`. Expected log when both meshes are placed flat, not rotated:
- Orange ID=1 normal approximately `<0,1,0>` facing UP.
- Blue ID=0 normal approximately `<0,-1,0>` facing DOWN.

The runtime green facing arrow and colored trigger-plane rectangle also use the new normal, rather than always local +Z. They are debug shapes and may not be visible in the static World Editor view.

### Test

1. Pull/check out the branch; compile game scripts.
2. Place Orange flat on floor, Blue flat on the ceiling directly above it (no need to rotate flat meshes if the underside should face down).
3. Confirm the portal facing diagnostics. If either still reads SIDEWAYS, inspect its entity rotation or Face Axis property.
4. Fall into Orange and verify the exit occurs *below* Blue. Watch `[CGS PORTAL PHYSICS]` for normal and velocity.

This addresses incorrect portal-facing direction, not guaranteed character-controller momentum or collision-damage immunity. Keep the previous main behavior available until testing.


## Phase 1.5 — Walk onto floor portals (test branch)

Branch: `feature/portal-walk-in-floor-entry`. **Workbench compilation/gameplay not yet verified.**

### Why walking was failing

The early-catch trigger is a plane 1 meter in front of the physical portal. The normal floor character's origin remains at foot height when walking, so it cannot cross this elevated plane. The player may visibly step onto Orange yet never teleport.

### How the fix works

- **Floor-facing portals only** (outward normal world Y > 0.75): detect a player's horizontal movement from outside to inside the portal's rectangle, while their origin is between -0.4 m and +1.4 m above its surface.
- **Falling or wall portals**: preserve directional front-to-back plane crossing as before.
- Does not trigger simply by standing motionless in the portal. Step out of the colored pad and then back inside to activate.
- The existing per-character teleport protection remains in `CGS_PortalTransferComponent`; the change does **not** alter portal travel distance, velocity or damage handling.
- Editable component values: `Allow walking onto upward-facing floor portals` (on) and `Maximum player-origin height...` (1.4 m).
- Successful transfer log includes `entry=WALK_ON` or `entry=PLANE`.

### Test steps

1. Check out branch and compile Game scripts.
2. Put Orange on the floor, Blue above it on the ceiling. Both should remain flat; their Face Axis values are 1 and 2 respectively.
3. In play mode, begin **outside** the floor portal's area and walk into it. The console should show `entry=WALK_ON`.
4. For a falling test, enter from above; it should show `entry=PLANE`.
5. If the player emerges below Blue but dies before reaching Orange again, that is a separate unresolved character-controller momentum/collision issue.

No changes were made to model orientation, prefab resource GUIDs or collision geometry.


## Phase 1.6 — Falling rescue / loop assist (EXPERIMENTAL)

Branch: `feature/portal-fall-catch-assist`. This is not yet validated by a Reforger Workbench compile or gameplay test.

### Evidence motivating this patch

During a test the Orange floor portal was at approximately `<125.148,1.353,79.561>` and the character died on terrain at approximately `<125.339,1.000,83.500>`. The impact was nearly 4 meters sideways from the floor portal, well outside the prior 3 m × 4 m detection rectangle. The video also showed the player drifting past the pad before impact.

### Editable settings

| Setting | Blue (ceiling) | Orange (floor) | Purpose |
| --- | ---: | ---: | --- |
| Face Axis | 2 (-Y) | 1 (+Y) | Blue points down, Orange points up |
| FUN MODE: rescue falling player | 0 (off) | 1 (on) | Only Orange catches a fall |
| FUN MODE: rescue height | 3.5 m | 3.5 m | Rescue plane in front of the portal |
| FUN MODE: rescue half-width/half-length | 5 / 5 m | 5 / 5 m | 10 × 10 m catch region |
| Exit lateral offset scale | 0 | 0 | Appear at exit center instead of preserving sideways entry displacement |

The standard visible portal footprint (3×4 m), walk-on detection, early plane crossing, and unlimited portal-to-portal range remain unchanged. The **pink rectangle** in play mode shows Orange's new extra falling catch region. Green arrows continue to show outward portal normals.

### Testing

1. Check out `feature/portal-fall-catch-assist`; reload Game scripts and compile.
2. Put Orange flat on ground facing UP and Blue approximately 10–15 m above it facing DOWN.
3. Walk onto Orange from outside; look for `entry=WALK_ON`.
4. Let go of movement controls and fall; look for `entry=FALL_RESCUE` on the next transfer as you pass the pink elevated rectangle.
5. Check whether two or more cycles occur without collision deaths.
6. If the character still dies, send `[CGS PORTAL]`, `[CGS PORTAL FALL]`, `[CGS PORTAL PHYSICS]`, and `KILL SUICIDE` log lines plus video.

This is **fall-assist teleportation**, not guaranteed true momentum conservation: Reforger's normal character physics body has been observed `dynamic=0`. The script now refrains from calling `Physics.SetVelocity` on a non-dynamic body. Normal collision damage remains enabled. Turn off the assist on Orange to compare with physically strict behavior. Set exit lateral scale to `1` on either portal to restore full entry position preservation. No world-distance limit was introduced.


## Phase 1.6b — Falling volume catch + safe player teleport test

Observed: Orange -> Blue `entry=WALK_ON`, followed by another Orange -> Blue `entry=PLANE`, then fatal abdomen COLLISION at ground level. The older one-frame `FALL_RESCUE` trigger was not reliably reached.

This experimental revision:
- Uses **FALL_ZONE** detection whenever a player is descending through the volume between 0.35 m and 3.5 m above the Orange portal, within a 10 m × 10 m lateral region. Unlike the earlier FALL_RESCUE, it does **not** require crossing the volume's exact top plane on a particular frame.
- Adds **Safe Player Teleport** (on by default for both test prefabs), using `SCR_Global.TeleportPlayer(playerId, destinationPosition)`. Reforger's stock method resets physics velocity, so this prioritizes survival/functional repeat loops over true momentum conservation.
- Keeps the existing 150 ms anti-loop cooldown, walking-entry detection, face axes, centered exit, and unchanged world-distance range.
- Avoids velocity writes on non-dynamic character bodies and reports `safePlayerTeleport=1` in the physics log.
- **Does not disable collision damage or guarantee survival.** Exit clearance still matters and character ownership in dedicated MP remains untested.

Expected transfer sequence after walking onto Orange:
`entry=WALK_ON`, then `entry=FALL_ZONE` repeatedly as the character falls from the Blue ceiling exit. Some `entry=PLANE` entries are acceptable if the narrow trigger catches first.

The fall-volume visualization remains the pink rectangle at its top; the actual rescue zone extends downward from it. To compare stock vs safe character teleport, toggle `Safe player teleport` in the destination portal's component. Restore it to ON after comparison.

**Workbench compilation has not been verified** for this revision. If compilation succeeds but a collision death still occurs, collect `[CGS PORTAL PHYSICS]`, `[CGS PORTAL]`, and `KILL SUICIDE` lines together with the position and timestamp to see whether the engine actually moved/cleared character velocity.
