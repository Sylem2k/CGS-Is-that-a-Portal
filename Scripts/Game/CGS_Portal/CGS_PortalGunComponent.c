//------------------------------------------------------------------------------------------------
// CGS Portal Gun - server-authoritative surface placement.
//
// The held-weapon input bridge lives in CGS_PortalPlayerController.c.
// The SERVER verifies that this exact gun is equipped, traces the shot,
// spawns the desired portal, and only then replaces that owner's old color.
// No ammo, grenades or ballistic projectiles are involved.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS Portal", description: "Shoot personal Blue/Orange portal onto a traced static surface.")]
class CGS_PortalGunComponentClass : ScriptComponentClass
{
}

class CGS_PortalGunComponent : ScriptComponent
{
	[Attribute("{E5DA100433B35A4E}Prefabs/Weapons/Core/Blue_Portal.et", UIWidgets.ResourceNamePicker, "Blue portal prefab", "et")]
	protected ResourceName m_sBluePortalPrefab;

	[Attribute("{EA7C0FE5E8E2E3C0}Prefabs/Weapons/Core/Orange_Portal.et", UIWidgets.ResourceNamePicker, "Orange portal prefab", "et")]
	protected ResourceName m_sOrangePortalPrefab;

	[Attribute("90", UIWidgets.EditBox, "Gun raycast distance (meters); separate from unlimited link distance")]
	protected float m_fShotDistance;

	[Attribute("0.04", UIWidgets.EditBox, "Portal surface offset (meters)")]
	protected float m_fSurfaceOffset;

	[Attribute("200", UIWidgets.EditBox, "Minimum delay between shots (milliseconds)")]
	protected int m_iShotCooldownMs;

	[Attribute("1", UIWidgets.CheckBox, "Log portal gun shot success and errors")]
	protected bool m_bDebug;

	protected int m_iLastShotTime = -10000;

