
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

	// PINKIFIER - PHASE 3 CLOTHING-ONLY TEST.
	// The character's body and face mesh remain unchanged.
	// Apply the material individually to identified wearable meshes only.
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

		// Verify that the pink material can be loaded.
		ResourceName pinkMaterial = "{6AB2F1A000000100}Common/Models/Data/CGS_Pinkifier_Pink.emat";

		Resource loaded = Resource.Load(pinkMaterial);
		if (!loaded || !loaded.IsValid())
		{
			Print("[CGS PINKIFIER] Pink material resource is missing or invalid.", LogLevel.ERROR);
			return;
		}

		// Phase 3: do not paint character's root body. Walk its entity
		// hierarchy and individually paint recognized clothing entities.
		// Never ask SCR_Global to recursively paint attached child entities.
		int painted = CGS_PaintWearableChildren(character, pinkMaterial, 0);
		PrintFormat("[CGS PINKIFIER CLOTHING TEST] Target=%1 wearable meshes painted=%2 root body unchanged=1", character, painted);
		if (painted == 0)
			Print("[CGS PINKIFIER CLOTHING TEST] No clothing meshes matched. This character may use a different wearable hierarchy or prefab names.", LogLevel.WARNING);
	}

	// Paint only clothing-like equipment entities; never the body mesh,
	// weapons, or unrelated children. Limits protect against unexpected
	// hierarchies. This is still an experimental native material test.
	protected int CGS_PaintWearableChildren(IEntity parent, ResourceName pinkMaterial, int depth)
	{
		if (!parent || depth >= 8)
			return 0;

		int painted = 0;
		int visited = 0;
		IEntity child = parent.GetChildren();

		while (child && visited < 96)
		{
			IEntity nextChild = child.GetSibling();
			visited++;

			if (!child.IsDeleted() && !child.FindComponent(WeaponComponent))
			{
				if (CGS_IsWearableClothing(child) && child.GetVObject())
				{
					SCR_Global.SetMaterial(child, pinkMaterial, false);
					painted++;
				}

				// Walk through meshless attachment wrappers as well; they
				// may contain the actual uniform or vest mesh entity.
				painted += CGS_PaintWearableChildren(child, pinkMaterial, depth + 1);
			}

			child = nextChild;
		}

		return painted;
	}

	// Use entity and prefab naming to restrict the experiment to clothing.
	// This avoids relying on one faction's specific clothing components.
	// A zero-hit warning will tell us if other modded clothing needs support.
	protected bool CGS_IsWearableClothing(IEntity entity)
	{
		if (!entity)
			return false;

		string hint = entity.GetName();
		EntityPrefabData prefabData = entity.GetPrefabData();
		if (prefabData)
			hint = hint + " " + prefabData.GetPrefabName();

		hint.ToLower();

		return hint.IndexOf("uniform") != -1
			|| hint.IndexOf("clothing") != -1
			|| hint.IndexOf("jacket") != -1
			|| hint.IndexOf("shirt") != -1
			|| hint.IndexOf("pants") != -1
			|| hint.IndexOf("trousers") != -1
			|| hint.IndexOf("fatigue") != -1
			|| hint.IndexOf("coverall") != -1
			|| hint.IndexOf("vest") != -1
			|| hint.IndexOf("carrier") != -1
			|| hint.IndexOf("helmet") != -1
			|| hint.IndexOf("headgear") != -1
			|| hint.IndexOf("gloves") != -1
			|| hint.IndexOf("boots") != -1
			|| hint.IndexOf("balaclava") != -1;
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
