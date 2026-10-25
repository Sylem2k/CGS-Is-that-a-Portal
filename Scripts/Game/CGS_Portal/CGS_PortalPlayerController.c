
modded class SCR_PlayerController
{
	protected bool m_bCGSWasPrimaryHeld;
	protected bool m_bCGSWasSecondaryHeld;
	protected bool m_bCGSInputContextReported;
	protected bool m_bCGSPinkWasPrimaryHeld;

	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);

		if (!GetGame() || GetGame().GetPlayerController() != this)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		bool primaryHeld = inputManager.GetActionValue("CharacterFire") > 0.5;

		// PINKIFIER - independent from the Portal Gun.
		CGS_PinkGunComponent equippedPinkGun = CGS_GetEquippedPinkGun();
		if (equippedPinkGun)
		{
			bool pinkPressed = primaryHeld && !m_bCGSPinkWasPrimaryHeld;
			m_bCGSPinkWasPrimaryHeld = primaryHeld;

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

		// PORTAL GUN - original input handling.
		CGS_PortalGunComponent equippedGun = CGS_GetEquippedPortalGun();
		if (!equippedGun || GetGame().GetMenuManager().IsAnyMenuOpen())
		{
			m_bCGSWasPrimaryHeld = primaryHeld;
			m_bCGSWasSecondaryHeld = false;
			m_bCGSInputContextReported = false;
			return;
		}

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

		bool secondaryHeld = inputManager.GetActionValue("CGSPortalGunOrange") > 0.5;

		bool primaryPressed = primaryHeld && !m_bCGSWasPrimaryHeld;
		bool secondaryPressed = secondaryHeld && !m_bCGSWasSecondaryHeld;

		m_bCGSWasPrimaryHeld = primaryHeld;
		m_bCGSWasSecondaryHeld = secondaryHeld;

		if (!primaryPressed && !secondaryPressed)
			return;

		vector cameraMatrix[4];
		GetGame().GetWorld().GetCurrentCamera(cameraMatrix);

		if (primaryPressed)
			CGS_RequestPortalShot(0, cameraMatrix[3], cameraMatrix[2]);
		else
			CGS_RequestPortalShot(1, cameraMatrix[3], cameraMatrix[2]);
	}

	// PINKIFIER - weapon identification.
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

	// PINKIFIER - request a server-authoritative shot.
	protected void CGS_RequestPinkShot(vector cameraPosition, vector cameraForward)
	{
		if (Replication.IsServer())
			RpcAsk_CGSPinkShot(cameraPosition, cameraForward);
		else
			Rpc(RpcAsk_CGSPinkShot, cameraPosition, cameraForward);
	}

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

	// PINKIFIER - validated server broadcast.
	void CGS_PinkBroadcastTarget(RplId characterID)
	{
		if (!Replication.IsServer() || !characterID.IsValid())
			return;

		Rpc(RpcDo_CGSPinkTarget, characterID);
		RpcDo_CGSPinkTarget(characterID);
	}

	// PINKIFIER - SKIN/BODY ONLY, HEAD ATTACHMENT SAFETY.
	// Previous Workbench tests showed that changing a live head mesh
	// detached its rendering from the original skeletal attachment.
	// Apply only to the root character mesh, never to child head,
	// helmet, uniform, equipment, or weapon entities.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_CGSPinkTarget(RplId characterID)
	{
		if (!characterID.IsValid())
			return;

		IEntity characterEntity = SCR_EntityHelper.RplIdToEntity(characterID);
		ChimeraCharacter character = ChimeraCharacter.Cast(characterEntity);
		if (!character || character.IsDeleted())
		{
			Print("[CGS PINKIFIER] Target unavailable on this machine.", LogLevel.WARNING);
			return;
		}

		ResourceName pinkMaterial = "{6AB2F1A000000100}Common/Models/Data/CGS_Pinkifier_Pink.emat";
		Resource loaded = Resource.Load(pinkMaterial);
		if (!loaded || !loaded.IsValid())
		{
			Print("[CGS PINKIFIER] Pink material resource is missing or invalid.", LogLevel.ERROR);
			return;
		}

		if (!character.GetVObject())
		{
			Print("[CGS PINKIFIER] Target has no root body mesh.", LogLevel.WARNING);
			return;
		}

		// Previously observed to tint exposed root-body skin without
		// the headgear detachment / T-pose seen on attached child meshes.
		// No recursive remap and no attached-entity SetMaterial calls.
		SCR_Global.SetMaterial(character, pinkMaterial, false);
		PrintFormat("[CGS PINKIFIER BODY-ONLY] Target=%1 bodyPainted=1 headPainted=0 equipmentPainted=0", character);
	}

	// PORTAL GUN - original weapon identification.
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

	// PORTAL GUN - original request handling.
	protected void CGS_RequestPortalShot(int color, vector cameraPosition, vector cameraForward)
	{
		PrintFormat("[CGS GUN INPUT] Player=%1 requested color=%2", GetPlayerId(), color);

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
