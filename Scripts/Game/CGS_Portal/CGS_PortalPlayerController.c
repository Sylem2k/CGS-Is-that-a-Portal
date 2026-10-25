//------------------------------------------------------------------------------------------------
// CGS Portal Gun - player input bridge.
//
// Uses Reforger's existing CharacterFire and CharacterWeaponADS input actions
// ONLY when CGS_PortalGunComponent is on the currently selected weapon.
//
// LMB -> Blue, RMB -> Orange. On multiplayer clients the request is sent
// to this player's OWN replicated controller. The server validates the
// currently equipped gun and performs its own raycast and portal spawn.
//
// This first test shares RMB with regular aim/ADS while the portal gun is
// equipped. A dedicated custom input context can replace it later.
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	protected bool m_bCGSWasPrimaryHeld;
	protected bool m_bCGSWasSecondaryHeld;

	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);

		// Only handle the local human's own inputs, never other players.
		if (!GetGame() || GetGame().GetPlayerController() != this)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		bool primaryHeld = inputManager.GetActionValue("CharacterFire") > 0.5;
		bool secondaryHeld = inputManager.GetActionValue("CharacterWeaponADS") > 0.5;

		// Rising-edge input: each click places only ONE portal, not one
		// portal per frame while mouse button is held.
		bool primaryPressed = primaryHeld && !m_bCGSWasPrimaryHeld;
		bool secondaryPressed = secondaryHeld && !m_bCGSWasSecondaryHeld;

		m_bCGSWasPrimaryHeld = primaryHeld;
		m_bCGSWasSecondaryHeld = secondaryHeld;

		if ((!primaryPressed && !secondaryPressed) || GetGame().GetMenuManager().IsAnyMenuOpen())
			return;

		CGS_PortalGunComponent equippedGun = CGS_GetEquippedPortalGun();
		if (!equippedGun)
			return;

		vector cameraMatrix[4];
		GetGame().GetWorld().GetCurrentCamera(cameraMatrix);

		// Prefer primary if two buttons are pressed simultaneously.
		if (primaryPressed)
			CGS_RequestPortalShot(0, cameraMatrix[3], cameraMatrix[2]);
		else if (secondaryPressed)
			CGS_RequestPortalShot(1, cameraMatrix[3], cameraMatrix[2]);
	}

	// The gun must be actively held; it must not activate from a
	// portal gun lying on the ground or merely sitting in inventory.
	protected CGS_PortalGunComponent CGS_GetEquippedPortalGun()
	{
		ChimeraCharacter playerCharacter = ChimeraCharacter.Cast(GetControlledEntity());
		if (!playerCharacter)
			return null;

		CharacterControllerComponent characterController = playerCharacter.GetCharacterController();
		if (!characterController || characterController.IsDead())
			return null;

		BaseWeaponManagerComponent weaponManager = characterController.GetWeaponManagerComponent();
		if (!weaponManager)
			return null;

		BaseWeaponComponent weapon = weaponManager.GetCurrentWeapon();
		if (!weapon)
			return null;

		IEntity weaponEntity = weapon.GetOwner();
		if (!weaponEntity)
			return null;

		return CGS_PortalGunComponent.Cast(weaponEntity.FindComponent(CGS_PortalGunComponent));
	}

	protected void CGS_RequestPortalShot(int color, vector cameraPosition, vector cameraForward)
	{
		// In Workbench/offline listen host the server may be local.
		// Server RPCs are not guaranteed to loop back when invoked
		// through Rpc() from their own authoritative instance.
		if (Replication.IsServer())
			RpcAsk_CGSPortalGunShoot(color, cameraPosition, cameraForward);
		else
			Rpc(RpcAsk_CGSPortalGunShoot, color, cameraPosition, cameraForward);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CGSPortalGunShoot(int color, vector cameraPosition, vector cameraForward)
	{
		if (!Replication.IsServer())
			return;

		CGS_PortalGunComponent equippedGun = CGS_GetEquippedPortalGun();
		if (!equippedGun)
			return;

		equippedGun.ServerTryPlacePortal(this, color, cameraPosition, cameraForward);
	}
}
