# CGS Portal: grabbable cube travels through paired portals

**Test branch only. This has not been compiled or played in Workbench or tested on a dedicated multiplayer server.**

## Implemented

- The **existing** `Not_Companion_Cube.et` now enables `CGS_GrabbableComponent.m_bAllowPortalTravel`. No model, texture, physics, prefab filename or resource GUID changes.
- Any **dynamic** object with `CGS_GrabbableComponent` can opt in using `Allow this dynamic prop to travel through paired portals`.
- Props use their own server-side crossing detection. This is **not** the player movement or old virtual-momentum system.
- The prop must be **released** (not still grabbed), traveling inward toward the **front** of a valid portal, and inside the actual opening rectangle.
- Its paired Blue/Orange portal must exist and be enabled.
- The server checks the destination exit ray before moving the prop. It places the prop in front of the exit (~2.5m), reorients the *linear velocity* into the exit direction and keeps speed **at most 25 m/s by default**. No boost or stacking.
- The `NwkPhysicsMovementComponent` on the existing cube is responsible for replicating server-driven object motion.
- A 650ms transfer cooldown plus destination-zone protection prevent an immediate portal ping-pong. The cube must leave the exit zone before it can re-enter that **same** portal.
- All portal/prop collision geometry remains solid. This is a *pre-collision catch* near the entry surface, **not** a hole cut into the road or wall mesh.

## Workbench test

1. Pull branch, rescan files, compile Game scripts. Spawn `Prefabs/Props/Core/Not_Companion_Cube.et`; verify the original resource GUID `{E4CC81B24CC91AB3}` remains intact.

### Reproduction from the Oct 28 Workbench log

- The user's test was **offline**, one player, with **both portals on the ROAD facing UP**, not wall portals. The cube was correctly grabbed with F many times but no `[CGS GRAB THROW INPUT] V pressed` or `[CGS GRAB THROW]` appeared anywhere in that log. **Holding F then releasing it only drops the cube**; to launch it, keep holding F and tap V.
- `[CGS PORTAL PROP BLOCKED] source=1 exit=0 blocker=SCR_ChimeraCharacter` proves the prop transfer detector was reached, but the requested destination was occupied by the player.
- This fix accepts **sideways rolling/sliding across the footprint of an upward-facing FLOOR portal** as well as physical inward/downward crossing, provided the cube was **previously outside** and has nonzero travel speed. It doesn't teleport stationary objects just because they are near a portal.
- If a character occupies the center exit lane, the server now checks **four off-center lanes inside the actual opening**. It still rejects all obstructed lanes, rather than phasing through the player or map geometry.
- For a rolling floor entry, the cube exits along the destination's outward normal with a small minimum physical speed (3.5 m/s, capped by the cube's current 25m/s maximum). Players retain normal momentum-free teleportation.

### Focused test of the user's actual setup

1. Create **Blue UP on road, Orange UP on another stretch of road** at least 6m apart. Stand to the side, *not above the other portal*.
2. Hold **F** to pull the existing cube. While still holding F, aim across the floor and tap **V** so the cube moves **into the footprint** of Blue. (F release alone is only a drop.)
3. Check `[CGS GRAB THROW INPUT] V pressed`, `[CGS GRAB THROW]` and `[CGS PORTAL PROP] ... floorSlide=1`. Cube should reappear above Orange and move out/up without a player-yeet effect.
4. Reverse Blue/Orange. After floor-to-floor works, test wall-to-wall and floor-to-wall.
5. If `[CGS PORTAL PROP BLOCKED]` persists, clear the destination (including other players and props) and try again.

2. Equip the Portal Gun and create **Blue on a wall, Orange on a separate wall** with a clear open area in front of each. For the first test use walls > 5m apart and leave 3m of free space at the exit.
3. Look at the cube, **hold F** to grab and pull it toward the gun.
4. Aim into the **center** of Blue and **tap V while continuing to hold F**. The cube must be thrown at Blue and appear in front of Orange, moving out of the Orange opening.
5. Expect `[CGS GRAB THROW]` then `[CGS PORTAL PROP]`. Check that the cube doesn't snap back, disappear or re-enter Orange immediately.
6. Repeat Orange to Blue. Try **Orange on the floor, Blue on a wall**, then drop/throw the cube downward toward Orange.
7. Check that holding the cube *inside* a portal does **not** teleport it until it is released/thrown.
8. If the log shows `[CGS PORTAL PROP BLOCKED]`, the exit ray found an obstacle. Move the destination portal somewhere with 3m clear space and retry.
9. In a **dedicated multiplayer test with two clients**, Player A throws the cube; Player B must see it emerge from the linked portal at the same position and direction.
10. Confirm **players still teleport without simulated momentum**; F/V controls, Pinkifier, Blue/Orange firing, and radio G continue working.

## Debug messages

- `[CGS GRAB THROW]` — server launched the held cube with native rigidbody physics.
- `[CGS PORTAL PROP]` — server detected an intentional crossing and teleported the dynamic prop to its partner.
- `[CGS PORTAL PROP BLOCKED]` — destination clearance check rejected this pass; logs are throttled to 1/second per prop.

## Known test limitations

- This first version recenters the cube at the exit; it does not preserve the object's precise lateral entry offset or rotate the *cube model* relative to the destination opening. It **does** rotate linear velocity.
- Entry uses a slightly smaller portal rectangle than the player to keep the cube away from the frame edges. Large props may be rejected by design.
- Exit clearance is currently checked by a **ray**, not a full cube-shaped sweep; real-world collision/overlap safety must be confirmed in Workbench.
- Momentum *for players* remains disabled. Props use ordinary rigidbody throw speed without acceleration.
- A cube that remains inside its exit capture zone cannot immediately re-enter that same portal; move it away and approach again.
- Exit alternate lanes use line traces, not a full rigidbody collision hull sweep. Test the cube near players and portal edges; do not merge until clipping is ruled out.
- The test may expose engine/physics replication constraints. Treat log results and actual client visuals as the authority.
