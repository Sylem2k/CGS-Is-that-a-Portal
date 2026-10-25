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
