
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

	// PINKIFIER - TEST IDENTITY-MANAGED PINK HEAD VARIANT.
	// White_04 receives an inherited, prefab-defined PINK head.
	// Other known head models remain vanilla camo as a control group.
	// Never calls SetMaterial on a head or attached equipment.
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

		// Only the authoritative server changes VisualIdentity. This
		// experiment follows existing Reforger facepaint implementations.
		// It never remaps the live head mesh.
		if (Replication.IsServer())
			CGS_TrySetVanillaCamoHead(character);

		// Preserve the already-tested non-recursive root body skin tint.
		SCR_Global.SetMaterial(character, pinkMaterial, false);
		PrintFormat("[CGS PINKIFIER IDENTITY TEST] Target=%1 bodyPink=1 attachedMeshRemaps=0", character);
	}

	// Identity test: replace the assigned head prefab through the
	// supported identity component, NOT by replacing a VObject.
	// Head camouflage is deliberately used as a compatibility test.
	protected void CGS_TrySetVanillaCamoHead(ChimeraCharacter character)
	{
		SCR_CharacterIdentityComponent identityComponent = SCR_CharacterIdentityComponent.Cast(character.FindComponent(SCR_CharacterIdentityComponent));
		if (!identityComponent || !identityComponent.GetIdentity())
		{
			Print("[CGS PINKIFIER IDENTITY TEST] Character has no usable identity component.", LogLevel.WARNING);
			return;
		}

		VisualIdentity visualIdentity = identityComponent.GetIdentity().GetVisualIdentity();
		if (!visualIdentity)
		{
			Print("[CGS PINKIFIER IDENTITY TEST] Character has no visual identity.", LogLevel.WARNING);
			return;
		}

		ResourceName currentHead = visualIdentity.GetHead();
		if (!currentHead)
		{
			Print("[CGS PINKIFIER IDENTITY TEST] Current head prefab is empty.", LogLevel.WARNING);
			return;
		}

		// Avoid repeated swap on subsequent shots.
		if (currentHead.Contains("_camo_") || currentHead.Contains("CGS_Head_White_04_Pink"))
		{
			PrintFormat("[CGS PINKIFIER IDENTITY TEST] Existing camo head already assigned=%1", currentHead);
			return;
		}

		ResourceName newHead = CGS_GetVanillaTestCamo(currentHead);
		if (!newHead)
		{
			PrintFormat("[CGS PINKIFIER IDENTITY TEST] No vanilla camo mapping for head=%1", currentHead);
			return;
		}

		visualIdentity.SetHead(newHead);
		identityComponent.CommitChanges();
		identityComponent.SetIdentity(identityComponent.GetIdentity());
		PrintFormat("[CGS PINKIFIER IDENTITY TEST] Head changed through VisualIdentity: old=%1 new=%2", currentHead, newHead);
	}

	// White_04 uses a CGS prefab with pink material assignments.
	// All other head types retain their known-good vanilla camo tests.
	protected ResourceName CGS_GetVanillaTestCamo(ResourceName currentHead)
	{
		if (currentHead.Contains("Head_Asian_02"))
			return "{FF94E9F1DAB8496C}Prefabs/Characters/Heads/Head_Asian_02/Head_Asian_02_camo_US_01.et";

		if (currentHead.Contains("Head_Black_02"))
			return "{33850F6F8AC1FC61}Prefabs/Characters/Heads/Head_Black_02/Head_Black_02_camo_US_01.et";

		if (currentHead.Contains("Head_White_02"))
			return "{43AA5BDFE4338F57}Prefabs/Characters/Heads/Head_White_02/Head_White_02_camo_US_01.et";

		if (currentHead.Contains("Head_White_03"))
			return "{51636CA07EC51127}Prefabs/Characters/Heads/Head_White_03/Head_White_03_camo_US_01.et";

		if (currentHead.Contains("Head_White_04"))
			return "{6AB2F1A000000420}Prefabs/Characters/Heads/CGS_Head_White_04_Pink.et";

		if (currentHead.Contains("Head_White_05"))
			return "{36B119F39289239E}Prefabs/Characters/Heads/Head_White_05/Head_White_05_camo_US_01.et";

		if (currentHead.Contains("Head_White_06"))
			return "{A42053AFB05A218B}Prefabs/Characters/Heads/Head_White_06/Head_White_06_camo_US_01.et";

		if (currentHead.Contains("Head_White_08"))
			return "{889ACA9FABA0471F}Prefabs/Characters/Heads/Head_White_08/Head_White_08_camo_US_01.et";

		if (currentHead.Contains("Head_White_09"))
			return "{F915F3544A1146EC}Prefabs/Characters/Heads/Head_White_09/Head_White_09_camo_US_01.et";

		if (currentHead.Contains("Head_White_10"))
			return "{7E7987E31FB379BF}Prefabs/Characters/Heads/Head_White_10/Head_White_10_camo_US_01.et";

		return "";
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
