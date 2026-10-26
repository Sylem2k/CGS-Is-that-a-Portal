
modded class SCR_PlayerController
{
	protected bool m_bCGSWasPrimaryHeld;
	protected bool m_bCGSWasSecondaryHeld;
	protected bool m_bCGSInputContextReported;
	protected bool m_bCGSPinkWasPrimaryHeld;

	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);

		// Maintain momentum on the owning client, where Reforger
		// predicts its character controller. The server still tracks
		// the effective velocity for chaining future portal entries.
		CGS_PortalMomentumService.UpdateOwner();

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

	// PINKIFIER - PINK SKIN HEAD TEST (ASIAN_02 AND WHITE_04).
	// Both heads have prefab-assigned pink materials; no live head remaps.
	// Unsupported heads remain unchanged instead of turning camouflage.
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
			CGS_TrySetPinkIdentityHead(character);

		// Preserve the already-tested non-recursive root body skin tint.
		SCR_Global.SetMaterial(character, pinkMaterial, false);
		PrintFormat("[CGS PINKIFIER PINK HEAD TEST] Target=%1 bodyPink=1 liveHeadMaterialChanges=0", character);
	}

	// Use Reforger's identity system to instantiate an appearance prefab
	// with skin materials assigned BEFORE the head is attached.
	// Never call SCR_Global.SetMaterial() on the live head entity.
	protected void CGS_TrySetPinkIdentityHead(ChimeraCharacter character)
	{
		SCR_CharacterIdentityComponent identityComponent = SCR_CharacterIdentityComponent.Cast(character.FindComponent(SCR_CharacterIdentityComponent));
		if (!identityComponent || !identityComponent.GetIdentity())
		{
			Print("[CGS PINKIFIER PINK HEAD TEST] No character identity component.", LogLevel.WARNING);
			return;
		}

		VisualIdentity visualIdentity = identityComponent.GetIdentity().GetVisualIdentity();
		if (!visualIdentity)
		{
			Print("[CGS PINKIFIER PINK HEAD TEST] No visual identity.", LogLevel.WARNING);
			return;
		}

		ResourceName currentHead = visualIdentity.GetHead();
		if (!currentHead)
		{
			Print("[CGS PINKIFIER PINK HEAD TEST] Current head is empty.", LogLevel.WARNING);
			return;
		}

		// Shots after successful pinkifying are idempotent.
		if (currentHead.Contains("CGS_Head_") && currentHead.Contains("_Pink.et"))
		{
			PrintFormat("[CGS PINKIFIER PINK HEAD TEST] Already pink: %1", currentHead);
			return;
		}

		// Supports vanilla and camouflaged versions of supported heads.
		ResourceName newHead = CGS_GetPinkHeadPrefab(currentHead);
		if (!newHead)
		{
			// Do not substitute vanilla camouflage: it obscures the
			// pink-head result and confused the previous Workbench test.
			PrintFormat("[CGS PINKIFIER PINK HEAD TEST] Unmapped head, unchanged: %1", currentHead);
			return;
		}

		visualIdentity.SetHead(newHead);
		identityComponent.CommitChanges();
		identityComponent.SetIdentity(identityComponent.GetIdentity());
		PrintFormat("[CGS PINKIFIER PINK HEAD TEST] Identity swapped old=%1 new=%2", currentHead, newHead);
	}

	// Automatically select a matching pink prefab for all 10 known
	// vanilla head types. Spawned US soldiers can have randomized heads.
	// Unrecognized modded head types remain unchanged and log a warning.
	protected ResourceName CGS_GetPinkHeadPrefab(ResourceName currentHead)
	{
		if (currentHead.Contains("Head_Asian_02"))
			return "{6AB2F1A000000430}Prefabs/Characters/Heads/CGS_Head_Asian_02_Pink.et";

		if (currentHead.Contains("Head_Black_02"))
			return "{6AB2F1A000000440}Prefabs/Characters/Heads/CGS_Head_Black_02_Pink.et";

		if (currentHead.Contains("Head_White_02"))
			return "{6AB2F1A000000450}Prefabs/Characters/Heads/CGS_Head_White_02_Pink.et";

		if (currentHead.Contains("Head_White_03"))
			return "{6AB2F1A000000460}Prefabs/Characters/Heads/CGS_Head_White_03_Pink.et";

		if (currentHead.Contains("Head_White_04"))
			return "{6AB2F1A000000420}Prefabs/Characters/Heads/CGS_Head_White_04_Pink.et";

		if (currentHead.Contains("Head_White_05"))
			return "{6AB2F1A000000470}Prefabs/Characters/Heads/CGS_Head_White_05_Pink.et";

		if (currentHead.Contains("Head_White_06"))
			return "{6AB2F1A000000480}Prefabs/Characters/Heads/CGS_Head_White_06_Pink.et";

		if (currentHead.Contains("Head_White_08"))
			return "{6AB2F1A000000490}Prefabs/Characters/Heads/CGS_Head_White_08_Pink.et";

		if (currentHead.Contains("Head_White_09"))
			return "{6AB2F1A0000004A0}Prefabs/Characters/Heads/CGS_Head_White_09_Pink.et";

		if (currentHead.Contains("Head_White_10"))
			return "{6AB2F1A0000004B0}Prefabs/Characters/Heads/CGS_Head_White_10_Pink.et";

		return "";
	}

	// PORTAL MP: Reforger characters are client-predicted.
	// One reliable Owner RPC handles BOTH initial teleport and local
	// virtual momentum, in the correct order, from a server-approved shot.
	void CGS_ServerSendPortalTeleportToOwner(vector exitPosition, vector carryVelocity, float carrySeconds, float gravity, float maxStep, bool simulateMomentum)
	{
		if (!Replication.IsServer() || !Replication.IsRunning())
			return;

		Rpc(RpcDo_CGSPortalTeleportOwner, exitPosition, carryVelocity, carrySeconds, gravity, maxStep, simulateMomentum);
		PrintFormat("[CGS PORTAL MP OWNER] Server sent owner=%1 exit=%2 momentum=%3 velocity=%4",
			GetPlayerId(), exitPosition, simulateMomentum, carryVelocity);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_CGSPortalTeleportOwner(vector exitPosition, vector carryVelocity, float carrySeconds, float gravity, float maxStep, bool simulateMomentum)
	{
		// A listen-server host uses the existing server simulation path.
		if (Replication.IsServer() || !GetGame() || GetGame().GetPlayerController() != this)
			return;

		// Always cancel a prior flight when another portal is traversed.
		IEntity character = GetControlledEntity();
		if (character)
			CGS_PortalMomentumService.Stop(character);

		// This helper is already confirmed to fix owner snapback in MP.
		bool accepted = SCR_Global.TeleportLocalPlayer(exitPosition);
		character = GetControlledEntity();

		PrintFormat("[CGS PORTAL MP OWNER] Client player=%1 accepted=%2 exit=%3 momentum=%4 velocity=%5",
			SCR_PlayerController.GetLocalPlayerId(), accepted, exitPosition, simulateMomentum, carryVelocity);

		if (accepted && character && simulateMomentum)
		{
			CGS_PortalMomentumService.StartOwner(character, carryVelocity, carrySeconds, gravity, maxStep, true);
		}
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
