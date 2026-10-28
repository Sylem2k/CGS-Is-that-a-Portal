# CGS Portal Gun — Phase 1 physics grab test

**This is a compile-first test branch. Neither Workbench nor dedicated multiplayer has been run.**

## Goal

- Keep existing Portal Gun **LMB = Blue** and **MMB = Orange** unchanged.
- **Hold F** while pointing at an opt-in, dynamic physics prop to pull it toward the camera.
- Keep holding F to carry it roughly **2.5m** ahead of the camera.
- **Release F** to drop it. The physics object resumes regular simulation.
- **Tap V while holding F** to throw the held object at the default 15 m/s; a server-validated reliable RPC sets the rigidbody velocity.
- Other players cannot claim the same object at the same time.
- No teleporting grabbed entities, no player grab, no vehicle grab.

## Added components

- `Scripts/Game/CGS_Portal/CGS_GrabbableComponent.c`: attach to the **root** of any permitted prop.
- `Scripts/Game/CGS_Portal/CGS_GrabGunComponent.c`: already installed on the existing Portal Gun prefab.
- `Scripts/Game/CGS_Portal/CGS_GrabService.c`: server-only dynamic-physics pull.
- `Configs/System/chimeraInputCommon.conf`: adds a hold-style `CGSPortalGunGrab` action bound to `keyboard:KC_F`.

## Existing cube test object

The existing `Prefabs/Props/Core/Not_Companion_Cube.et` is already in this addon.
It uses `Common/Models/NotCompaCube.xob` and has a dynamic 20kg
RigidBody, CGS_GrabbableComponent, RplComponent, and
NwkPhysicsMovementComponent. **Do not rename the existing prefab or its
resource GUID.** Spawn or use the already placed cube in `unnamed.ent`.

For other props later, attach CGS_GrabbableComponent to the root of any
compatible **dynamic, collision-enabled replicated physics entity**.

## Tests

1. **Game-script compile:** verify that all three new classes compile, input config loads, and weapon prefab instantiates with the grab component.
2. **Offline basic:** look at the test crate from 2–6m. Hold F. Expect `[CGS GRAB] Started`, the crate to physically approach, then to follow the aim. Release F, expect `[CGS GRAB] Released`, then gravity.
3. **Throw:** hold F until the cube is in front of you, press V once while F remains held. Expect `[CGS GRAB THROW INPUT]` and `[CGS GRAB THROW]`, and the cube should move forward without player teleportation. Release F afterwards.
4. **Rejection:** aim at world terrain, characters, a vehicle, static crate, or another object without the component. No grab should occur.
5. **Collision:** face a wall while carrying. The crate should **not teleport through the wall**. It may collide, stop or release if it gets too far away.
6. **Ownership:** on a dedicated server, Player A grabs the crate. Player B tries to grab the same crate; B must fail. A releases, then B can grab.
7. **Weapon swap:** while holding F, put away the Portal Gun or disconnect. The grab should release automatically.
8. **Regression:** shoot Blue with LMB and Orange with MMB. Pinkifier and portal teleportation must still work.

## Current limitations and next phase

- F pulls/holds/drops; **V throws only the prop currently carried by F**. There is not yet a separate ranged push blast for unheld objects.
- The test object uses the existing `NotCompaCube.xob` mesh; its final art/texture pass is separate from the grab/throw mechanics.
- High-latency carry may jitter: target aim arrives at roughly 8 updates per second; server drives physics; clients observe physics replication.
- Grabbing only supports world-space, dynamic physics props. Inventory items, static map geometry, players and vehicles are explicitly unsupported.
- Enfusion platform API assumptions need validation by running Workbench; report any script errors rather than treating this branch as production-ready.
