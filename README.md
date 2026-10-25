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


## Phase 1.7 — Virtual momentum carry (experimental test branch)

Branch: `feature/portal-virtual-momentum` — **not yet compiled or play-tested in Reforger Workbench**.

### Reason

The working floor-to-ceiling portal loop can transfer the character repeatedly without the previous collision deaths. However, Reforger characters have a **non-dynamic Physics body** (`physics.IsDynamic() == false` in our tests), and the normal `CharacterControllerComponent` exposes `GetVelocity()` but no public `SetVelocity()`. Calling `Physics.SetVelocity()` therefore does not reliably impart launch momentum to normal players.

### Experimental mechanism

The new `CGS_PortalMomentumService.c` stores an independent (virtual) velocity for each recently teleported player. On portal entry, the transfer script reads that carried velocity if present (else the controller's), transforms it from source orientation to destination orientation, and limits exit speed using the existing Maximum Exit Speed setting.

For the next few seconds, the service adds virtual gravity and compares its target velocity to the controller's actual velocity. It attempts to correct **only the difference** as small, server-authoritative `BaseGameEntity.Teleport()` offsets. Each offset has a sphere sweep and a terrain-height check. Ordinary character movement, gravity, and collision/damage remain enabled. The service stops when the duration elapses, the character dies, the server stalls for longer than 150 ms, or a correction hits an obstacle.

This is a **kinematic approximation**, *not real engine character-controller momentum*. It can feel choppy, especially in multiplayer, and may not work if engine networking corrects the forced positions. The branch is isolated so the original stable loop remains on `main`.

### Editable settings (both test prefabs)

| Field | Value |
| --- | --- |
| EXPERIMENT: carry simulated momentum | Enabled |
| EXPERIMENT: carry momentum for... | 4.0 seconds |
| EXPERIMENT: virtual gravity | 9.81 m/s² |
| EXPERIMENT: largest momentum correction each frame | 0.7 m |
| EXPRIMENT: max exit speed | 30 m/s (change to 0 only after stability tests) |
| Safe player teleport | Enabled |
| Floor FALL_ZONE rescue | Enabled on Orange |
| Blue/Orange face axes | Blue local -Y (ceiling), Orange local +Y (floor) |

### Tests

1. Check out this branch in GitHub Desktop. Pull, reload Workbench Game scripts, compile. The added file is `Scripts/Game/CGS_Portal/CGS_PortalMomentumService.c`.
2. **Baseline:** Orange floor below Blue ceiling. Enter Orange and confirm at least three consecutive `FALL_ZONE` transfers with no collision death.
3. **Acceleration:** Watch `[CGS PORTAL PHYSICS]` logs. `incoming` should represent carried velocity and can become more negative across loops up to the configured exit cap. `[CGS MOMENTUM]` lines report virtual target, native controller velocity, and correction.
4. **Direction test (after basic safety):** Orient the destination portal to face horizontally into clear open air and jump/fall into the entrance from above. Check whether the transferred downward speed produces horizontal launch movement. Avoid structures/terrain during the initial test.
5. If motion jitters, collides, snaps back, or becomes unstable, disable `EXPERIMENT: carry simulated momentum` on both prefabs, or return to `main`. Do not attempt production dedicated MP deployment from this experimental branch without an ownership/replication test.

No portal-to-portal distance limit has been added. Original prefabs and GUIDs are preserved.


## Phase 1.8 — Returning through a wall portal; future portal gun placement

Branch: `feature/portal-return-wall-entry`. **Not yet Workbench compiled/tested.**

Observed log: Orange (ID 1) → Blue (ID 0) once, but no subsequent Blue → Orange transfer when walking back. In that test both portal normals were horizontal (Blue approximately world +Z; Orange approximately world -X), so the existing floor walk-on and fall-rescue modes do not apply. Plane crossing alone can miss players when walls stop the character origin before it crosses the plane.

This branch adds a configurable **wall portal approach** detector, evaluated independently for each portal. It requires moving toward the opening within the portal's rectangle, using a chest-height probe and the portal's **world-space oriented basis**. It supports approaching from the front, and optionally from behind. It is not hardcoded to any coordinates, portal IDs, or specific world axis. Successful transfers log `entry=WALL_APPROACH`. When debug is on, `[CGS PORTAL WALL CHECK]` prints approach depth and offsets at most once every 1.5 seconds per character.

### Default editable settings in both portal prefabs

- Allow walking back into wall portals: **on**
- Wall approach distance: **1.75 m**
- Character-height probe offset: **0.85 m**
- Extra edge padding: **0.35 m**
- Approach from either side of the opening: **on** for experimentation; this can be restricted to the front later
- `m_bSimulateMomentum`: unchanged from the existing virtual momentum branch
- Other floor, ceiling, fall rescue and unlimited-range behavior: unchanged

### Future gun placement is an explicit requirement

The stationary test prefabs are **not** the final user experience. The finished CGS Portal Gun should fire two colored portals (Blue/Orange) onto eligible collision surfaces, using a raycast hit point plus **surface normal**, then compute the portal's orientation and position *at runtime*. No Workbench rotation or user-entered axis should be required.

To support that:
- Reuse these portal components on runtime-spawned entities rather than a separately hardcoded floor or wall script.
- Determine the entity's rotation so `GetPortalBasis(..., normal)` points **out of the impacted surface**. Currently Blue's mesh uses local -Y and Orange's uses local +Y as its front, so the placement code must respect these differing local face axes (or standardize both before shipping).
- On placing a new Blue or Orange, remove/replace that color's old portal registration safely and update the linked partner for all players.
- Validate surface size/placement and collision-safe spawn offsets; do not place portals inside walls or unsupported geometry.
- Make server spawning and replication authoritative so every connected player sees the same pair.
- Keep portal-to-portal range **unlimited**; placement distance is a different gameplay control.
- Keep the currently optional FUN falling-assist system distinct from accurate momentum physics.

### Workbench test now

1. Switch to `feature/portal-return-wall-entry`, pull, and compile.
2. Place both portals on walls oriented in any horizontal direction, at accessible walking height.
3. Walk into Orange: `entry=WALL_APPROACH` or `entry=PLANE`.
4. After exiting Blue, turn around and deliberately walk *into Blue*; confirm a log showing `portal 0 to 1 entry=WALL_APPROACH` and that you emerge at Orange.
5. Repeat several times; look for excessive ping-pong, jitter, clipping or fall damage.
6. If a trigger does not fire, provide `[CGS PORTAL WALL CHECK]` lines, facing normals and video.

Wall-entry assistance is a test affordance: it may activate a short distance before actual surface crossing and is not yet tuned for every possible mounted geometry.


## Phase 2.0 — Portal gun pistol prefab foundation

**Branch:** `feature/portal-gun-pistol-base` (Workbench compile and equip test pending)

The user pushed `Prefabs/Weapons/Core/Portal_Gun_.et`, which is a full `GenericEntity` prefab containing weapon, inventory, replication, sound, and pickup components. **Keep the original filename `Portal_Gun_.et` and its existing `.et.meta` GUID `{1B677D5095315BED}`; do not rename or reimport it.**

This phase converts that prefab into a clean pistol foundation:

- Assign existing model: `{A4EB7B72BC9B1048}Common/Models/Portal_Gun.xob`
- Set weapon category `Handgun`, slot `secondary`, display name **CGS Portal Gun**, and item physical properties
- Use stock M9 pistol animation assets `player_main_pistol.asi` and `p_m9_ik.anm` as *temporary* hand/weapon pose references; adjust custom grip pose after seeing the gun in game
- Keep `WeaponComponent`, `SCR_WeaponAttachmentsStorageComponent`, equip/holster/pickup actions, `WeaponAnimationComponent`, `RplComponent`, `NwkPhysicsMovementComponent`, `SignalsManagerComponent`, `Persistence`, and `Hierarchy`
- Add 1.5 kg `RigidBody` with model geometry enabled (check that the imported XOB supplies usable weapon collision geometry)
- Remove leftover bipod, magazine detach, fire-mode, safety user actions; their unused contexts; default recoil-camera shake; magazine HUD config; and firearm gamepad trigger effects
- Keep `WeaponSoundComponent` for later custom portal shot and handling audio
- Remove the missing `eye` pivot from `SightsPosition` until suitable model pivots have been authored

**This is the equippable weapon base, not a firing portal weapon yet.** The copied prefab has no `MuzzleComponent`/fire modes, so standard primary/secondary weapon firing does not currently place portals. Do not add a live-ammunition muzzle or pretend it can fire already.

### Workbench validation required

1. Pull `feature/portal-gun-pistol-base` and compile.
2. Add `Portal_Gun_.et` to an inventory or spawn it in the test world; verify it appears and that pickup, equip, stow and drop work.
3. Verify 1P and 3P hand poses and animation alignment. The imported model may require custom grip and aim pivots.
4. Verify weapon collision geometry; if there are rigidbody errors, supply a physics LOD for the XOB or revise the RigidBody settings.
5. Confirm the existing fixed Blue/Orange test portals still function. This branch does not modify their scripts or prefabs.

### Next code phase — per-player portal gun shooting

- Add server-side map of player owner ID -> Blue and Orange portal instances; replace same-color portal without touching another player's pair. Keep stationary test pair isolated.
- Add client-side key actions/weapon checks for *primary attack = Blue* and *secondary attack = Orange* without hijacking unrelated guns' ADS.
- Send color/shot intent to server with validated owner identity and a rate limit. Server raycasts valid surfaces and applies collision-safe alignment using actual hit normal and the spawned portal model's local front axis.
- Spawn the colored portal prefab with replication, update partner registry, and delete the owner's previous same-color portal only **after** validating a successful new placement.
- Keep the unlimited distance **between** paired portals; raycast maximum shot range is separately configurable.
- Preserve existing fall/return/momentum mechanics and hardening for multiplayer.



## Phase 2.1 — Personal Blue/Orange gun firing prototype

**Branch:** `feature/portal-gun-personal-firing` — **not compiled or multiplayer play-tested yet**.

The confirmed equippable/holsterable `Portal_Gun_.et` remains unchanged in model, scale, IK and original `.et.meta` GUID. This update only adds `CGS_PortalGunComponent` to that prefab.

### Inputs for the prototype

- **Left mouse button** (native `CharacterFire`) shoots/replaces Blue.
- **Right mouse button** (native `CharacterWeaponADS`) shoots/replaces Orange.
- Both use **rising-edge detection**: holding a mouse button does not spam shots.
- In this prototype, **right-click still shares vanilla ADS/aim**, since no custom input context has been created yet. Replace the shared ADS binding with a dedicated portal action when the gun firing loop is proven in Workbench.
- Shots only work while `Portal_Gun_.et` is the character's **actively equipped** weapon; they do not affect other weapons.
- Invalid/no-hit/dynamic surface shots leave existing portals intact.
- The gun costs no ammo and has a default 200 ms server-side cooldown to avoid duplicates.

### Ownership and replication

The old global Blue/Orange pair in `CGS_PortalManagerComponent.c` was replaced with a **player ID → Blue/Orange pair** map. Legacy stationary portals with owner ID 0 still form their own independent test pair.

- The client's local `SCR_PlayerController` reads inputs and sends a reliable **server RPC on the owning controller**.
- The authoritative server verifies the player is alive and holding the *exact gun* for the RPC, rate-limits it, validates a camera-origin radius around the character, and performs a server-side surface raycast.
- It creates the selected portal on the world/static hit surface, attaches that portal to the shooter's pair, and deletes only the shooter's old portal of the same color.
- A player can own at most one Blue and one Orange portal at a time. Different players' pairs remain isolated.
- Anyone can walk through any active pair. The manager links portals only within the same owner's pair.
- Gun-spawned portals are replicated using the existing prefab `RplComponent`. Owner linkage is server-side (teleport handling is already server-authoritative).
- When an owning player ID disappears from the connected-player list, cleanup deletes their portals. Normal respawning does not remove them.

### Placement

Shot placement uses the camera's position/forward vector validated near the player and a raycast into `TraceFlags.WORLD | TraceFlags.ENTS` (up to **90 m** by default). Moving physics surfaces are rejected. Geometry's collision is **not** removed: the existing early wall approach trigger is required for wall-mounted portals.

The hit normal determines the portal transform in world space. Blue's local face is **-Y**; Orange's is **+Y**. The script constructs separate orthogonal transforms so either prefab faces outward from any floor, wall or ceiling *without Workbench rotation*.

Portal-to-portal travel distance remains unlimited; the 90 m raycast cap is the **gun's placement range only**.

**Current limitations:**
- Surface-size/planarity checks, special blocked-surface rules, visual invalid-placement feedback, and robust camera/muzzle obstruction rejection still need implementing.
- Right mouse also invokes Reforger ADS in this first prototype.
- Shooting/portal replication on dedicated multiplayer still needs user testing.
- Physics momentum is still the separate experimental kinematic assist, not fully native character velocity.
- The first test requires manually obtaining the gun (placed in world or inventory); there is not yet an arsenal catalog entry.

### Workbench test sequence

1. Pull branch `feature/portal-gun-personal-firing`, reload Game scripts, and compile. Send any compile errors verbatim.
2. Equip `Prefabs/Weapons/Core/Portal_Gun_.et` and aim at a broad vertical wall.
3. Click **LMB** once; watch for `[CGS GUN] Player=... fired color=0`, and a Blue portal should appear.
4. Click **RMB** once on another wall; watch for `[CGS GUN] Player=... fired color=1`, and an Orange portal should appear.
5. Walk Blue → Orange and Orange → Blue; confirm the working `WALL_APPROACH` logs.
6. Click LMB somewhere else: the first Blue should be deleted, new Blue should remain linked with the original Orange. Repeat with RMB for Orange.
7. Try firing at sky, vehicles and invalid objects to ensure the previous portal remains.
8. Then test two connected players; confirm both can own their own pairs and travel through either pair.

**New files:** `Scripts/Game/CGS_Portal/CGS_PortalGunComponent.c` and `Scripts/Game/CGS_Portal/CGS_PortalPlayerController.c`. Existing files updated: `CGS_PortalManagerComponent.c`, `CGS_PortalComponent.c`, and `Portal_Gun_.et`.


## Phase 2.1b — ADS null-pointer crash and dropped gun component repair

**Branch:** `feature/portal-gun-ads-safety-fix`. Workbench/runtime confirmation pending.

### Reported failure

The latest Workbench log loaded `Portal_Gun_.et` successfully, then repeatedly logged:
- `ANIMATION (E): anim controller initialization data is invalid (graph , node MasterControl, instance )` when the prefab was instantiated.
- `Virtual Machine Exception` at `CharacterCameraADS.c:233`, `CharacterCameraADS.SolveNewMethod`, null return.

**Root cause of ADS crash:** In Reforger's stock `CharacterCameraADS.SolveNewMethod` implementation, the current weapon's muzzle is accessed without checking it for null:

`muzzleType = currentWeapon.GetCurrentMuzzle().GetMuzzleType();`

Our initial simplified pistol prefab contained a `SightsComponent` and `WeaponComponent` but **no `MuzzleComponent`**. Entering ADS therefore dereferenced a nonexistent muzzle every fixed frame, flooding the script log.

**Additional discovery:** The recent user push on `main` still had the resized pistol mesh and M9 item animation data but no longer included `CGS_PortalGunComponent`, so the equipped weapon would not pass the firing bridge's gun-presence check even after the camera was fixed.

### Patch

Only `Prefabs/Weapons/Core/Portal_Gun_.et` is changed:
- Add an **inert `MuzzleComponent`** with one `BaseFireMode` named Portal. It has no magazine, chambered cartridge, projectile source, ballistics, casing, or damage.
- Set `WeaponComponent.useAimingType None`, following existing non-ballistic handheld tools. This should prevent vanilla ADS camera activation while right-click still provides `CharacterWeaponADS` input to our portal-color script. It is not a dedicated custom input binding yet.
- Disable empty/unconfigured `WeaponAnimationComponent` (previously trying to initialize without `AnimGraph` or `AnimInstance`). The existing character-side `ItemAnimationAttributes` still select stock M9 pistol animations and IK.
- Re-add `CGS_PortalGunComponent` with Blue/Orange prefabs, 90m gun range, 200ms duplicate-fire guard. Preserve all existing custom gun mesh, grip/IK, item size, weapon primary GUID, and `.et.meta` file unchanged.

### Test

1. Pull `feature/portal-gun-ads-safety-fix`, reload scripts/resources, and equip the Portal Gun.
2. Check that neither `CharacterCameraADS.SolveNewMethod` null-pointer spam nor invalid `MasterControl` animation message occurs.
3. Verify pistol placement in the hand, holstering, dropping, and re-equipping remain unchanged.
4. Aim at a broad wall and left-click once (Blue). Right-click once (Orange). Expected: `[CGS GUN INPUT]`, `[CGS GUN RPC]`, and `[CGS GUN]` shot logs.
5. Try both directions of travel, then replace a color by firing it again. Verify only the owner's matching color moves.
6. If Orange input stops working after ADS was disabled, the next step is **a dedicated custom secondary input action**, rather than re-enabling the broken ADS camera.

This fix has not been tested in Workbench from ChatGPT. No changes were made to existing portal travel/return detection, ownership maps, or momentum code.


## Phase 2.1c — Orange right-click has a dedicated input (Blue unchanged)

**Branch:** `feature/portal-gun-dedicated-input` — **Workbench compilation and runtime verification pending**.

Player report: **Blue already fires successfully on LMB**, but **RMB still starts vanilla ADS instead of shooting Orange**. The previous script polled `CharacterWeaponADS` to request Orange, causing the conflict.

This patch preserves the working Blue path: `GetActionValue("CharacterFire")` and its owner-bound server RPC remain unchanged. Only Orange's source changes:

- Adds `Configs/System/chimeraInputCommon.conf` and matching `.meta`.
- Declares `CGSPortalGunOrange` bound to **`mouse:button1` (RMB)** and `gamepad0:left_trigger`.
- Declares `CGSPortalGunContext`, priority **10100**, and enables it each frame **only while the CGS Portal Gun is held and no menu is open**. This is intended to take the right mouse button from the normal ADS action in that state.
- `CGS_PortalPlayerController.c` now polls `CGSPortalGunOrange` rather than `CharacterWeaponADS`. It also prints exactly one diagnostic on equipping: `[CGS GUN INPUT] Orange action registered=... contextActivated=...`.
- Input latches ensure one mouse click places only one portal. Other weapons do not activate the custom context.
- Existing portal gun mesh, pistol holding/holstering, muzzle camera fix, portal physics, personal portal pairs and prefab GUIDs are untouched.

**Test:** Pull the branch, compile/reload, equip portal gun. Read the single registration line. Left-click a wall: Blue must still spawn. Right-click a different wall: Orange should spawn instead of ADS and emit:
`[CGS GUN INPUT] Player=1 requested color=1`, `[CGS GUN RPC] Server received player=1 color=1`, `[CGS GUN] Player=1 fired color=1 ...`.

If Orange still ADS, supply the registration/context line. If it reports `registered=0`, the input resource is not being merged and Workbench must import/reload the `.conf`; if it reports `registered=1` but never requests `color=1`, the context/action is not receiving RMB. If it requests `color=1` but no portal appears, check `[CGS GUN]` placement rejections.

**Limitation:** This is the first attempt at a high-priority custom input context, not confirmed yet to suppress every version of vanilla ADS. No global ADS or input behavior is changed for other weapons.


## Phase 2.1d — Orange middle mouse and missing input registration

**Branch:** `feature/portal-orange-middle-input-load` — not Workbench-compiled or gameplay-verified.

### User-observed diagnostic
```
[CGS GUN INPUT] Orange action registered=0 contextActivated=0
[CGS GUN INPUT] Player=1 requested color=0
[CGS GUN RPC] Server received player=1 color=0
[CGS GUN] Player=1 fired color=0 ...
```

This proves the Blue path is firing reliably and the Orange action **does not exist in the loaded input manager**. A key swap alone would not fix the problem.

### Actual changes

1. `addon.gproj`: adds the Reforger `Configurations / GameProjectConfig PC / InputManagerSettings` block with `Default "{6AB21D161F2E7525}Configs/System/chimeraInputCommon.conf"`, using the existing `.conf.meta` resource GUID. The previous PR added a standalone `.conf` but did not configure the input manager to load it.
2. `Configs/System/chimeraInputCommon.conf`: changes its root to `ActionManager : "{795184CF9AD764DB}Configs/System/chimeraInputCommon.conf"` so the mod config **inherits the original game's complete input set**. This avoids dropping vanilla input mappings when the project points its input manager at the new config.
3. Remaps `CGSPortalGunOrange` to **middle mouse (mouse:button2)**; the gamepad left trigger is retained as an optional alternative. `CGSPortalGunContext` is activated only while our portal gun is equipped.
4. Changes documentation-only comments in `CGS_PortalPlayerController.c`: LMB **Blue** stays exactly as previously working (`CharacterFire`); RMB remains default vanilla ADS; MMB **Orange** uses the dedicated `CGSPortalGunOrange` action.
5. No changes to portal gun prefab, customized hand-fit, model, collision, weapon animations, raycast placement, player ownership map, portal teleportation, momentum, or existing Blue click handling.

### Test plan
- Pull branch, close and relaunch Workbench (game project input settings are loaded at startup), compile.
- Equip `Portal_Gun_.et` and look for **`[CGS GUN INPUT] Orange action registered=1 contextActivated=1`**. If either remains zero, send fresh logs.
- Left click a surface → Blue portal. Middle mouse click a different surface → Orange portal. Check `[CGS GUN INPUT] Player=1 requested color=1`, `[CGS GUN RPC] Server received player=1 color=1`, `[CGS GUN] Player=1 fired color=1`.
- Right click should continue ADS; confirm middle-click does not activate unrelated actions. Shoot Orange twice and verify only Orange is replaced.
- Confirm movement, menu inputs, other weapons, and multiplayer replication remain functional: this patch changes the **project's input default**, so the inherited vanilla input config must be validated.

**Note:** Workbench can have extra addon dependencies with their own overrides to `chimeraInputCommon.conf`. Our config inherits the vanilla base and may need additional handling if another addon modifies the same resource, so test with the project's actual mod set.


## Phase 2.2 — Automatic portal alignment to hit surfaces

**Branch:** `feature/portal-auto-surface-orientation` — **Workbench compile and in-game testing required**.

I kept the proven firing and traversal system and updated only `Scripts/Game/CGS_Portal/CGS_PortalGunComponent.c`. I did not change the gun prefab, blue/orange prefab IDs, player ownership, input configuration, model, replicated portal components, teleport, or momentum code.

### What the gun does on every valid raycast

1. Obtain the true surface normal from `TraceParam.TraceNorm`; normalize and flip it if it points along the incoming ray, so the opening always faces the shooter.
2. Categorize the surface for diagnostics: **FLOOR** (`normal.Y >= 0.85`), **CEILING** (`normal.Y <= -0.85`), **SLOPE** (`abs(normal.Y) >= 0.4`), or **WALL**.
3. For walls and ordinary slopes, project world vertical onto the impact plane to preserve the portal's upright orientation.
4. For floor/ceiling surfaces, project the camera's aiming direction onto the impact plane to define the portal's oval long axis. If the camera points straight down/up, use projected world +Z as a stable fallback.
5. Build an orthonormal surface frame: `portalRight = portalUp x normal`, then `portalUp = normal x portalRight`. The basis remains right-handed across all four surface types.
6. Preserve each portal prefab's existing mesh-facing convention when filling the `EntitySpawnParams.Transform` matrix:
   - **Blue** front = **local -Y**, configured in `Blue_Portal.et` as `m_iFacingAxis 2`; use `[right, -normal, up, position]`.
   - **Orange** front = **local +Y**, configured in `Orange_Portal.et` as `m_iFacingAxis 1`; use `[right, normal, -up, position]`.
   - Spawn center remains `hitPosition + normal * m_fSurfaceOffset`; existing offset constraints, owner replacement, surface rejection, replication, and shot cooldown are unchanged.
7. Once spawned, log the actual mesh-facing normal through the unchanged `CGS_PortalComponent.GetPortalBasis`, along with the surface type and the alignment dot product. Expected `alignment=1`. Values below 0.9 log a warning, but don't delete the portal; this allows diagnosis of mesh/prefab orientation mistakes without breaking the working gun.

### Validation steps

- Pull branch and compile Game scripts.
- Fire **Blue on LMB** and **Orange on middle mouse** at a vertical wall. Both should stand **vertically** instead of lying on the ground, with green direction arrow away from the wall.
- Fire at the floor: portal should be flat, facing **UP**; try walking on it.
- Shoot the **underside of a ceiling**: portal should be flat, facing **DOWN**; try falling out of it.
- Shoot a slope: oval should lie flush with the sloped surface.
- Check the log:
  `[CGS GUN ALIGN] Player=1 color=0 surface=WALL hitNormal=<...> portalNormal=<...> alignment=1 top=<...>`
  (same for Orange, FLOOR, CEILING, SLOPE).
- Test wall-to-wall teleport both ways and floor-to-ceiling travel (keep original physics/return settings).
- Test same-color replacement and separate players' pairs, to confirm no regressions.

**Known limitations:** Only static collision surfaces are supported. This patch aligns the *portal opening*, not the *character's rotation* after teleportation; the existing safe-player teleport intentionally preserves upright player facing. Portal surface-size validation and clipping into narrow/uneven walls remain future work.


## Phase 2.3 — Portal picture-in-picture isolated Workbench experiment

**Branch:** \`feature/portal-pip-render-prototype\` (uncompiled; gameplay not verified).

We have two new imported frame models, each already containing a separate portal view-plane material:
- \`Common/Models/Blue_Portal_Frame.xob\` GUID \`{82397680AD75CA10}\`
- \`Common/Models/Orange_Portal_Frame.xob\` GUID \`{C2D10223091B0296}\`

The new independent test prefabs are:
- \`Prefabs/Weapons/Core/Blue_Portal_PIP_Test.et\` — PIP **enabled**, camera index **20**
- \`Prefabs/Weapons/Core/Orange_Portal_PIP_Test.et\` — PIP **disabled**, camera index **21**

**Do not replace the gun's working Blue/Orange prefabs.** Both PIP test prefabs have \`CGS_PortalComponent.m_bEnabled 0\`, so the experiment tests cameras/materials and does not teleport players.

### Rendering mechanism

Based on the actual Reforger vanilla \`SCR_PIPSightsComponent.c\`:
1. Spawn both test prefabs into a Workbench test world, positioned far enough apart to see different scenery.
2. New \`Scripts/Game/CGS_Portal/CGS_PortalPIPTestComponent.c\` discovers the opposite-color *test* prefab on the same local client.
3. The Blue test prefab aims local camera index 20 outward from Orange's opening by calling \`GetWorld().SetCameraEx(20, ...) \`.
4. Create vanilla \`{EF091399D840192D}UI/layouts/Sights/PictureInPictureSightsLayout.layout\`, find its \`RTTexture0\` and \`RenderTarget0\` widget objects.
5. \`RenderTarget0.SetWorld(world, 20)\` renders the second view; \`RTTexture0.SetRenderTarget(bluePortalEntity)\` streams those rendered pixels into its mesh section with the new \`Common/Models/Data/CGS_Portal_PIP_Screen_TEST.emat\` (\`BCRMap "$rendertarget"\`). New blue/orange \`.xob.meta\` files replace **only** the screen material mapping; the frame metal/glow assignments remain as imported.
6. If the first feed works, enable the Orange PIP test component locally to try the reverse view, camera slot 21. **Two active PIP portals can capture each other's screens, which may recurse. Test one feed first.**

**Expected first log:**
\`\`\`
[CGS PORTAL PIP] Registered test portal color=0 enable=1 camera=20
[CGS PORTAL PIP] Registered test portal color=1 enable=0 camera=21
[CGS PORTAL PIP] LIVE test feed bound: color=0 camera=20 partner=1 fps=12 scale=0.5
\`\`\`

### Limitations / follow-up

- **Unverified proof-of-concept**, not a finished portal window. May require changing layout/widget binding or material settings after testing in Workbench.
- Uses one **static test pair** only, not personal-player-pair networking.
- The remote camera is presently fixed at the partner opening, **not** viewer-relative or parallax-correct. The full effect requires per-viewer camera transform and surface masking.
- No automatic clipping behind the destination portal, recursive portal occlusion control, or optimization for many pairs.
- Every running **client** renders locally. A dedicated server should not create UI or cameras.
- Render target sizes may differ from game resolution; first test is capped at 12 FPS and 0.5 scale to limit cost.
- New generated textures (BCR/NMO) are **not required for the PIP screen**, because that specific test material uses \`BCRMap "$rendertarget"\`; metal/glow can keep their own materials.
- The working PR #15 teleport re-entry-loop fix is included in this test branch so that changing branches won't regress the user-tested stability. When PR #15 is merged into main, GitHub will naturally deduplicate the overlapping commits.

### Smoke test

1. Compile Game module. If it fails, send the *first* errors mentioning \`CGS_PortalPIPTestComponent.c\`.
2. In Resource Manager check \`Blue_Portal_Frame.xob.meta\` assigns the **ViewPlane** source material to \`CGS_Portal_PIP_Screen_TEST.emat\`. Force-reimport the XOB if necessary.
3. In World Editor place one Blue and one Orange \`*_PIP_Test.et\`. You can leave them in the same world as stable portals; the test entities do not alter the gun.
4. Enter Play. Look for logs above; inspect Blue screen for a view from Orange's location. Try moving AI near Orange to prove it is a **live** feed.
5. If it shows black or a static color, send the PIP logs, screenshot of **screen material** in Resource Manager, and any Layout warnings. Do not overwrite the stable prefab with this test.
6. Only after the single live feed succeeds should the Orange feed be enabled and tested for recursion / cost.


## Phase 2.3a — Fix blank PIP screen (ScriptCamera + dedicated layout)

**Observed:** `[CGS PORTAL PIP] LIVE test feed bound` appeared in the Workbench log while the portal screen showed no image. That message proves widget/mesh binding was attempted, not that a world camera was rendering.

**Root-cause hypothesis:** The original prototype used `BaseWorld.SetCameraEx` without a `ScriptCamera` entity. Both vanilla Reforger PIP optics and the user-supplied old SVC CCTV mod create an actual camera entity and configure its camera slot before connecting the rendering widgets.

Changes in `fix/portal-pip-live-camera`: 
- Create and configure a local `ScriptCamera` for the remote portal camera slot, and follow the partner transform every frame with `FreeFly` disabled.
- Apply slot FOV, near/far planes, perspective camera type, and lens-flare settings, as in the old CCTV implementation.
- Defer camera/layout initialization for 2.2 seconds to allow the game renderer and UI workspace to initialize.
- Provide our own 512x512 `UI/Layouts/CGS_PortalPIP_Test.layout` containing `RTTexture0` and `RenderTarget0`, instead of using the rifle-scope layout.
- Preserve the current portal test prefabs, frame meshes, glow materials, PIP `$rendertarget` material, normal gameplay portals, gun controls, and teleport scripts.
- Add one-time debug logs confirming the real camera entity exists and showing world camera matrix readback.
- Log a warning if the Orange test frame points mostly up/down. Neutral test prefab orientation can cause the remote camera to look only at sky/ground.

### Validate in Workbench
1. Pull this branch and compile Game scripts. This is still an untested change.
2. Place one `Blue_Portal_PIP_Test.et` and one `Orange_Portal_PIP_Test.et` manually in the editor. Rotate both test frames upright. Aim Orange toward a tree/building/vehicle.
3. Enter Play and look at Blue's plane. The Orange feed is intentionally OFF until Blue is confirmed.
4. Expected diagnostics:
   `[CGS PORTAL PIP CAMERA] ScriptCamera CREATED slot=20 origin=... forward=...`
   `[CGS PORTAL PIP] LIVE test feed bound: color=0 camera=20 partner=1 fps=12 scale=0.5`
   `[CGS PORTAL PIP CAMERA] World camera READBACK slot=20 origin=... forward=...`
5. If all camera messages appear but the plane stays blank, check `Blue_Portal_Frame.xob` material assignment: `CGS_Portal_BLUE_Screen_PLACEHOLDER` should map to `CGS_Portal_PIP_Screen_TEST.emat` containing `BCRMap "$rendertarget"`. Force reimport the XOB after checking it.
6. If the camera forward vector is nearly vertical, rotate the Orange test frame upright to get a scene instead of sky/ground.

Do not turn on the Orange feed yet. Two-way recursive rendering, viewer-relative parallax, full multiplayer pairing, and clipping are future phases.

## Phase 2.3b — PiP viewport dimensions and independent visual diagnostics

I ran the second test after merging PR #17. The camera was created and world-camera readback succeeded, but the Blue portal still showed no live picture.

### Findings

- The log confirmed slot 20 had a real camera, but its forward direction was `(0,1,0)`, pointing straight up, which could make the view featureless.
- The initial sample also logged camera origin around `(0,2.25,0)`. A later world load showed separately placed test portals around `(2215,55,2904)` and `(2251,55,2879)`. These appeared in different reload sections of the log, so I need the same-run position diagnostic to know if there is a real camera-coordinate problem.
- The nested RenderTarget0 in `CGS_PortalPIP_Test.layout` used stretch anchors plus `SizeX -512` and `SizeY -512` under a 512px parent. Those offsets can collapse the drawable region. I changed both SizeX and SizeY to 0 for full stretch and added actual widget dimension readback.

### Changes

- `UI/Layouts/CGS_PortalPIP_Test.layout`: fixes RenderTarget0 stretched size, using SizeX 0 and SizeY 0.
- `Scripts/Game/CGS_Portal/CGS_PortalPIPTestComponent.c`: adds widget-size logging, logs partner world position on camera creation and readback, and adds a horizontal-facing camera fallback when test portal is pointing upward.
- `UI/Layouts/CGS_PortalPIP_DebugHUD.layout` and `.meta`: introduces an optional small image on my local HUD that samples the same RTTexture widget directly using `ImageWidget.SetImageTexture(0, texture)`. This bypasses the XOB/material assignment completely.
- `Blue_Portal_PIP_Test.et`: HUD enabled, camera follows partner and looks horizontally if the test frame is face-up.
- `Orange_Portal_PIP_Test.et`: still has its own live PiP feed disabled; it is the source location for Blue's test.

### My next Workbench test

1. Pull PR #18, compile, enter Play with one Blue and one Orange PiP test prefab; no gun portal changes.
2. The temporary camera preview should be visible in a **256x256 area at the upper-left HUD**. Inspect the in-world Blue portal plane separately.
3. Read `[CGS PORTAL PIP CAMERA] ScriptCamera CREATED ... partnerPosition=...` and `[CGS PORTAL PIP CAMERA] World camera READBACK ... partnerPosition=...`. Camera origin should be close to Orange test portal world position, offset roughly 0.25m in the chosen viewing direction.
4. Read `[CGS PORTAL PIP LAYOUT] RTTexture0=...x... RenderTarget0=...x...`. Both dimensions must be greater than zero.
5. If the **HUD shows the remote world** but the portal plane does not, the camera and widget work. Inspect `Blue_Portal_Frame.xob.meta` source material `CGS_Portal_BLUE_Screen_PLACEHOLDER` and the `CGS_Portal_PIP_Screen_TEST.emat` assignment; reimport XOB.
6. If **both HUD and portal plane are blank**, inspect render-widget dimensions and camera origin/forward before changing the model.
7. Keep Orange feed disabled until the one-way Blue test is verified. Disable the debug HUD when done.

This is a temporary diagnostic PIP test. The existing functioning portal gun and standard Blue/Orange portal prefabs, all shooting controls, re-entry protection, and teleport logic remain untouched.

## Phase 2.3c — Live camera diagnostic, stale pair guard, and accurate world transforms

### What I saw in my Workbench test

I ran the PiP branch and saw a solid white 128px square in the upper-left corner when approaching the new Blue frame. That square is the optional HUD diagnostic that was added in PR #18, not a model error or a real portal feature. Both the HUD preview and the oval ViewPlane remained white.

My logs confirm these facts in one game session:
- World initialisation placed the Blue test prefab around `(2215,55,2904)` and the Orange test prefab around `(2251,55,2879)`.
- The camera/partner log instead reported `partnerPosition=(0,2,0)` and `ScriptCamera` origin `(0,2,0.25)`. This is **not** Orange's expected location; rendering a view at world origin is not meaningful proof that the portal camera works.
- Both `RTTexture0` and `RenderTarget0` reported positive size, approximately 180x180 physical pixels, so the earlier zero-area hypothesis is no longer sufficient to explain the blank view.
- The old HUD used `ImageWidget.SetImageTexture(0, RTTexture0)`; a white widget can mean missing/invalid pixels, not necessarily a working camera.

### What I changed in this branch

1. I changed `FindOppositeTest()` to only accept live, not-deleted, same-world portal objects and prefer the nearest opposite-color test portal. This guards against stale instances in Workbench and multiple temporary test pairs.
2. I changed the remote camera placement to use the opposite portal entity's **GetWorldTransform()[3]** instead of `GetOrigin()`. I also use `SetWorldTransform()` on the spawned `ScriptCamera`.
3. I log `blueWorld`, `blueGetOrigin`, `orangeWorld`, `orangeGetOrigin` as well as camera world position and partner-world position. That tells me whether the origin mismatch persists.
4. I replaced the white HUD `ImageWidget` with a **direct RenderTargetWidget** called `PIPDebugCamera`, bound to camera slot 20 via `SetWorld(world, slot)`. This is independent of `$rendertarget`, XOB material mapping, and RTTexture0.
5. I made this diagnostic **exclusive**: with `m_bDebugHUD=1`, the direct camera renders on the HUD and the in-world portal material is intentionally not fed. With `m_bDebugHUD=0`, the HUD disappears and the render-to-mesh pipeline is enabled. I don't run both passes together, because the historical SVC CCTV mod warned about native renderer crashes when too many targets came up simultaneously.
6. I made no changes to the working player portal prefabs, blue/orange input, normal gun logic, collision, transfer, or exit-loop protection.

### My next one-way test

1. Pull this branch into my Workbench and compile.
2. Load one Blue and one Orange `*_PIP_Test.et`; in Blue's component keep `m_bPreviewEnabled=1` and `m_bDebugHUD=1` for stage A. Orange remains `m_bPreviewEnabled=0`.
3. Start Play. The upper-left area should now be a **direct camera-rendered scene**, not a white `ImageWidget` placeholder. The Blue portal itself should remain white during this HUD-only phase, by design.
4. Compare `[CGS PORTAL PIP PAIR] blueWorld=... orangeWorld=...` with `[CGS PORTAL PIP CAMERA] ... partnerWorld=...`. If the camera still reports world origin while the editor has the portals at 2200+ coordinates, the partner selection/transform is still wrong.
5. If the HUD shows Orange's scenery correctly, disable Blue's `m_bDebugHUD` (set to 0), reload Play, and inspect the in-world Blue portal plane. Only then do I investigate XOB material mapping / `$rendertarget` if the screen remains blank.
6. Never enable Orange's second camera feed until this one-way proof works.

This is still an uncompiled Workbench experiment and should remain a draft PR until the direct HUD camera is verified.

## Phase 2.3d — Switch verified remote camera from HUD to Blue portal mesh

**My successful test:** After pulling PR #19, I can see a real-time picture of the distant trees/area at Orange's location in the small HUD camera window. My logs confirm Blue and Orange world coordinates, a correctly placed ScriptCamera on slot 20, and the message `[CGS PORTAL PIP HUD] DIRECT world-camera widget created`.

I verified that **rendering a second view of the same world is technically possible in Reforger**. However, the render-to-mesh path has not yet been verified; the oval stayed white on purpose during HUD-only diagnostic mode.

**This change:** I switched `Prefabs/Weapons/Core/Blue_Portal_PIP_Test.et` from `m_bDebugHUD 1` to `m_bDebugHUD 0`. The existing `CGS_PortalPIPTestComponent.c` now uses its Stage B route: it calls `RenderTargetWidget.SetWorld(world, cameraIndex)`, enables that widget, then calls `RTTextureWidget.SetRenderTarget(GetOwner())`. The frame XOB's ViewPlane material is already assigned to `CGS_Portal_PIP_Screen_TEST.emat`, which uses `BCRMap "$rendertarget"`. The small HUD preview no longer appears.

### Next test

1. Pull the Stage B branch, compile Game scripts, enter Play with one `Blue_Portal_PIP_Test.et` and one `Orange_Portal_PIP_Test.et`.
2. Look at Blue's oval plane: it should now display the scene previously shown in the debug HUD, instead of solid white.
3. Confirm `[CGS PORTAL PIP MESH] Render target bound to ViewPlane. Camera diagnostic HUD disabled.` and `[CGS PORTAL PIP] LIVE mesh feed bound: color=0 camera=20 partner=1 ...`.
4. Confirm `[CGS PORTAL PIP LAYOUT] ... RenderTarget0=...x...` has positive dimensions again.
5. If the plane stays white even with correct logs, inspect the frame's source material assignment for `CGS_Portal_BLUE_Screen_PLACEHOLDER` and **reimport `Blue_Portal_Frame.xob`**. Also inspect the ViewPlane normals/UVs and `CGS_Portal_PIP_Screen_TEST.emat`. The camera is already independently proven.
6. Leave Orange feed off until the one-way Blue mesh feed works. Two-way recursive camera rendering, per-viewer perspective, and multi-client performance remain future phases.

**Safety:** Both stage-A and stage-B tests use separate `*_PIP_Test.et` prefabs with teleport disabled. I made no change to the working gun, its normal portal prefabs, controls, placement or teleportation. All camera tests remain experimental until tested in Workbench.

## Phase 2.3e — PiP Blue screen shader + runtime imported XOB screen-slot remap

I tested PR #20: the camera still renders correctly and the scripts report that the mesh render-target binding happened, but I still cannot see the live view on Blue's white oval. The camera is correctly positioned ~0.25m in front of Orange's world coordinates `(2251.21,55.53,2879.14)`, the world-camera slot 20 has a valid direction, and both `RTTexture0` / `RenderTarget0` report a nonzero ~257px region. The independent direct-HUD render previously showed genuine live scenery. That narrows the unknown to the render-to-mesh path rather than the camera itself.

### Material findings

- The old `CGS_Portal_PIP_Screen_TEST.emat` was a copy of a weapon optic shader with `Emissive 1 1 1 0` and `EmissiveLV -3.429`. Although this shader works in other contexts, it might be inappropriate for our imported opaque screen plane.
- The Reforger Radar Development Framework screen material uses both `BCRMap "$rendertarget"` and `EmissiveMap "$rendertarget"`; the public minimal render-to-mesh test in Cops-n-Robbers uses `Color 1 1 1 1`, `Emissive 1 1 1 1`, and `EmissiveLV 5` to make it visible. I combined these characteristics in the dedicated screen material and kept the metal/glow materials untouched.
- The Radar Development Framework also documents an intermittent issue where `.xob.meta` material assignments leave a mesh using its original surface material, even though the assignment exists in the resource file. The workaround uses `entity.SetObject(vObject, "$remap 'source material name' 'new .emat resource';")`. That call is a documented Enfusion API, and stock `SCR_FlagComponent` uses it as well.

### Changes

1. In `Common/Models/Data/CGS_Portal_PIP_Screen_TEST.emat` I set opaque white base/emissive tints, a bright emission level, `BCRMap` and `EmissiveMap` pointing at `$rendertarget`.
2. In `CGS_PortalPIPTestComponent.c` I added `ForceScreenRenderMaterial()`. It runs only in the experimental Stage B test prefab, loads the material asset, uses `GetOwner().GetVObject().GetMaterials(...)` to print the actual imported material names, remaps only screen/ViewPlane slots, and leaves the metal frame and blue/orange glow slots alone. The call happens before `RTTextureWidget.SetRenderTarget(GetOwner())`.
3. If the expected source slot is absent from the returned materials, I also attempt the exact original imported name `CGS_Portal_BLUE_Screen_PLACEHOLDER` / `CGS_Portal_ORANGE_Screen_PLACEHOLDER` as a fallback and print a diagnostic.
4. I did not touch the normal Blue/Orange gun portal prefabs, placement, input, teleportation, or the already working anti-loop protection.

### My next Workbench validation

1. Pull this branch, compile, and start Play with exactly one Blue and Orange PiP test prefab. I leave Orange's live feed disabled, with the Blue HUD debug view disabled so the screen receives the image.
2. Look for `[CGS PORTAL PIP MATERIAL] Frame VObject reports N material slot(s)`, followed by `Source slot[i]='...'` and `Runtime XOB screen remap issued`.
3. Inspect whether the Blue oval now shows the remote live scenery. If the frame is still white, copy the material slot diagnostics and the camera/mesh lines. Those will show whether the XOB actually contains the `...Screen_PLACEHOLDER` section.
4. If the material loads and the correct source slot was successfully remapped but the oval still stays blank, I will test the same `RTTexture0` with a simple changing text payload before introducing a second camera or changing the working teleport system.

**Status:** GitHub PR only; uncompiled in Workbench. Shader and material remap behavior still require in-game confirmation.

## Phase 2.3f — Live Blue portal visible, but remote scene too dark

I have now confirmed that the imported Blue frame receives a genuine live picture from Orange. After PR #21, the ViewPlane showed the distant cabin and trees through the oval, rather than an opaque white screen. The camera was correct all along, and the actual imported XOB screen material name was `CGS_Portal_BLUE_Screen_PLACEHOLDER`; runtime `$remap` and the emissive render-target material were the important final connection.

### My next correction: copy the active player's HDR exposure

The live image is significantly darker than the current player's world view. I checked the official Reforger `SCR_2DPIPSightsComponent.UpdateHDR()` implementation. Vanilla PIP sights query the current player's camera via `GetGame().GetCameraManager().CurrentCamera()`, then obtain the main camera's exposure using `BaseWorld.GetCameraHDRBrightness(mainCameraIndex)` and apply it to the picture-in-picture camera with `BaseWorld.SetCameraHDRBrightness(pipIndex, hdrBrightness)` **every frame**.

My portal camera did not previously do that. It had its own exposure which might not match the game camera. I added `SyncRemoteCameraHDR()` in `CGS_PortalPIPTestComponent.c`, using the stock PIP exposure-matching method.

### Changes in the test branch

- New GM/Workbench component parameter `m_bMatchPlayerHDR`: 1 by default. If disabled, the PiP camera's exposure remains unmanaged like the old version.
- New parameter `m_fHDRExposureMultiplier`: 1.0 by default, meaning match the live player's camera. If needed, I can try 1.5 or 2.0 to increase the remote image brightness, or lower the value if the picture becomes too bright.
- During setup, synchronize PiP HDR before activating its RenderTargetWidget. During every frame, update to match changes in the player's exposure (moving indoors/outdoors or through time-of-day/weather changes).
- New one-time `[CGS PORTAL PIP HDR]` logging reports the main camera index, its exposure, the portal camera slot (20), its applied exposure and the multiplier.
- The Blue test prefab explicitly sets HDR matching ON and multiplier 1.0.
- This is only a brightness fix. I **did not change** the now-working emissive screen shader, runtime source material remap, mesh XOB/meta, camera placement, working gun prefabs, player controls, teleportation, or the anti-reentry protection.

### My next Workbench test

1. Pull this branch (PR #22) and compile Game scripts.
2. Keep the same Blue and Orange PiP test prefabs. The Blue HDR-matching parameter should read ON, exposure multiplier 1.0.
3. In Play, compare the live picture on Blue to the nearby terrain and inspect the `[CGS PORTAL PIP HDR]` output.
4. If still visibly dark, try increasing `m_fHDRExposureMultiplier` to 1.5 or 2.0 on Blue's CGS Portal PiP Test component, then rerun. If the image brightens but has poor contrast, the next isolated shader test will use the `MatCommon { AlbedoMap "$rendertarget"; DisablePrexposure 1 }` pattern from a known existing PiP material. I will not discard the currently working screen material until that alternative has been tested.
5. Leave Orange's camera disabled until one-way Blue has correct exposure. No recursion protection, viewer-relative parallax, or multiplayer personal portal pairing has been added.

**Status:** Script changes are pushed, but they still require Workbench compilation and a brightness screenshot.


## Phase 2.4 — Both live portal views integrated into the gun (first-person notes)

I confirmed that the Blue portal's center finally displays a genuine moving scene from Orange's location. My latest image shows the remote building and trees inside the oval. The PiP HDR exposure fix also helps: my log showed player-camera exposure around 0.00495584 and matched portal-camera exposure around 0.00495585 after the first frame. The other important fixes were the original XOB material slot name, the runtime screen-only `$remap`, and a MatPBRBasic screen material that samples `$rendertarget` for both BCR and emission.

I am now applying the same setup to Orange **and** to the real portals created by my gun. This phase is code-complete on the GitHub branch but is not yet compiled or multiplayer-tested in Workbench.

### What I changed

- `Prefabs/Weapons/Core/Blue_Portal.et` now uses my imported `Blue_Portal_Frame.xob`; `Orange_Portal.et` uses `Orange_Portal_Frame.xob`. Their existing `CGS_PortalComponent`, `RplComponent`, traversal data, crossing ranges, momentum, and original prefab/resource GUIDs are unchanged. The gun still spawns exactly these original prefab resource paths and still uses **LMB = Blue**, **MMB = Orange**, and **RMB = ADS**.
- I added `CGS_PortalPIPLiveComponent` to each real gun portal. It creates a **client-local**, fixed-direction remote `ScriptCamera`, uses the existing `RTTexture0` / `RenderTarget0` layout, forces the imported ViewPlane's screen material with `SetObject("$remap ...")`, and connects the live camera view directly to the frame's center plane. The metal frame and colored glow-ring materials are left separate.
- Blue looks outward from its matching Orange portal. Orange looks outward from its matching Blue portal. Both views use actual exit-facing normals, including wall, floor, ceiling, and sloped placements. There is no old horizontal camera override in the production gun component.
- Every client pairs personal portals using the **replicated** `m_iPortalOwnerID` in `CGS_PortalComponent` (new `[RplProp()]` plus a server `Replication.BumpMe()` when assigned). I cannot use the server's static portal manager map for client-side camera pairing because that map does not replicate. World/test portals with owner ID 0 do not match a gun portal.
- I matched the portal camera's HDR exposure to the local player's camera on each frame using the official PIP scope approach. Default multiplier is 1.0, with a component property to adjust brightness as needed.
- To avoid rendering every player's portal camera at the same time, each CLIENT renders at most the **two closest eligible portal faces within 120 meters**, on camera slots 26 and 27. Each feed defaults to **12 FPS, half render scale**. As I walk to another portal, my client releases old render targets before reusing its slots. The server does not stream camera images or create UI widgets on a dedicated server.
- `Orange_Portal_PIP_Test.et` also has its live PiP feed and HDR matching enabled now, so I can first verify both directions with the simple static test frames (Blue test stays enabled). Both static PiP test prefabs continue to have teleportation disabled.
- I gave my previously unconfigured Blue and Orange glow ring .emat materials their own matching emissive colors rather than plain white. The ViewPlane keeps the proven high-brightness material, unchanged.
- I did **not** change the gun's shoot validation, surface trace/raycast, automatic portal transform, per-player replacement logic, teleport method, momentum, or the already successful anti-reentry fix.

### My testing order in Workbench

1. **Compile first.** If Game scripts fail, send the first compiler error for `CGS_PortalPIPLiveComponent.c` or the new `[RplProp()]` in `CGS_PortalComponent.c`.
2. **Static two-way preview:** Place one `Blue_Portal_PIP_Test.et` and one `Orange_Portal_PIP_Test.et` in the same world. Both are now enabled at 12 FPS and half resolution. View Blue and Orange from different positions and confirm that each displays the remote scene, not its own scene. Test separately from other active gun pairs at first.
3. **Gun integration:** Equip the regular CGS portal gun. Fire LMB Blue into a valid static wall, then MMB Orange at a second wall. The spawned portals should use the new colored 3 m × 4 m frames, with a live center on both. Move between them to confirm the corresponding remote view updates as a character moves in front of the opposite portal.
4. **Teleport safety:** Walk through Blue and back through Orange. Check that the player teleports once with no endless bounce, and that moving/replacing either portal with another shot still clears and rebinds its associated client view.
5. **Orientation:** Fire a Blue wall portal and an Orange floor/ceiling portal. The image should follow the true remote portal normal rather than the test camera's horizontal override.
6. **Multiplayer:** With two clients, each owning a different Blue/Orange pair, confirm that neither player's camera renders from another owner's exit, and that the server remains authoritative for teleporting. Watch for `[CGS GUN PIP] LIVE owner=... color=... remoteColor=... slot=...` in the local client log if `m_bDebug` is enabled.
7. **Load / recursion:** Put both portals facing one another and watch for visual feedback, high GPU time, or recursive image artifacts. Proper portal clipping, viewer-relative parallax and recursive portal masking are not yet implemented.

### Important limitations

The first production preview is **a live remote camera image on a flat plane**, not yet a true per-eye geometric portal. I haven't implemented perspective parallax based on viewer position or clipping of objects between the exit camera and the exit plane. Opposing cameras may capture each other's screen and produce visual feedback. Each local player is capped at two simultaneous live screens for performance; more distant personal portals remain present and teleportable but may show only their inactive screen material until selected for rendering. Slots 26 and 27 are reserved for this local test and could conflict with other mods that use those particular camera indexes. A custom camera-slot allocator and per-client load policy will be needed for production on large modded servers.

**Status:** unverified in Workbench for both-way runtime gun integration; the older one-way Blue test and HDR fix have been user-tested and remain our fallback baseline.

## Phase 2.4a — Fix portal owner-ID annotation compile syntax

After integrating the two-way gun portal picture-in-picture system in PR #23, Workbench reported six parser errors in `Scripts/Game/CGS_Portal/CGS_PortalComponent.c` before I could test the new portals.

I found that I had written `[RplProp()]` immediately above `[Attribute(...)]` as separate annotation lines for the same `m_iPortalOwnerID` field. In Enfusion these two annotations must be grouped in **one bracket list**, which is how Bohemia's own scripts combine `Attribute` and `RplProp`.

I corrected the field to `[Attribute("0", UIWidgets.EditBox, "Portal owner player ID. 0 = shared test portal"), RplProp()]`. The owner-ID value, call to `Replication.BumpMe()` on server assignment, existing gun logic, prefabs, traversal, teleportation, exit guard, and camera code are unchanged.

This is PR #24, an urgent one-line parser fix. It still needs a full Game script compile in Workbench. I will only diagnose any subsequent errors from the *new* compiler output; I should not assume all errors are resolved until Workbench confirms it.


## Phase 2.4b — Align my imported portal frame to walls, floors and ceilings

After merging the new live PiP frames into the real gun portal prefabs, I found that the portal **visuals** were rotated 90 degrees. My screenshot shows a horizontally sliced ring against a wall and an upright portal when placed on the ground.

The logs prove the **existing logical portal normal was aligned correctly** with the raycast surface. For example, my wall hit logged `alignment=1`, and my floor hits logged `normal=<0,1,0> alignment=1`. I confirmed that the problem was not the hit trace or the teleport calculation: the old gun rotation and `m_iFacingAxis` settings were still configured for the previous sample XZ-facing meshes.

The imported `Blue_Portal_Frame.xob` and `Orange_Portal_Frame.xob` use a flat XY viewing surface, so their visible face is local **+Z**. The old prefabs were still configured for `Blue: -Y` and `Orange: +Y`. Placing the new visual mesh into the old transform produced the 90-degree mismatch.

### What I changed

- I updated `CGS_PortalGunComponent.c` so **both colors** spawn with `Transform[0]=portalRight`, `Transform[1]=portalUp`, `Transform[2]=surfaceNormal`. This aligns the imported frame's face and its real screen plane with the surface normal.
- I set `CGS_PortalComponent.m_iFacingAxis 0` in both `Blue_Portal.et` and `Orange_Portal.et`. Axis 0 selects local **+Z** for portal traversal, precisely matching the model now. The crossing and exit basis stays consistent with the visual model.
- I did **not** modify either imported XOB file, its material slots, the working PiP render-to-texture component, per-client HDR handling, the raycast, the server-owned teleport method, the anti-reentry loop fix, momentum, or the gun input setup.
- I kept all original prefab paths, entity GUIDs and the remaining portal configuration settings unchanged.

### My next Workbench tests

1. Pull the orientation fix and compile.
2. Shoot a Blue portal into a vertical wall: the **entire ellipse should be upright and flush with the wall**, rather than a horizontal bar. Shoot Orange at another wall and check the same.
3. Shoot Blue and Orange on flat terrain: the **whole portal should lie flat on the ground** with its opening facing upward, not stand like a doorway.
4. Try a slope and a ceiling to verify the imported frame follows the true surface normal.
5. Verify the logs still say `[CGS GUN ALIGN] ... alignment=1` for all supported surfaces, and that each color shows the other portal's PiP feed.
6. Walk through both directions and check that my anti-reentry protection still stops bounce loops. When I replace a portal with another gunshot, the matching view should rebind without stealing another player's pair.

**Status:** This is a source-level repair pushed as a separate PR, not yet compiled/tested by Workbench. If a model still appears 90 degrees out after the update, I will compare the actual mesh front axis and its Workbench import rotation before changing any gameplay/teleport code again.


## Phase 2.5 — CGS Pinkifier Gun (independent novelty weapon)

I wanted a funny alternative to the portal gun: a pistol that turns another player's or AI soldier's visible body and outfit hot pink when shot, with **zero damage**. I have added a first prototype on `feature/pinkifier-nonlethal-gun`, without replacing or retasking my working portal gun.

### Assets I added

- `Prefabs/Weapons/Core/Pinkifier_Gun.et` with its own resource metadata and independent internal component GUIDs. The prefab can be dragged directly into a Workbench world for testing.
- `Scripts/Game/CGS_Portal/CGS_PinkGunComponent.c`: new server-authoritative Pinkifier shot logic, separate from `CGS_PortalGunComponent`.
- `Common/Models/Data/CGS_Pinkifier_Pink.emat`: bright hot-pink replacement material for character meshes, clothing and other visible child meshes.
- `Common/Models/Data/CGS_Pinkifier_Gun.emat`: pink paint for the gun model itself, applied **only to the Pinkifier instance** with a material-slot remap; my original CGS Portal Gun stays visually unchanged.

I reused my existing `Portal_Gun.xob`, basic pistol handling/IK, inventory pickup/holster actions, and empty non-ballistic muzzle for the Pinkifier's first model. I can import a new model later without changing this paint-shooting logic.

### Shooting and multiplayer design

- **LMB** shoots a non-damaging, server-validated visual ray at up to 75 meters. **RMB** retains normal aiming; there is no secondary Pinkifier action.
- The server verifies that the player is alive, actively holding their own Pinkifier, the submitted camera start is near their actual eyes, the aim vector is valid, the shot cooldown elapsed, and the **first ray hit** is a character (or something attached to a character). Walls or vehicles in front of the target block the paint ray.
- The server extracts the affected `ChimeraCharacter` and its replicated entity ID. It uses a reliable Broadcast RPC to instruct each connected client to apply the same material locally to that character. This is **not** a damage event and never changes health, armor values, hit zones, inventory contents, character physics or portal placement.
- Reforger already provides `SCR_Global.SetMaterial(entity, material, recursively: true)`. I use that official material-remapping helper on the target character, including children, so clothing and worn equipment should take on the pink look along with the body. Equipped objects parented to the character may also turn pink; this is fine for a novelty gun.
- The Pinkifier uses the same working approach as my Portal Gun's client-input + server-RPC pipeline, but with distinct methods. While holding the Pinkifier, it cannot accidentally fire or replace my Blue/Orange portals.

### What I need to test

1. Pull this branch and **compile the Game script module**. This feature has not yet been compiled in Workbench.
2. Find `Prefabs/Weapons/Core/Pinkifier_Gun.et` in Resource Browser, drop it into a test world, pick it up and equip it. Its model should appear pink. The normal Portal Gun should still look and function as before.
3. Stand 5–20 meters from an AI soldier and shoot them with LMB. Check that the character's visible mesh/outfit turns hot pink without injury. Watch for `[CGS PINKIFIER] player=... painted character=... id=...` in the authority log.
4. Shoot an actual wall, vehicle or empty ground; these should not change. Verify no bullets, hitscan damage or death are produced.
5. With two players, paint another player's character. Check that **both clients** see the same hot-pink visual on that target. Paint an AI soldier while a second player watches to test non-player RplId lookup.
6. Equip my actual Portal Gun and verify LMB Blue, MMB Orange, PiP live views, teleportation and anti-reentry still work.
7. Report the first compiler errors or RPC warnings if any. I will keep the new system isolated from the existing portal scripts except for the new Pinkifier-only branch in the player-controller input bridge.

### Known prototype limitations

- The pink effect is intended to last for the lifetime of the affected character, but a respawn, model/LOD change or replacing equipped gear may restore the original material. **Late joiners do not yet receive a persistent painted-character registry**; this is an event-based first multiplayer test.
- Characters or AI without a valid replicated entity ID are rejected rather than silently coloring the server only.
- My first shader replaces the original material rather than preserving clothing camouflage and normal textures. I can add multiple colors (pink, blue, lime, gold), a duration timer or a restore mechanism after the initial firing/replication path is verified.
- The Pinkifier is a separate, Workbench-placeable prefab. Registering it in every Game Master inventory/arsenal category is a separate step if I want it distributed through those catalogs.

**Status:** Source and prefab are pushed for Workbench testing, but I have not claimed to verify compilation, actual character painting or multiplayer replication.


## Phase 2.5a — Fix Pinkifier game crash on shooting characters

I tested the first Pinkifier prototype. The Pinkifier weapon equipped and was able to shoot, but **the game crashed when the pink material was applied to the character**.

I reviewed the actual broadcast path in `CGS_PortalPlayerController.c` and found that it called `SCR_Global.SetMaterial(character, pinkMaterial, true)`. In stock Reforger this helper loops through the entity's VObject material slots and **calls `entity.SetObject(mesh, "$remap ...")`**, then recursively repeats this for all child entities. That was too aggressive for a live animated character carrying clothes, weapons, equipment and an inventory. A native crash is plausible. I cannot prove this was the only underlying cause without the crash dump.

### What I changed

- **Removed the recursive mesh/material replacement from the Pinkifier hit broadcast.** The new code NEVER calls `SCR_Global.SetMaterial` or `SetObject` on the target character hierarchy.
- Added `Scripts/Game/CGS_Portal/CGS_PinkVisuals.c`. It checks whether the character or EQUIPPED clothing has the built-in `ParametricMaterialInstanceComponent`. If available, it calls the documented `SetColor(Color.PINK)` to tint the existing material INSTANCE without replacing the 3D object/material resource, inventory, hit zones or animation skeleton.
- I intentionally limit the tint to the character and seven known equipped clothing areas: jacket, pants, vest, head cover, boots, gloves and backpack. I do NOT recurse through the thousands of possible child/inventory entities or touch dynamically added objects.
- The code logs exactly how many eligible material components it found and tinted. Custom clothing without a parametric color component is skipped, with a warning. Some outfits may not visually tint until I implement a safe explicit character material solution, but the game should no longer be exposed to mass VObject remapping.
- Server shot validation, zero-damage trace, RplId-based broadcast, Pinkifier weapon prefab, gun material, and the original Portal Gun's Blue/Orange scripts remain the same.

### My Workbench test order

1. Pull PR #27, compile Game scripts and check the Pinkifier and Portal Gun still equip.
2. Shoot ONE AI soldier with the Pinkifier while monitoring log. Expected: `[CGS PINKIFIER SAFE] Color applied to N material-instance component(s), M equipped clothing slot(s) found. No VObject remaps.` Followed by `[CGS PINKIFIER] SAFE paint applied` only when N > 0.
3. Confirm the target stays alive and the game doesn't crash. Check visually which clothing pieces turned pink. If none turn pink, record `N`, `M` and the target's outfit prefab; do not go back to recursive mesh replacement.
4. With two players, confirm that the server paint RPC is visible locally on connected clients. Late-join state is still not persisted.
5. Test the original CGS Portal Gun's LMB Blue / MMB Orange, PiP and teleportation after the Pinkifier test.

### Limitations and follow-up

This is a **source-level crash mitigation, not yet compiled or game-tested**. The original crash stack/dump was not available, so I treat recursive remapping as the strongest suspect rather than a confirmed engine root cause. Material-instance tinting may not affect every standard or modded uniform if its shader does not expose a compatible color parameter. I will use the actual Workbench result to determine whether we need a character-safe palette/overlay approach. I will not sacrifice server stability to force every piece of clothing to turn pink.
