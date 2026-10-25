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
	protected bool m_bCGSPinkWasPrimaryHeld;

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

		// SEPARATE Pinkifier weapon. It deliberately does not activate the
		// portal input context or send portal placement RPCs.
		CGS_PinkGunComponent equippedPinkGun = CGS_GetEquippedPinkGun();
		if (equippedPinkGun)
		{
			bool pinkPressed = primaryHeld && !m_bCGSPinkWasPrimaryHeld;
			m_bCGSPinkWasPrimaryHeld = primaryHeld;

			// Keep the existing portal gun's primary-fire latch in sync
			// so switching weapons while holding LMB cannot fire both.
			m_bCGSWasPrimaryHeld = primaryHeld;
			m_bCGSWasSecondaryHeld = false;
			m_bCGSInputContextReported = false;

			if (pinkPressed && !GetGame().GetMenuManager().IsAnyMenuOpen())
			{
				vector pinkCamera[4];
				GetGame().GetWorld().GetCurrentCamera(pinkCamera);
				CGS_RequestPinkShot(pinkCamera[3], pinkCamera[2]);
			}
			return;
		}
		m_bCGSPinkWasPrimaryHeld = primaryHeld;

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

	//------------------------------------------------------------------------------------------------
	// Pinkifier only. This is a different gun prefab with its own
	// component and is never mistaken for the CGS Portal Gun.
	protected CGS_PinkGunComponent CGS_GetEquippedPinkGun()
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
		if (!weapon || !weapon.GetOwner())
			return null;

		return CGS_PinkGunComponent.Cast(weapon.GetOwner().FindComponent(CGS_PinkGunComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected void CGS_RequestPinkShot(vector cameraPosition, vector cameraForward)
	{
		// As with the verified portal gun, a listen-host controller
		// must invoke its own server handler directly.
		if (Replication.IsServer())
			RpcAsk_CGSPinkShot(cameraPosition, cameraForward);
		else
			Rpc(RpcAsk_CGSPinkShot, cameraPosition, cameraForward);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CGSPinkShot(vector cameraPosition, vector cameraForward)
	{
		if (!Replication.IsServer())
			return;

		CGS_PinkGunComponent pinkGun = CGS_GetEquippedPinkGun();
		if (!pinkGun)
			return;

		pinkGun.ServerTryPinkify(this, cameraPosition, cameraForward);
	}

	//------------------------------------------------------------------------------------------------
	// Called only AFTER the server validated a Pinkifier shot.
	// Broadcast the RplId to all replicas of this controller.
	void CGS_PinkBroadcastTarget(RplId characterID)
	{
		if (!Replication.IsServer() || !characterID.IsValid())
			return;

		Rpc(RpcDo_CGSPinkTarget, characterID);
		RpcDo_CGSPinkTarget(characterID);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_CGSPinkTarget(RplId characterID)
	{
		if (!characterID.IsValid())
			return;

		IEntity characterEntity = SCR_EntityHelper.RplIdToEntity(characterID);
		ChimeraCharacter character = ChimeraCharacter.Cast(characterEntity);
		if (!character || character.IsDeleted())
		{
			Print("[CGS PINKIFIER] Target entity is not available on this client yet.", LogLevel.WARNING);
			return;
		}

		ResourceName pinkMaterial = "{6AB2F1A000000100}Common/Models/Data/CGS_Pinkifier_Pink.emat";
		Resource loaded = Resource.Load(pinkMaterial);
		if (!loaded || !loaded.IsValid())
		{
			Print("[CGS PINKIFIER] Missing target pink material resource.", LogLevel.ERROR);
			return;
		}

		// The native Reforger SCR_Global helper recursively remaps mesh
		// material slots on a character and worn/equipped child entities.
		// It changes APPEARANCE only, not health, inventory or animations.
		SCR_Global.SetMaterial(character, pinkMaterial, true);
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
