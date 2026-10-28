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
- The test may expose engine/physics replication constraints. Treat log results and actual client visuals as the authority.