	// Entry point may only be executed by the server controller RPC.
	bool ServerTryPlacePortal(SCR_PlayerController requester, int color, vector clientCameraOrigin, vector clientCameraDirection)
	{
		if (!Replication.IsServer() || !requester || (color != 0 && color != 1))
			return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		int shooterId = requester.GetPlayerId();
		IEntity playerEntity = playerManager.GetPlayerControlledEntity(shooterId);
		ChimeraCharacter playerCharacter = ChimeraCharacter.Cast(playerEntity);
		if (!playerCharacter || requester.GetControlledEntity() != playerEntity)
			return false;

		CharacterControllerComponent controller = playerCharacter.GetCharacterController();
		if (!controller || controller.IsDead())
			return false;

		// Security and gameplay: never fire a portal from an inventory
		// item, another player's weapon, or a dropped portal gun.
		BaseWeaponManagerComponent weaponManager = controller.GetWeaponManagerComponent();
		if (!weaponManager)
			return false;

		BaseWeaponComponent currentWeapon = weaponManager.GetCurrentWeapon();
		if (!currentWeapon || currentWeapon.GetOwner() != GetOwner())
			return false;

		int now = GetGame().GetWorld().GetWorldTime();
		if (now - m_iLastShotTime < m_iShotCooldownMs)
			return false;

		float aimLength = clientCameraDirection.Length();
		if (aimLength < 0.75 || aimLength > 1.25)
			return false;

		vector aimDirection = clientCameraDirection.Normalized();
		vector playerEye = playerEntity.GetOrigin() + Vector(0, 1.6, 0);

		// The client's camera is useful for 1P and 3P aiming. Keep
		// requests near the actual player to prevent remote raycasts.
		if (vector.DistanceSq(clientCameraOrigin, playerEye) > 25)
			return false;

		float maxRange = m_fShotDistance;
		if (maxRange < 1)
			maxRange = 1;
		if (maxRange > 250)
			maxRange = 250;

		TraceParam trace = new TraceParam();
		trace.Start = clientCameraOrigin;
		trace.End = clientCameraOrigin + aimDirection * maxRange;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		ref array<IEntity> excluded = {};
		excluded.Insert(playerEntity);
		excluded.Insert(GetOwner());
		trace.ExcludeArray = excluded;

		float hitFraction = GetGame().GetWorld().TraceMove(trace, null);
		if (hitFraction >= 1)
		{
			if (m_bDebug)
				PrintFormat("[CGS GUN] Player %1: %2 shot missed", shooterId, color);
			return false;
		}

		// Player, cars and mobile props cannot safely host portals.
		// Later, surface-size checks can make invalid placement visible.
		if (trace.TraceEnt)
		{
			Physics hitPhysics = trace.TraceEnt.GetPhysics();
			if (hitPhysics && hitPhysics.IsDynamic())
			{
				if (m_bDebug)
					Print("[CGS GUN] Shot rejected: hit entity is dynamic.");
				return false;
			}
		}

		vector hitPosition = trace.Start + (trace.End - trace.Start) * hitFraction;
		vector normal = trace.TraceNorm;
		if (normal.LengthSq() < 0.5)
		{
			if (m_bDebug)
				Print("[CGS GUN] Shot rejected: surface normal is invalid.");
			return false;
		}
		normal.Normalize();

		// Choose a stable in-plane axis. World Y is vertical in Reforger.
		// For floors/ceilings, choose Z instead to avoid degeneracy.
		vector referenceUp = Vector(0, 1, 0);
		if (Math.AbsFloat(vector.Dot(referenceUp, normal)) > 0.90)
			referenceUp = Vector(0, 0, 1);

		// In Enfusion, vector A * vector B produces the cross product.
		vector portalRight = referenceUp * normal;
		portalRight.Normalize();
		vector portalUp = normal * portalRight;
		portalUp.Normalize();

		// The existing Blue test mesh faces LOCAL -Y (axis setting 2).
		// Orange faces LOCAL +Y (axis setting 1). Build transforms so
		// their GetPortalBasis() normals both point OUT from the wall.
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = portalRight;
		if (color == 0)
		{
			params.Transform[1] = -normal;
			params.Transform[2] = portalUp;
		}
		else
		{
			params.Transform[1] = normal;
			params.Transform[2] = -portalUp;
		}

		float offset = m_fSurfaceOffset;
		if (offset < 0.01)
			offset = 0.01;
		if (offset > 0.2)
			offset = 0.2;
		params.Transform[3] = hitPosition + normal * offset;

		ResourceName portalName = m_sBluePortalPrefab;
		if (color == 1)
			portalName = m_sOrangePortalPrefab;

		Resource portalResource = Resource.Load(portalName);
		if (!portalResource || !portalResource.IsValid())
		{
			if (m_bDebug)
				PrintFormat("[CGS GUN] Missing portal resource for color=%1", color);
			return false;
		}

		// Do not erase the existing colored portal until the new one
		// is successfully created. Invalid shots preserve the old pair.
		CGS_PortalComponent previous = CGS_PortalManagerComponent.GetOwnedPortal(shooterId, color);
		IEntity entity = GetGame().SpawnEntityPrefab(portalResource, GetGame().GetWorld(), params);
		if (!entity)
		{
			if (m_bDebug)
				Print("[CGS GUN] Portal prefab failed to spawn.");
			return false;
		}

		CGS_PortalComponent spawned = CGS_PortalComponent.Cast(entity.FindComponent(CGS_PortalComponent));
		if (!spawned)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
			Print("[CGS GUN] Spawned prefab has no CGS_PortalComponent.");
			return false;
		}

		spawned.SetPortalOwnerID(shooterId);
		m_iLastShotTime = now;

		// Delete only this owner's OLD portal of the SAME color.
		if (previous && previous != spawned && previous.GetOwner())
			SCR_EntityHelper.DeleteEntityAndChildren(previous.GetOwner());

		if (m_bDebug)
			PrintFormat("[CGS GUN] Player=%1 fired color=%2 hit=%3 normal=%4 surfaceEntity=%5", shooterId, color, hitPosition, normal, trace.TraceEnt);
		return true;
	}
}
