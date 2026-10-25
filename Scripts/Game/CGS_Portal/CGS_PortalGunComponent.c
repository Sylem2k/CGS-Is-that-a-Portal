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
			// Character controllers often have NON-dynamic physics.
			// Dynamic-physics checks alone would incorrectly allow portals
			// on players. Explicitly refuse characters, vehicles and
			// existing portals as unsupported host surfaces.
			if (ChimeraCharacter.Cast(trace.TraceEnt) || Vehicle.Cast(trace.TraceEnt) || trace.TraceEnt.FindComponent(CGS_PortalComponent))
			{
				if (m_bDebug)
					Print("[CGS GUN] Shot rejected: characters, vehicles and portals are not valid surfaces.");
				return false;
			}
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

		// The opening must face the PLAYER side of the surface.
		// Some two-sided wall meshes report their fixed polygon normal even
		// when hit from behind. Correct it to oppose the incoming shot.
		if (vector.Dot(normal, aimDirection) > 0)
			normal = -normal;

		// AUTO SURFACE ORIENTATION:
		//   Wall    -> portal stands vertically, top aligned to world up.
		//   Floor   -> portal lies flat and faces up.
		//   Ceiling -> portal lies flat upside down and faces down.
		//   Slope   -> follows the actual hit normal without snapping.
		//
		// Reforger Y is world vertical; these portal meshes are in LOCAL XZ,
		// unlike ordinary models whose frontal axis is often local Z.
		float verticalFacing = Math.AbsFloat(normal[1]);
		string surfaceType = "WALL";
		if (normal[1] >= 0.85)
			surfaceType = "FLOOR";
		else if (normal[1] <= -0.85)
			surfaceType = "CEILING";
		else if (verticalFacing >= 0.4)
			surfaceType = "SLOPE";

		vector portalUp;
		if (verticalFacing >= 0.85)
		{
			// World-up is parallel to the surface normal on floors and
			// ceilings, so it CANNOT define an in-plane "top" axis.
			// Instead use the player's sight direction projected along
			// that surface. The oval follows the direction of the shot.
			portalUp = aimDirection - normal * vector.Dot(aimDirection, normal);
			if (portalUp.LengthSq() < 0.0025)
			{
				// Looking exactly vertically? Pick a stable horizontal top.
				vector worldForward = Vector(0, 0, 1);
				portalUp = worldForward - normal * vector.Dot(worldForward, normal);
			}
		}
		else
		{
			// Walls/slopes should always be upright relative to gravity.
			vector worldUp = Vector(0, 1, 0);
			portalUp = worldUp - normal * vector.Dot(worldUp, normal);
		}

		// Orthonormal right/up/normal frame. Unlike Euler angles this
		// does not introduce sudden 90/180-degree flips on vertical hits.
		if (portalUp.LengthSq() < 0.0025)
		{
			if (m_bDebug)
				Print("[CGS GUN ALIGN] Shot rejected: could not build surface frame.");
			return false;
		}
		portalUp.Normalize();

		// Enfusion vector * vector is a CROSS product.
		// right x up = normal (right-handed), important for replication
		// and character transfer portal-local coordinates.
		vector portalRight = portalUp * normal;
		portalRight.Normalize();
		portalUp = normal * portalRight;
		portalUp.Normalize();

		// Blueprint model faces were tested with these separate axes:
		// Blue:   LOCAL -Y is its opening face (m_iFacingAxis=2)
		// Orange: LOCAL +Y is its opening face (m_iFacingAxis=1)
		// Both produce the SAME outward portal basis (right/up/normal).
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

		// Compare the actual spawned portal face against the raycast
		// normal. This exposes model/prefab facing-axis mistakes in
		// Workbench without altering the working teleport code.
		if (m_bDebug)
		{
			vector actualRight;
			vector actualUp;
			vector actualNormal;
			spawned.GetPortalBasis(actualRight, actualUp, actualNormal);
			float facingDot = vector.Dot(actualNormal, normal);
			PrintFormat("[CGS GUN ALIGN] Player=%1 color=%2 surface=%3 hitNormal=%4 portalNormal=%5 alignment=%6 top=%7", shooterId, color, surfaceType, normal, actualNormal, facingDot, actualUp);
			if (facingDot < 0.90)
				PrintFormat("[CGS GUN ALIGN] WARNING: Spawned portal face mismatch. color=%1 expectedNormal=%2 actualNormal=%3", color, normal, actualNormal);
		}

		// Delete only this owner's OLD portal of the SAME color.
		if (previous && previous != spawned && previous.GetOwner())
			SCR_EntityHelper.DeleteEntityAndChildren(previous.GetOwner());

		if (m_bDebug)
			PrintFormat("[CGS GUN] Player=%1 fired color=%2 hit=%3 normal=%4 surfaceEntity=%5", shooterId, color, hitPosition, normal, trace.TraceEnt);
		return true;
	}
}
