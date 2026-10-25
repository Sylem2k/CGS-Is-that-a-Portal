//------------------------------------------------------------------------------------------------
// CGS Portal Gun - independent primary and secondary input bridge.
//
// LEFT MOUSE -> Blue: keep Reforger CharacterFire, confirmed working.
// MIDDLE MOUSE -> Orange: separate CGSPortalGunOrange action.
//   Configs/System/chimeraInputCommon.conf binds mouse:button2.
//   Right mouse remains normal ADS.
//   CGSPortalGunContext is activated only while the portal gun is held.
//
// Uses one click -> one server-authoritative portal placement. The server
// validates the gun, color, aim ray and surface before replacing a portal.
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	protected bool m_bCGSWasPrimaryHeld;
	protected bool m_bCGSWasSecondaryHeld;
	protected bool m_bCGSInputContextReported;

	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);

		// Only the local human's controller reads this machine's mouse.
		if (!GetGame() || GetGame().GetPlayerController() != this)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		// IMPORTANT: Blue is unchanged from the earlier working build.
		bool primaryHeld = inputManager.GetActionValue("CharacterFire") > 0.5;

		CGS_PortalGunComponent equippedGun = CGS_GetEquippedPortalGun();
		if (!equippedGun || GetGame().GetMenuManager().IsAnyMenuOpen())
		{
			// Don't activate the custom secondary context for ordinary guns.
			// Reset latch when not using the Portal Gun.
			m_bCGSWasPrimaryHeld = primaryHeld;
			m_bCGSWasSecondaryHeld = false;
			m_bCGSInputContextReported = false;
			return;
		}

		// Custom Orange uses middle mouse. Vanilla ADS stays on RMB.
		// Re-activate every update while this gun is equipped because
		// input contexts are frame-lived in Enfusion.
		bool activated = inputManager.ActivateContext("CGSPortalGunContext");

		if (!m_bCGSInputContextReported)
		{
			bool actionFound;
			for (int i = 0; i < inputManager.GetActionCount(); i++)
			{
				if (inputManager.GetActionName(i) == "CGSPortalGunOrange")
				{
					actionFound = true;
					break;
				}
			}

			PrintFormat("[CGS GUN INPUT] Orange action registered=%1 contextActivated=%2", actionFound, activated);
			m_bCGSInputContextReported = true;
		}

		// Orange reads the dedicated middle-click action; vanilla ADS is not used.
		bool secondaryHeld = inputManager.GetActionValue("CGSPortalGunOrange") > 0.5;

		// Rising edge prevents placing portals each frame while held.
		bool primaryPressed = primaryHeld && !m_bCGSWasPrimaryHeld;
		bool secondaryPressed = secondaryHeld && !m_bCGSWasSecondaryHeld;

		m_bCGSWasPrimaryHeld = primaryHeld;
		m_bCGSWasSecondaryHeld = secondaryHeld;

		if (!primaryPressed && !secondaryPressed)
			return;

		vector cameraMatrix[4];
		GetGame().GetWorld().GetCurrentCamera(cameraMatrix);

		// Favor Blue only when both buttons were pressed this frame.
		if (primaryPressed)
			CGS_RequestPortalShot(0, cameraMatrix[3], cameraMatrix[2]);
		else
			CGS_RequestPortalShot(1, cameraMatrix[3], cameraMatrix[2]);
	}

	// The weapon must be actively equipped, not merely in inventory.
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
		PrintFormat("[CGS GUN INPUT] Player=%1 requested color=%2", GetPlayerId(), color);

		// An authority controller does not route its own server RPC
		// back to itself in every mode. Execute directly in SP/host.
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
		{
			PrintFormat("[CGS GUN RPC] Server rejected player=%1 color=%2: portal gun not actively equipped", GetPlayerId(), color);
			return;
		}

		PrintFormat("[CGS GUN RPC] Server received player=%1 color=%2", GetPlayerId(), color);
		equippedGun.ServerTryPlacePortal(this, color, cameraPosition, cameraForward);
	}
}
