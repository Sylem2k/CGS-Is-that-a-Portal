// CGS Portal Gun: attach this to the ROOT of a replicated dynamic physics prop.
// Only objects explicitly marked grabbable can be carried.
// Does not change any existing portal-gun shooting logic.
[ComponentEditorProps(category: "CGS Portal", description: "Opt-in server-owned physical grab target for the Portal Gun.")]
class CGS_GrabbableComponentClass : ScriptComponentClass
{
}

class CGS_GrabbableComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Allow the Portal Gun to grab this prop")]
	protected bool m_bCanBeGrabbed;

	[Attribute("100", UIWidgets.EditBox, "Maximum mass in kilograms (0 = no mass restriction for this prop)")]
	protected float m_fMaxMass;

	[Attribute("2.5", UIWidgets.EditBox, "Distance in front of the player's camera while carried (meters)")]
	protected float m_fHoldDistance;

	[Attribute("1", UIWidgets.CheckBox, "Allow this dynamic prop to travel through paired portals")]
	protected bool m_bAllowPortalTravel;

	[Attribute("25", UIWidgets.EditBox, "Maximum prop exit speed after portal travel (m/s, no boosting)")]
	protected float m_fPortalMaxExitSpeed;

	// Remember the last portal exit for this prop alone. A newly
	// teleported prop must first move away from the destination opening.
	protected CGS_PortalComponent m_pProtectedExit;
	protected int m_iLastPortalTransferMs = -10000;
	protected vector m_vPreviousPropPosition;
	protected int m_iPreviousPropSampleMs;
	protected bool m_bHasPreviousPropPosition;
	protected int m_iLastBlockedPortalLogMs = -10000;
	protected int m_iLastPortalWatchLogMs = -10000;
	protected bool m_bPortalTickReported;

	// This state is only read and changed by the authoritative server.
	// Do not replicate it as a writable client variable.
	protected int m_iGrabbedByPlayer = -1;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.FRAME);
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer() || !GetGame() || !owner || owner.IsDeleted())
			return;

		int nowMs = GetGame().GetWorld().GetWorldTime();
		vector currentPos = owner.GetOrigin();
		if (!m_bPortalTickReported && m_bAllowPortalTravel)
		{
			m_bPortalTickReported = true;
			PrintFormat("[CGS PORTAL PROP TICK] active owner=%1 at=%2 travelEnabled=%3",
				owner, currentPos, CanTravelThroughPortals());
		}

		if (!m_bHasPreviousPropPosition)
		{
			m_vPreviousPropPosition = currentPos;
			m_iPreviousPropSampleMs = nowMs;
			m_bHasPreviousPropPosition = true;
			return;
		}

		int elapsedMs = nowMs - m_iPreviousPropSampleMs;
		if (elapsedMs > 0 && elapsedMs <= 250)
		{
			CGS_PortalPropTransferService.TryTransfer(this, m_vPreviousPropPosition, currentPos, elapsedMs, nowMs);
		}

		// IMPORTANT: GetOrigin AFTER attempting transfer. Otherwise the
		// huge jump to the other portal looks like an impossible motion
		// sample on the next frame and can generate duplicate transfers.
		m_vPreviousPropPosition = owner.GetOrigin();
		m_iPreviousPropSampleMs = nowMs;
	}

	bool CanTravelThroughPortals()
	{
		return m_bAllowPortalTravel && m_bCanBeGrabbed;
	}

	float GetPortalMaxExitSpeed()
	{
		return Math.Clamp(m_fPortalMaxExitSpeed, 1.0, 35.0);
	}

	bool ServerIsGrabbed()
	{
		return m_iGrabbedByPlayer > 0;
	}

	bool IsPropPortalEntrySuppressed(CGS_PortalComponent entry, vector currentPos, int nowMs)
	{
		if (m_iLastPortalTransferMs < 0)
			return false;

		// Universal short cooldown stops instant ping-pong when paired
		// portals are close together or network physics is catching up.
		if (nowMs - m_iLastPortalTransferMs < 650)
			return true;

		if (!m_pProtectedExit || !m_pProtectedExit.GetOwner() || m_pProtectedExit.GetOwner().IsDeleted())
		{
			m_pProtectedExit = null;
			return false;
		}

		if (!m_pProtectedExit.IsWithinExitCaptureArea(currentPos))
		{
			m_pProtectedExit = null;
			return false;
		}

		// Still standing/falling inside the opening we exited from.
		// Do not re-enter until the physical prop leaves that area.
		return entry == m_pProtectedExit;
	}

	// Near-portal diagnostics report the crossing gates once every
	// 650ms per cube. No per-frame log spam on a busy server.
	bool ShouldLogPortalWatch(int nowMs)
	{
		if (nowMs - m_iLastPortalWatchLogMs < 650)
			return false;

		m_iLastPortalWatchLogMs = nowMs;
		return true;
	}

	// Failed exit clearance can be tested many times while a cube is
	// pushing against the portal. Log at most once per second per prop.
	bool ShouldLogPortalBlocked(int nowMs)
	{
		if (nowMs - m_iLastBlockedPortalLogMs < 1000)
			return false;

		m_iLastBlockedPortalLogMs = nowMs;
		return true;
	}

	void ServerRecordPortalExit(CGS_PortalComponent destination, int nowMs)
	{
		if (!Replication.IsServer())
			return;

		m_pProtectedExit = destination;
		m_iLastPortalTransferMs = nowMs;
	}

	bool ServerTryClaim(int playerId)
	{
		if (!Replication.IsServer() || playerId <= 0 || !m_bCanBeGrabbed)
			return false;

		IEntity entity = GetOwner();
		if (!entity || entity.IsDeleted())
			return false;

		Physics physics = entity.GetPhysics();
		if (!physics || !physics.IsDynamic())
			return false;

		if (m_fMaxMass > 0 && physics.GetMass() > m_fMaxMass)
			return false;

		if (m_iGrabbedByPlayer > 0 && m_iGrabbedByPlayer != playerId)
			return false;

		m_iGrabbedByPlayer = playerId;
		return true;
	}

	bool ServerIsHeldBy(int playerId)
	{
		return Replication.IsServer() && playerId > 0 && m_iGrabbedByPlayer == playerId;
	}

	void ServerRelease(int playerId)
	{
		if (!Replication.IsServer())
			return;

		if (playerId == m_iGrabbedByPlayer)
			m_iGrabbedByPlayer = -1;
	}

	float GetHoldDistance()
	{
		return Math.Clamp(m_fHoldDistance, 1.4, 4.5);
	}
}
