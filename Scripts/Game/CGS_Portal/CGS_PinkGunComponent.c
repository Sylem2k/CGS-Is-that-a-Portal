//------------------------------------------------------------------------------------------------
// CGS Pinkifier Gun - NON-LETHAL raycast toy.
//
// This is a separate pistol-like weapon, not a mode of the working
// Blue/Orange portal gun. It uses the existing pistol animation/ADS
// setup but has no physical bullets, ammo or damage.
//
// On LMB the owning local player requests a shot. SERVER verifies the
// held weapon, trace start/direction, and actual first entity hit.
// Only a ChimeraCharacter (player or AI) can be painted.
//
// The authority then broadcasts the target's RplId. Every client
// locally changes that character's visible mesh materials to pink,
// including worn child models. No mesh, gear, damage or hit zone data
// is replaced. This is cosmetic, lasts until the entity is replaced,
// and has no per-hit network video/texture traffic.
//
// Current prototype limitation: paint state is sent as an event and
// is not yet JIP-persistent; players joining later may see original
// clothes. Some inventory/LOD swaps may restore original materials.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS Pinkifier", description: "Experimental zero-damage gun: shoot characters to paint their visible meshes pink.")]
class CGS_PinkGunComponentClass : ScriptComponentClass
{
}

class CGS_PinkGunComponent : ScriptComponent
{
	[Attribute("75", UIWidgets.EditBox, "Maximum shot distance in meters.")]
	protected float m_fRange;

	[Attribute("300", UIWidgets.EditBox, "Minimum time between successful shots, milliseconds.")]
	protected int m_iShotCooldownMs;

	[Attribute("1", UIWidgets.CheckBox, "Log successful paints and rejected shots on server.")]
	protected bool m_bDebug;

	[Attribute("1", UIWidgets.CheckBox, "Color the Pinkifier weapon model pink too, locally, without modifying the Portal Gun.")]
	protected bool m_bTintThisGun;

	protected int m_iLastShotTime = -10000;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!m_bTintThisGun || !owner)
			return;

		VObject weaponMesh = owner.GetVObject();
		if (!weaponMesh)
			return;

		// The imported Portal_Gun.xob source-material slot is "Water_Gun".
		// Remap on THIS weapon entity only. The normal portal gun keeps
		// its original water-gun material unmodified.
		owner.SetObject(weaponMesh, "$remap 'Water_Gun' '{6AB2F1A000000101}Common/Models/Data/CGS_Pinkifier_Gun.emat';");
	}

	//------------------------------------------------------------------------------------------------
	bool ServerTryPinkify(SCR_PlayerController requester, vector cameraOrigin, vector cameraForward)
	{
		if (!Replication.IsServer() || !requester || !GetOwner())
			return false;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return false;

		int playerId = requester.GetPlayerId();
		IEntity playerEntity = players.GetPlayerControlledEntity(playerId);
		ChimeraCharacter player = ChimeraCharacter.Cast(playerEntity);
		if (!player || requester.GetControlledEntity() != playerEntity)
			return false;

		CharacterControllerComponent characterController = player.GetCharacterController();
		if (!characterController || characterController.IsDead())
			return false;

		BaseWeaponManagerComponent manager = characterController.GetWeaponManagerComponent();
		if (!manager)
			return false;

		BaseWeaponComponent equipped = manager.GetCurrentWeapon();
		if (!equipped || equipped.GetOwner() != GetOwner())
		{
			if (m_bDebug)
				Print("[CGS PINKIFIER] Shot refused: Pinkifier is not actively equipped.");
			return false;
		}

		int now = GetGame().GetWorld().GetWorldTime();
		if (now - m_iLastShotTime < m_iShotCooldownMs)
			return false;

		float cameraLength = cameraForward.Length();
		if (cameraLength < 0.75 || cameraLength > 1.25)
			return false;

		// Server validates the camera cannot be forged far from player.
		vector playerEye = playerEntity.GetOrigin() + Vector(0, 1.6, 0);
		if (vector.DistanceSq(cameraOrigin, playerEye) > 25)
			return false;

		float range = Math.Clamp(m_fRange, 2.0, 150.0);
		vector direction = cameraForward.Normalized();

		TraceParam trace = new TraceParam();
		trace.Start = cameraOrigin;
		trace.End = cameraOrigin + direction * range;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;

		// Cannot pinkify yourself by shooting your own body or pistol.
		ref array<IEntity> excluded = {};
		excluded.Insert(playerEntity);
		excluded.Insert(GetOwner());
		trace.ExcludeArray = excluded;

		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		if (fraction >= 1)
			return false;

		IEntity hit = trace.TraceEnt;
		ChimeraCharacter target = ChimeraCharacter.Cast(hit);

		// Hitting worn equipment may return a clothing/helmet child,
		// not the main ChimeraCharacter. Climb its parent hierarchy.
		for (int depth = 0; depth < 12 && hit && !target; depth++)
		{
			hit = hit.GetParent();
			target = ChimeraCharacter.Cast(hit);
		}

		if (!target || target.IsDeleted())
		{
			if (m_bDebug)
				Print("[CGS PINKIFIER] Shot blocked by world geometry or a non-character entity.");
			return false;
		}

		// Use the CHARACTER's replicated identity so remote clients
		// can find the corresponding pawn, including AI characters.
		RplId targetID = SCR_EntityHelper.EntityToRplId(target);
		if (!targetID.IsValid())
		{
			if (m_bDebug)
				Print("[CGS PINKIFIER] Target has no valid RplId; cannot replicate pink paint.");
			return false;
		}

		m_iLastShotTime = now;
		requester.CGS_PinkBroadcastTarget(targetID);

		if (m_bDebug)
			PrintFormat("[CGS PINKIFIER] player=%1 painted character=%2 id=%3 distance=%4", playerId, target, targetID, range * fraction);

		return true;
	}
}
