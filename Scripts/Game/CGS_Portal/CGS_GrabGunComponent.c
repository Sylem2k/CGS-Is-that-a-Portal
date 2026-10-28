// CGS Portal Gun - Phase 1 grab/pull/drop.
// This component belongs on the GUN. It only operates on objects that
// contain CGS_GrabbableComponent on their root and have dynamic physics.
// All target selection, ownership and physical movement are server-owned.
[ComponentEditorProps(category: "CGS Portal", description: "Server-authoritative F grab / V throw for opt-in dynamic props.")]
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

	[Attribute("15", UIWidgets.EditBox, "Speed in meters per second when throwing with V")]
	protected float m_fThrowSpeed;

	[Attribute("1", UIWidgets.CheckBox, "Log grab/release/throw events")]
	protected bool m_bDebug;

	protected CGS_GrabbableComponent m_pHeld;
	protected int m_iHolderPlayerId = -1;
	protected vector m_vAimOrigin;
	protected vector m_vAimDirection;
	protected int m_iLastAimUpdateMs;

	// A throwing gun already receives reliable server FRAME updates,
	// while an idle physics prop may stop ticking when the rigidbody
	// sleeps. Watch the most recently thrown object for four seconds.
	// This supplements the grabbable's own tick; the transfer service
	// guards against duplicate crossings.
	protected CGS_GrabbableComponent m_pRecentThrown;
	protected vector m_vRecentThrownPosition;
	protected int m_iRecentThrownSampleMs;
	protected int m_iRecentThrownUntilMs;
	protected int m_iRecentThrownLogMs;

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
		{
			if (m_bDebug)
				Print("[CGS GRAB REJECT] Gun not equipped, invalid camera, or character unavailable.");
			return false;
		}

		int playerId = requester.GetPlayerId();
		if (m_pHeld)
		{
			if (m_bDebug)
				PrintFormat("[CGS GRAB REJECT] Gun already holds a prop for player=%1", m_iHolderPlayerId);
			return false;
		}

		vector forward = cameraDirection.Normalized();
		TraceParam trace = new TraceParam();
		trace.Start = cameraOrigin;
		trace.End = cameraOrigin + forward * Math.Clamp(m_fGrabRange, 1.0, 15.0);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;

		ref array<IEntity> excluded = {};
		excluded.Insert(requester.GetControlledEntity());
		excluded.Insert(GetOwner());
		trace.ExcludeArray = excluded;

		float hitFraction = GetGame().GetWorld().TraceMove(trace, null);
		if (hitFraction >= 1)
		{
			if (m_bDebug)
				PrintFormat("[CGS GRAB REJECT] Raycast hit nothing within %1m. player=%2", m_fGrabRange, playerId);
			return false;
		}

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
		{
			if (m_bDebug)
				PrintFormat("[CGS GRAB REJECT] Ray hit non-grabbable entity=%1; add CGS_GrabbableComponent to root.", trace.TraceEnt);
			return false;
		}

		IEntity prop = grabbable.GetOwner();
		if (!prop || prop.IsDeleted() || !prop.GetPhysics() || !prop.GetPhysics().IsDynamic())
		{
			if (m_bDebug)
				PrintFormat("[CGS GRAB REJECT] Prop has no DYNAMIC physics. prop=%1", prop);
			return false;
		}

		// Acquire only an explicit opt-in object that no other player holds.
		if (!grabbable.ServerTryClaim(playerId))
		{
			if (m_bDebug)
				PrintFormat("[CGS GRAB REJECT] Prop declined claim (disabled, overweight, or held by another player). prop=%1", prop);
			return false;
		}

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

	// The player must still hold the original, server-approved object
	// and have this same Portal Gun EQUIPPED to throw. The target is
	// NEVER supplied by the client. V only applies to an active hold.
	bool ServerThrowGrab(SCR_PlayerController requester, vector cameraOrigin, vector cameraDirection)
	{
		if (!Replication.IsServer() || !m_pHeld || !requester)
			return false;

		int playerId = requester.GetPlayerId();
		if (playerId != m_iHolderPlayerId)
			return false;

		if (!ValidatePlayerAndAim(requester, cameraOrigin, cameraDirection))
		{
			if (m_bDebug)
				PrintFormat("[CGS GRAB THROW REJECT] player=%1 gun not equipped or aim invalid", playerId);
			ServerReleaseGrab(playerId);
			return false;
		}

		IEntity prop = m_pHeld.GetOwner();
		if (!prop || prop.IsDeleted() || !m_pHeld.ServerIsHeldBy(playerId))
		{
			ServerReleaseGrab(playerId);
			return false;
		}

		// Use the same dynamic-body check as regular grab. If physics
		// disappears, release safely instead of launching a static prop.
		if (!CGS_GrabService.ApplyThrowVelocity(prop, cameraDirection, m_fThrowSpeed))
		{
			if (m_bDebug)
				PrintFormat("[CGS GRAB THROW REJECT] player=%1 prop=%2 dynamic physics unavailable", playerId, prop);
			ServerReleaseGrab(playerId);
			return false;
		}

		// Start tracking before dropping the weapon's held reference.
		// The cube is already server-owned and released from the hold
		// velocity controller, so normal rigidbody motion can continue.
		m_pRecentThrown = m_pHeld;
		m_vRecentThrownPosition = prop.GetOrigin();
		m_iRecentThrownSampleMs = GetGame().GetWorld().GetWorldTime();
		m_iRecentThrownUntilMs = m_iRecentThrownSampleMs + 4000;
		m_iRecentThrownLogMs = m_iRecentThrownSampleMs - 1000;

		m_pHeld.ServerRelease(playerId);
		m_pHeld = null;
		m_iHolderPlayerId = -1;

		if (m_bDebug)
			PrintFormat("[CGS GRAB THROW] player=%1 prop=%2 velocity=%3",
				playerId, prop, prop.GetPhysics().GetVelocity());
		return true;
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

	// One gun tick after V performs the same server-side portal check as
	// the cube's own script tick. This makes V throws observable even
	// when the cube's component is not receiving FRAME events.
	protected void CGS_UpdateRecentThrown()
	{
		if (!m_pRecentThrown || !GetGame())
			return;

		int nowMs = GetGame().GetWorld().GetWorldTime();
		IEntity prop = m_pRecentThrown.GetOwner();
		if (nowMs > m_iRecentThrownUntilMs || !prop || prop.IsDeleted() || m_pRecentThrown.ServerIsGrabbed())
		{
			m_pRecentThrown = null;
			return;
		}

		vector position = prop.GetOrigin();
		int elapsedMs = nowMs - m_iRecentThrownSampleMs;
		if (elapsedMs > 0 && elapsedMs <= 250)
		{
			if (CGS_PortalPropTransferService.TryTransfer(m_pRecentThrown, m_vRecentThrownPosition, position, elapsedMs, nowMs))
			{
				if (m_bDebug)
					PrintFormat("[CGS PORTAL THROW WATCH] transfer succeeded at %1", prop.GetOrigin());
				m_pRecentThrown = null;
				return;
			}
		}

		if (m_bDebug && nowMs - m_iRecentThrownLogMs >= 750)
		{
			m_iRecentThrownLogMs = nowMs;
			ref array<CGS_PortalComponent> portals = {};
			CGS_PortalManagerComponent.GetActivePortals(portals);
			Physics body = prop.GetPhysics();
			vector velocity = vector.Zero;
			if (body)
				velocity = body.GetVelocity();
			PrintFormat("[CGS PORTAL THROW WATCH] prop=%1 pos=%2 velocity=%3 registeredPortals=%4 timeLeftMs=%5",
				prop, position, velocity, portals.Count(), m_iRecentThrownUntilMs - nowMs);
		}

		m_vRecentThrownPosition = prop.GetOrigin();
		m_iRecentThrownSampleMs = nowMs;
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer() || !GetGame())
			return;

		CGS_UpdateRecentThrown();

		if (!m_pHeld)
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
