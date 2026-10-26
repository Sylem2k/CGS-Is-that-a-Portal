// CGS Portal Gun - Phase 1 grab/pull/drop.
// This component belongs on the GUN. It only operates on objects that
// contain CGS_GrabbableComponent on their root and have dynamic physics.
// All target selection, ownership and physical movement are server-owned.
[ComponentEditorProps(category: "CGS Portal", description: "Server-authoritative G-key grab and physical hold for opt-in props.")]
class CGS_GrabGunComponentClass : ScriptComponentClass
{
}

class CGS_GrabGunComponent : ScriptComponent
{
	[Attribute("8", UIWidgets.EditBox, "Maximum distance to acquire a grabbable prop (meters)")]
	protected float m_fGrabRange;

	[Attribute("12", UIWidgets.EditBox, "Release when prop moves farther than this from player (meters)")]
	protected float m_fBreakDistance;

	[Attribute("8", UIWidgets.EditBox, "Physical pull responsiveness")]
	protected float m_fPullResponse;

	[Attribute("15", UIWidgets.EditBox, "Maximum physical hold speed (meters per second)")]
	protected float m_fMaxHoldSpeed;

	[Attribute("1", UIWidgets.CheckBox, "Log grab/release events")]
	protected bool m_bDebug;

	protected CGS_GrabbableComponent m_pHeld;
	protected int m_iHolderPlayerId = -1;
	protected vector m_vAimOrigin;
	protected vector m_vAimDirection;
	protected int m_iLastAimUpdateMs;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.FRAME);
	}

	// Never trust a player-supplied target entity or origin. The server
	// verifies the EQUIPPED gun and repeats the raycast from near the eye.
	protected bool ValidatePlayerAndAim(SCR_PlayerController requester, vector cameraOrigin, vector cameraDirection)
	{
		if (!Replication.IsServer() || !requester || !GetOwner() || !GetGame())
			return false;

		int playerId = requester.GetPlayerId();
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		ChimeraCharacter character = ChimeraCharacter.Cast(playerEntity);
		if (!character || character.IsDeleted() || requester.GetControlledEntity() != playerEntity)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.IsDead())
			return false;

		BaseWeaponManagerComponent manager = controller.GetWeaponManagerComponent();
		if (!manager)
			return false;

		BaseWeaponComponent currentWeapon = manager.GetCurrentWeapon();
		if (!currentWeapon || currentWeapon.GetOwner() != GetOwner())
			return false;

		vector expectedEye = playerEntity.GetOrigin() + Vector(0, 1.6, 0);
		if (vector.DistanceSq(expectedEye, cameraOrigin) > 25)
			return false;

		float aimLengthSq = cameraDirection.LengthSq();
		return aimLengthSq >= 0.5625 && aimLengthSq <= 1.5625;
	}

	bool ServerStartGrab(SCR_PlayerController requester, vector cameraOrigin, vector cameraDirection)
	{
		if (!ValidatePlayerAndAim(requester, cameraOrigin, cameraDirection))
			return false;

		int playerId = requester.GetPlayerId();
		if (m_pHeld)
			return false;

		vector forward = cameraDirection.Normalized();
		TraceParam trace = new TraceParam();
		trace.Start = cameraOrigin;
		trace.End = cameraOrigin + forward * Math.Clamp(m_fGrabRange, 1.0, 15.0);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;

		ref array<IEntity> excluded = {};
		excluded.Insert(requester.GetControlledEntity());
		excluded.Insert(GetOwner());
		trace.ExcludeArray = excluded;

		if (GetGame().GetWorld().TraceMove(trace, null) >= 1)
			return false;

		IEntity hit = trace.TraceEnt;
		CGS_GrabbableComponent grabbable;
		for (int i = 0; i < 12 && hit; i++)
		{
			grabbable = CGS_GrabbableComponent.Cast(hit.FindComponent(CGS_GrabbableComponent));
			if (grabbable)
				break;
			hit = hit.GetParent();
		}

		if (!grabbable || !hit || ChimeraCharacter.Cast(hit) || Vehicle.Cast(hit))
			return false;

		IEntity prop = grabbable.GetOwner();
		if (!prop || prop.IsDeleted() || !prop.GetPhysics() || !prop.GetPhysics().IsDynamic())
			return false;

		// Acquire only an explicit opt-in object that no other player holds.
		if (!grabbable.ServerTryClaim(playerId))
			return false;

		m_pHeld = grabbable;
		m_iHolderPlayerId = playerId;
		m_vAimOrigin = cameraOrigin;
		m_vAimDirection = forward;
		m_iLastAimUpdateMs = GetGame().GetWorld().GetWorldTime();

		if (m_bDebug)
			PrintFormat("[CGS GRAB] Started player=%1 prop=%2", playerId, prop);
		return true;
	}

	void ServerUpdateGrabAim(SCR_PlayerController requester, vector cameraOrigin, vector cameraDirection)
	{
		if (!m_pHeld || !requester || requester.GetPlayerId() != m_iHolderPlayerId)
			return;

		if (!ValidatePlayerAndAim(requester, cameraOrigin, cameraDirection))
		{
			ServerReleaseGrab(m_iHolderPlayerId);
			return;
		}

		m_vAimOrigin = cameraOrigin;
		m_vAimDirection = cameraDirection.Normalized();
		m_iLastAimUpdateMs = GetGame().GetWorld().GetWorldTime();
	}

	void ServerReleaseGrab(int playerId)
	{
		if (!Replication.IsServer() || playerId != m_iHolderPlayerId)
			return;

		if (m_pHeld)
			m_pHeld.ServerRelease(playerId);

		if (m_bDebug)
			PrintFormat("[CGS GRAB] Released player=%1", playerId);

		m_pHeld = null;
		m_iHolderPlayerId = -1;
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer() || !m_pHeld || !GetGame())
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		int now = GetGame().GetWorld().GetWorldTime();

		// Losing the gun, disconnecting, opening a menu or dropping the
		// input stream must NEVER leave a permanently claimed object.
		PlayerController requester = players.GetPlayerController(m_iHolderPlayerId);
		SCR_PlayerController grabber = SCR_PlayerController.Cast(requester);
		if (!grabber || now - m_iLastAimUpdateMs > 600 || !ValidatePlayerAndAim(grabber, m_vAimOrigin, m_vAimDirection))
		{
			ServerReleaseGrab(m_iHolderPlayerId);
			return;
		}

		IEntity prop = m_pHeld.GetOwner();
		IEntity player = players.GetPlayerControlledEntity(m_iHolderPlayerId);
		if (!prop || prop.IsDeleted() || !player || !m_pHeld.ServerIsHeldBy(m_iHolderPlayerId))
		{
			ServerReleaseGrab(m_iHolderPlayerId);
			return;
		}

		float maxDistance = Math.Clamp(m_fBreakDistance, 2.0, 25.0);
		if (vector.DistanceSq(player.GetOrigin(), prop.GetOrigin()) > maxDistance * maxDistance)
		{
			ServerReleaseGrab(m_iHolderPlayerId);
			return;
		}

		vector target = m_vAimOrigin + m_vAimDirection * m_pHeld.GetHoldDistance();
		if (!CGS_GrabService.ApplyHoldVelocity(prop, target, m_fPullResponse, m_fMaxHoldSpeed))
			ServerReleaseGrab(m_iHolderPlayerId);
	}
}
