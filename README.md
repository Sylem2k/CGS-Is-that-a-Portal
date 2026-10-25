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
