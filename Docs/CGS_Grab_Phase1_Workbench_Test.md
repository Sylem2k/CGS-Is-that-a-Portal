# CGS Portal Gun — Phase 1 physics grab test

**This is a compile-first test branch. Neither Workbench nor dedicated multiplayer has been run.**

## Goal

- Keep existing Portal Gun **LMB = Blue** and **MMB = Orange** unchanged.
- **Hold G** while pointing at an opt-in, dynamic physics prop to pull it toward the camera.
- Keep holding G to carry it roughly **2.5m** ahead of the camera.
- **Release G** to drop it. The physics object resumes regular simulation.
- Other players cannot claim the same object at the same time.
- No teleporting grabbed entities, no player grab, no vehicle grab, no throw yet.

## Added components

- `Scripts/Game/CGS_Portal/CGS_GrabbableComponent.c`: attach to the **root** of any permitted prop.
- `Scripts/Game/CGS_Portal/CGS_GrabGunComponent.c`: already installed on the existing Portal Gun prefab.
- `Scripts/Game/CGS_Portal/CGS_GrabService.c`: server-only dynamic-physics pull.
- `Configs/System/chimeraInputCommon.conf`: adds a hold-style `CGSPortalGunGrab` action bound to `keyboard:KC_G`.

## Make a temporary test object

Until the custom CGS Relay Core model is imported:

1. In Workbench, **duplicate** a small, movable, collision-enabled vanilla crate/prop into this addon. Do NOT modify the base-game prefab.
2. Make sure its root entity has a **dynamic RigidBody/Physics** body that supports `Physics.IsDynamic() == true`. A static decor mesh with no physics will be rejected by design.
3. For multiplayer, make sure the prop has the appropriate **RplComponent** and **NwkPhysicsMovementComponent** configured for server-owned simulated motion. This is required for all players to see the same carry.
4. Add **CGS_GrabbableComponent** to the prop's root entity, keep **Allow Grab = true**, mass limit **100 kg**, and hold distance **2.5m**. Save the prop as a new addon-local prefab.
5. Spawn that prefab in the test world and equip `Prefabs/Weapons/Core/Portal_Gun_.et`.

## Tests

1. **Game-script compile:** verify that all three new classes compile, input config loads, and weapon prefab instantiates with the grab component.
2. **Offline basic:** look at the test crate from 2–6m. Hold G. Expect `[CGS GRAB] Started`, the crate to physically approach, then to follow the aim. Release G, expect `[CGS GRAB] Released`, then gravity.
3. **Rejection:** aim at world terrain, characters, a vehicle, static crate, or another object without the component. No grab should occur.
4. **Collision:** face a wall while carrying. The crate should **not teleport through the wall**. It may collide, stop or release if it gets too far away.
5. **Ownership:** on a dedicated server, Player A grabs the crate. Player B tries to grab the same crate; B must fail. A releases, then B can grab.
6. **Weapon swap:** while holding G, put away the Portal Gun or disconnect. The grab should release automatically.
7. **Regression:** shoot Blue with LMB and Orange with MMB. Pinkifier and portal teleportation must still work.

## Current limitations and next phase

- The first version pulls/holds/drops only; **push/throw is not implemented yet**.
- No custom Relay Core cube mesh is included in this phase; once physics is verified, import an original crate/cube model and add this **same component** to it.
- High-latency carry may jitter: target aim arrives at roughly 8 updates per second; server drives physics; clients observe physics replication.
- Grabbing only supports world-space, dynamic physics props. Inventory items, static map geometry, players and vehicles are explicitly unsupported.
- Enfusion platform API assumptions need validation by running Workbench; report any script errors rather than treating this branch as production-ready.
