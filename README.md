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
