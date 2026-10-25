[ComponentEditorProps(category: "CGS Portal", description: "Stationary two-way portal: front-to-back plane crossing test.")]
class CGS_PortalComponentClass : ScriptComponentClass
{
}

class CGS_PortalComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.EditBox, "Portal identity: 0 = Blue, 1 = Orange")]
	protected int m_iPortalID;

	// 0 = manually placed legacy test portal; >0 = owner player's ID.
	// Runtime-spawned gun portals get assigned immediately after spawn.
	//
	// PIP needs to pair each owner's Blue and Orange portals LOCALLY
	// on every client. Server-only static manager maps do not replicate.
	// This single RplProp keeps per-player render pairing unambiguous in MP.
	[RplProp()]
	[Attribute("0", UIWidgets.EditBox, "Portal owner player ID. 0 = shared test portal")]
	protected int m_iPortalOwnerID;

	[Attribute("1", UIWidgets.CheckBox, "Enable this portal")]
	protected bool m_bEnabled;

	[Attribute("2.5", UIWidgets.EditBox, "Exit offset in meters")]
	protected float m_fExitOffset;

	[Attribute("2.0", UIWidgets.EditBox, "Legacy proximity activation distance (meters)")]
	protected float m_fActivationDistance;

	[Attribute("1", UIWidgets.CheckBox, "Print portal diagnostic messages to server log")]
	protected bool m_bDebug;

	[Attribute("1", UIWidgets.CheckBox, "Use portal plane crossing; disable for legacy proximity mode")]
	protected bool m_bUsePlaneCrossing;

	[Attribute("1.5", UIWidgets.EditBox, "Portal half-width in meters")]
	protected float m_fHalfWidth;

	[Attribute("2.0", UIWidgets.EditBox, "Portal half-height in meters")]
	protected float m_fHalfHeight;

	[Attribute("1.0", UIWidgets.EditBox, "Front-side catch distance (meters); catches player before hitting solid floor/wall")]
	protected float m_fFrontCatchDistance;

	[Attribute("6.0", UIWidgets.EditBox, "Maximum crossing step distance in meters")]
	protected float m_fCrossingStep;

	// Players walking on level ground do not cross a horizontal portal plane.
	// This allows them to enter the portal footprint from the side instead.
	[Attribute("1", UIWidgets.CheckBox, "Allow walking onto upward-facing floor portals")]
	protected bool m_bWalkOnFloorEnabled;

	[Attribute("1.4", UIWidgets.EditBox, "Maximum player-origin height above floor portal for walking entry (meters)")]
	protected float m_fWalkOnHeight;

	// A solid wall can stop the character origin before it crosses the
	// original trigger plane. Detect walking into the face of a vertical
	// portal, using a chest-height probe rather than the feet position.
	[Attribute("1", UIWidgets.CheckBox, "Allow walking back into wall portals")]
	protected bool m_bWallEntryAssist;

	[Attribute("1.75", UIWidgets.EditBox, "Wall portal approach distance from surface (meters)")]
	protected float m_fWallEntryDistance;

	[Attribute("0.85", UIWidgets.EditBox, "Wall entry character-height adjustment (meters)")]
	protected float m_fWallEntryHeightOffset;

	[Attribute("0.35", UIWidgets.EditBox, "Wall entry extra border padding (meters)")]
	protected float m_fWallEntryEdgePadding;

	[Attribute("1", UIWidgets.CheckBox, "Allow approaching wall portal from either side of the flat mesh")]
	protected bool m_bWallEntryBothSides;

	// Optional generous catch volume for a FUN floor-to-ceiling fall loop.
	// This is separate from the visible portal's regular size.
	[Attribute("1", UIWidgets.CheckBox, "FUN MODE: rescue falling player above an upward floor portal")]
	protected bool m_bFallCatchAssist;

	[Attribute("3.5", UIWidgets.EditBox, "FUN MODE: rescue plane height above floor portal (meters)")]
	protected float m_fFallCatchHeight;

	[Attribute("5.0", UIWidgets.EditBox, "FUN MODE: rescue half-width (meters)")]
	protected float m_fFallCatchHalfWidth;

	[Attribute("5.0", UIWidgets.EditBox, "FUN MODE: rescue half-length (meters)")]
	protected float m_fFallCatchHalfLength;

	[Attribute("1.0", UIWidgets.EditBox, "Exit lateral offset scale: 1=physical position, 0=center of exit portal")]
	protected float m_fExitLateralScale;

	[Attribute("1", UIWidgets.CheckBox, "Safe player teleport: use Reforger SCR_Global.TeleportPlayer (resets velocity)")]
	protected bool m_bUseSafePlayerTeleport;


	[Attribute("1", UIWidgets.CheckBox, "EXPERIMENT: attempt to preserve player momentum through portal")]
	protected bool m_bPreserveMomentum;

	[Attribute("1.0", UIWidgets.EditBox, "EXPERIMENT: exit velocity multiplier")]
	protected float m_fMomentumMultiplier;

	[Attribute("45", UIWidgets.EditBox, "EXPERIMENT: max exit speed in m/s; 0 means uncapped")]
	protected float m_fMaxExitSpeed;

	// Normal characters do not have dynamic physics velocity. This mode
	// remembers velocity across teleports and compensates their movement
	// in small, collision-swept increments, without disabling damage.
	[Attribute("0", UIWidgets.CheckBox, "EXPERIMENT: carry simulated momentum after teleport")]
	protected bool m_bSimulateMomentum;

	[Attribute("4.0", UIWidgets.EditBox, "EXPERIMENT: carry momentum for this many seconds (max 15)")]
	protected float m_fMomentumCarrySeconds;

	[Attribute("9.81", UIWidgets.EditBox, "EXPERIMENT: virtual gravity in m/s2")]
	protected float m_fMomentumGravity;

	[Attribute("0.7", UIWidgets.EditBox, "EXPERIMENT: largest momentum correction each frame (meters)")]
	protected float m_fMomentumMaxStep;

	[Attribute("1", UIWidgets.CheckBox, "Draw +Z exit facing arrow and trigger outline during play")]
	protected bool m_bShowFacingArrow;

	[Attribute("2.5", UIWidgets.EditBox, "Facing arrow length in meters")]
	protected float m_fFacingArrowLength;

	[Attribute("1", UIWidgets.EditBox, "Portal front axis: 0=+Z, 1=+Y, 2=-Y, 3=-Z, 4=+X, 5=-X")]
	protected int m_iFacingAxis;

	protected int m_iSuccessfulTransfers;

	protected ref map<IEntity, vector> m_mPreviousPositions = new map<IEntity, vector>();

	// Debug each wall approach at most once every 1.5 seconds per player.
	protected ref map<IEntity, int> m_mLastWallDebugTimes = new map<IEntity, int>();

	bool IsDebugEnabled()
	{
		return m_bDebug;
	}

	int GetPortalOwnerID()
	{
		return m_iPortalOwnerID;
	}

	// Must be called on server immediately after spawning the prefab.
	// Move registration from the temporary world pair to this player's pair.
	void SetPortalOwnerID(int ownerPlayerId)
	{
		if (!Replication.IsServer() || ownerPlayerId < 1)
			return;

		CGS_PortalManagerComponent.UnregisterPortal(this);
		m_iPortalOwnerID = ownerPlayerId;
		// Ensure already-replicated proxies receive the updated owner ID
		// after the server assigns it immediately after spawning.
		Replication.BumpMe();
		CGS_PortalManagerComponent.RegisterPortal(this);
		if (m_bDebug)
			PrintFormat("[CGS PORTAL OWNER] Portal color=%1 registered to player=%2", m_iPortalID, m_iPortalOwnerID);
	}

	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			// A deleted/replaced portal must not remain in the exit guard.
			CGS_PortalTransferComponent.ForgetDeletedPortal(this);
			CGS_PortalManagerComponent.UnregisterPortal(this);
		}
		super.OnDelete(owner);
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
	}

	override void EOnInit(IEntity owner)
	{
		if (!Replication.IsServer())
			return;

		CGS_PortalManagerComponent.RegisterPortal(this);
		if (m_bDebug)
		{
			vector right;
			vector up;
			vector normal;
			GetPortalBasis(right, up, normal);

			string facing = "SIDEWAYS";
			if (normal[1] > 0.75)
				facing = "UP";
			else if (normal[1] < -0.75)
				facing = "DOWN";

			PrintFormat("[CGS PORTAL FACING] ID=%1 position=%2 faceAxis=%3 normal=%4 facing=%5", m_iPortalID, owner.GetOrigin(), m_iFacingAxis, normal, facing);
		}
	}

	// Called after a successful transfer to prevent the previous position
	// (in a different part of the world) being treated as a new crossing.
	void ResetEntryHistory(IEntity character)
	{
		if (!character)
			return;

		m_mPreviousPositions.Remove(character);
		m_mLastWallDebugTimes.Remove(character);
	}

	// Whether this point is still inside this portal's FULL catch area.
	// This is used ONLY to re-arm an exit portal after its traveller
	// actually departs the area. It does not change entry detection.
	bool IsWithinExitCaptureArea(vector worldPosition)
	{
		IEntity portalEntity = GetOwner();
		if (!portalEntity)
			return false;

		vector portalRight;
		vector portalUp;
		vector portalNormal;
		GetPortalBasis(portalRight, portalUp, portalNormal);

		vector delta = worldPosition - portalEntity.GetOrigin();

		float halfWidth = m_fHalfWidth;
		float halfHeight = m_fHalfHeight;
		float frontLimit = m_fFrontCatchDistance;

		// The Orange floor portal's FUN MODE uses a WIDER fall zone than
		// its visible oval. Without including that volume, a traveller
		// could be re-armed while still inside the catch trigger.
		if (m_bFallCatchAssist && portalNormal[1] > 0.75)
		{
			if (m_fFallCatchHalfWidth > halfWidth)
				halfWidth = m_fFallCatchHalfWidth;
			if (m_fFallCatchHalfLength > halfHeight)
				halfHeight = m_fFallCatchHalfLength;
			if (m_fFallCatchHeight > frontLimit)
				frontLimit = m_fFallCatchHeight;
		}

		// Wall entry has its own depth; include it when checking whether
		// a character has properly cleared the exit.
		if (m_bWallEntryAssist && Math.AbsFloat(portalNormal[1]) < 0.6)
		{
			if (m_fWallEntryDistance > frontLimit)
				frontLimit = m_fWallEntryDistance;
		}

		// Small hysteresis margin avoids re-arm flicker on zone edges.
		float margin = 0.65;
		float side = Math.AbsFloat(vector.Dot(delta, portalRight));
		float height = Math.AbsFloat(vector.Dot(delta, portalUp));
		float depth = vector.Dot(delta, portalNormal);

		return side <= halfWidth + margin && height <= halfHeight + margin
			&& depth >= -margin && depth <= frontLimit + margin;
	}

	// Return an orthonormal frame for the ACTUAL opening face.
	// Reforger world Y is vertical. Our current Blender portal planes lie
	// in local XZ, so local +Y (faceAxis 1) is their top surface.
	void GetPortalBasis(out vector right, out vector up, out vector normal)
	{
		vector axes[4];
		GetOwner().GetWorldTransform(axes);

		// 0: +Z, the original behavior for a plane modeled in local XY.
		right = axes[0];
		up = axes[1];
		normal = axes[2];

		// 1: +Y, floor portal with original Blender plane lying flat.
		if (m_iFacingAxis == 1)
		{
			right = axes[0];
			up = -axes[2];
			normal = axes[1];
		}
		// 2: -Y, underside of a ceiling portal using the same flat mesh.
		else if (m_iFacingAxis == 2)
		{
			right = axes[0];
			up = axes[2];
			normal = -axes[1];
		}
		else if (m_iFacingAxis == 3)
		{
			right = -axes[0];
			up = axes[1];
			normal = -axes[2];
		}
		else if (m_iFacingAxis == 4)
		{
			right = -axes[2];
			up = axes[1];
			normal = axes[0];
		}
		else if (m_iFacingAxis == 5)
		{
			right = axes[2];
			up = axes[1];
			normal = -axes[0];
		}
	}

	// Debug shapes only. The green arrow is the chosen EXIT normal.
	// The blue/yellow rectangle shows the early entry catch plane.
	protected void DrawFacingDebug(IEntity owner)
	{
		vector transform[4];
		owner.GetWorldTransform(transform);

		float length = m_fFacingArrowLength;
		if (length < 0.5)
			length = 0.5;

		vector rightAxis;
		vector upAxis;
		vector direction;
		GetPortalBasis(rightAxis, upAxis, direction);

		vector origin = transform[3];
		vector tip = origin + direction * length;
		vector wingBase = tip - direction * 0.5;
		vector segment[2];
		int frameColor = Color.BLUE;
		if (m_iPortalID == 1)
			frameColor = Color.YELLOW;

		segment[0] = origin;
		segment[1] = tip;
		Shape.CreateLines(Color.GREEN, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);
		segment[0] = tip;
		segment[1] = wingBase + rightAxis * 0.35;
		Shape.CreateLines(Color.GREEN, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);
		segment[1] = wingBase - rightAxis * 0.35;
		Shape.CreateLines(Color.GREEN, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);

		float catchDistance = m_fFrontCatchDistance;
		if (catchDistance < 0)
			catchDistance = 0;
		vector center = origin + direction * catchDistance;
		vector right = rightAxis * m_fHalfWidth;
		vector up = upAxis * m_fHalfHeight;
		vector a = center + right + up;
		vector b = center - right + up;
		vector c = center - right - up;
		vector d = center + right - up;

		segment[0] = a;
		segment[1] = b;
		Shape.CreateLines(frameColor, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);
		segment[0] = b;
		segment[1] = c;
		Shape.CreateLines(frameColor, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);
		segment[0] = c;
		segment[1] = d;
		Shape.CreateLines(frameColor, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);
		segment[0] = d;
		segment[1] = a;
		Shape.CreateLines(frameColor, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);

		// Pink rectangle = expanded FUN MODE midair rescue area.
		if (m_bFallCatchAssist && direction[1] > 0.75)
		{
			float rescueDepth = m_fFallCatchHeight;
			if (rescueDepth < m_fFrontCatchDistance + 0.1)
				rescueDepth = m_fFrontCatchDistance + 0.1;
			vector rescueCenter = origin + direction * rescueDepth;
			vector rescueRight = rightAxis * m_fFallCatchHalfWidth;
			vector rescueUp = upAxis * m_fFallCatchHalfLength;
			vector p1 = rescueCenter + rescueRight + rescueUp;
			vector p2 = rescueCenter - rescueRight + rescueUp;
			vector p3 = rescueCenter - rescueRight - rescueUp;
			vector p4 = rescueCenter + rescueRight - rescueUp;

			segment[0] = p1;
			segment[1] = p2;
			Shape.CreateLines(Color.PINK, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);
			segment[0] = p2;
			segment[1] = p3;
			Shape.CreateLines(Color.PINK, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);
			segment[0] = p3;
			segment[1] = p4;
			Shape.CreateLines(Color.PINK, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);
			segment[0] = p4;
			segment[1] = p1;
			Shape.CreateLines(Color.PINK, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, segment, 2);
		}
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		// Once-per-frame server update. The service itself handles duplicate
		// calls from Blue and Orange in the same frame.
		if (Replication.IsServer())
			CGS_PortalMomentumService.Update();

		if (m_bShowFacingArrow)
			DrawFacingDebug(owner);

		if (Replication.IsServer())
			CGS_PortalManagerComponent.CleanupDisconnectedPlayers();

		if (!Replication.IsServer() || !m_bEnabled)
			return;

		CGS_PortalComponent partner = CGS_PortalManagerComponent.GetPartner(this);
		if (!partner || !partner.IsPortalEnabled())
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		ref array<int> players = {};
		playerManager.GetPlayers(players);

		vector portalTransform[4];
		owner.GetWorldTransform(portalTransform);
		vector portalRight;
		vector portalUp;
		vector portalNormal;
		GetPortalBasis(portalRight, portalUp, portalNormal);

		foreach (int playerId : players)
		{
			IEntity character = playerManager.GetPlayerControlledEntity(playerId);
			if (!character)
				continue;

			vector currentPos = character.GetOrigin();

			// The portal that the player JUST emerged from must not
			// immediately grab them again. In particular, Orange's
			// wide FALL_ZONE covers the normal floor-portal exit height.
			// Update previous position even while entry is suppressed.
			if (CGS_PortalTransferComponent.IsExitEntrySuppressed(character, this))
			{
				m_mPreviousPositions.Set(character, currentPos);
				continue;
			}

			bool shouldTransfer = false;
			string entryMode = "PLANE";

			if (m_bUsePlaneCrossing)
			{
				vector previousPos;
				if (m_mPreviousPositions.Find(character, previousPos))
				{
					vector previousOffset = previousPos - portalTransform[3];
					vector currentOffset = currentPos - portalTransform[3];

					float previousDepth = vector.Dot(previousOffset, portalNormal);
					float currentDepth = vector.Dot(currentOffset, portalNormal);

					// A floor portal is also usable by WALKING ONTO its visible
					// footprint. A standing character's entity origin never
					// crosses the floating early-catch plane above the floor.
					// Trigger only on moving OUTSIDE -> INSIDE the rectangle,
					// and only if the portal actually faces mostly upward.
					if (m_bWalkOnFloorEnabled && portalNormal[1] > 0.75)
					{
						float prevSide = vector.Dot(previousOffset, portalRight);
						float prevHeight = vector.Dot(previousOffset, portalUp);
						float currSide = vector.Dot(currentOffset, portalRight);
						float currHeight = vector.Dot(currentOffset, portalUp);

						bool prevInside = Math.AbsFloat(prevSide) <= m_fHalfWidth && Math.AbsFloat(prevHeight) <= m_fHalfHeight;
						bool currInside = Math.AbsFloat(currSide) <= m_fHalfWidth && Math.AbsFloat(currHeight) <= m_fHalfHeight;

						// Character origins on ground are usually at foot level,
						// approximately even with the portal surface.
						if (!prevInside && currInside && currentDepth >= -0.4 && currentDepth <= m_fWalkOnHeight)
						{
							if (vector.DistanceSq(previousPos, currentPos) <= m_fCrossingStep * m_fCrossingStep)
							{
								shouldTransfer = true;
								entryMode = "WALK_ON";
							}
						}
					}

					// Bidirectional wall portal: approaching the entrance should
					// work even if a wall's collision stops the player before
					// crossing the portal plane. Unlike proximity-only
					// teleportation, a deliberate inward movement is required.
					if (!shouldTransfer && m_bWallEntryAssist && Math.AbsFloat(portalNormal[1]) < 0.6)
					{
						float maxApproach = m_fWallEntryDistance;
						if (maxApproach < 0.4)
							maxApproach = 0.4;
						float edgePadding = m_fWallEntryEdgePadding;
						if (edgePadding < 0)
							edgePadding = 0;

						float heightOffset = m_fWallEntryHeightOffset;
						if (heightOffset < 0)
							heightOffset = 0;

						vector bodyProbe = currentPos + Vector(0, heightOffset, 0);
						vector previousProbe = previousPos + Vector(0, heightOffset, 0);
						vector bodyOffset = bodyProbe - portalTransform[3];
						vector previousBodyOffset = previousProbe - portalTransform[3];

						float bodyDepth = vector.Dot(bodyOffset, portalNormal);
						float previousBodyDepth = vector.Dot(previousBodyOffset, portalNormal);
						float bodyRight = vector.Dot(bodyOffset, portalRight);
						float bodyUp = vector.Dot(bodyOffset, portalUp);
						bool insideFrame = Math.AbsFloat(bodyRight) <= m_fHalfWidth + edgePadding && Math.AbsFloat(bodyUp) <= m_fHalfHeight + edgePadding;

						float depthMovement = previousBodyDepth - bodyDepth;
						bool frontApproach = previousBodyDepth > 0.20 && bodyDepth >= -0.35 && bodyDepth <= maxApproach && depthMovement > 0.012;
						bool backApproach = m_bWallEntryBothSides && previousBodyDepth < -0.20 && bodyDepth >= -maxApproach && bodyDepth <= 0.35 && depthMovement < -0.012;
						bool shortStep = vector.DistanceSq(previousPos, currentPos) <= m_fCrossingStep * m_fCrossingStep;

						if (insideFrame && shortStep && (frontApproach || backApproach))
						{
							shouldTransfer = true;
							entryMode = "WALL_APPROACH";
							if (m_bDebug)
								PrintFormat("[CGS PORTAL WALL] Player=%1 portal=%2 depth=%3 frameRight=%4 frameUp=%5 fromBack=%6", playerId, m_iPortalID, bodyDepth, bodyRight, bodyUp, backApproach);
						}
						else if (m_bDebug && bodyDepth < maxApproach + 0.8 && bodyDepth > -maxApproach - 0.8)
						{
							int worldMs = GetGame().GetWorld().GetWorldTime();
							int lastWallDebug;
							if (!m_mLastWallDebugTimes.Find(character, lastWallDebug) || worldMs - lastWallDebug >= 1500)
							{
								m_mLastWallDebugTimes.Set(character, worldMs);
								PrintFormat("[CGS PORTAL WALL CHECK] ID=%1 depth=%2 frameRight=%3 frameUp=%4 inFrame=%5 inward=%6 backside=%7", m_iPortalID, bodyDepth, bodyRight, bodyUp, insideFrame, frontApproach, backApproach);
							}
						}
					}

					// FUN LOOP: rescue a DESCENDING character anywhere within the
					// elevated rescue volume, not just on the exact frame where
					// the top plane is crossed. Large frame steps, teleport
					// corrections and animation can skip a single-plane check.
					if (!shouldTransfer && m_bFallCatchAssist && portalNormal[1] > 0.75)
					{
						float rescueTop = m_fFallCatchHeight;
						if (rescueTop < m_fFrontCatchDistance + 0.1)
							rescueTop = m_fFrontCatchDistance + 0.1;

						float rescueSide = vector.Dot(currentOffset, portalRight);
						float rescueLength = vector.Dot(currentOffset, portalUp);
						bool inFootprint = Math.AbsFloat(rescueSide) <= m_fFallCatchHalfWidth && Math.AbsFloat(rescueLength) <= m_fFallCatchHalfLength;

						// A character may move only a few centimeters in one
						// rendered frame. Check both displacement and velocity.
						bool descending = previousPos[1] > currentPos[1] + 0.015;
						ChimeraCharacter candidate = ChimeraCharacter.Cast(character);
						if (candidate)
						{
							CharacterControllerComponent movementController = candidate.GetCharacterController();
							if (movementController && movementController.IsFalling() && movementController.GetVelocity()[1] < -0.25)
								descending = true;
						}

						// For an upward-facing floor portal this is a vertical
						// prism from 0.35 m above the portal to rescueTop.
						// Trigger BEFORE terrain collision, while falling.
						if (descending && inFootprint && currentDepth <= rescueTop && currentDepth >= 0.35)
						{
							shouldTransfer = true;
							entryMode = "FALL_ZONE";
						}
					}

					// Catch the player IN FRONT of the portal plane, before they
					// hit a solid floor or wall. Positive local Z is the entry side.
					float triggerDepth = m_fFrontCatchDistance;
					if (triggerDepth < 0)
						triggerDepth = 0;

					if (!shouldTransfer && previousDepth > triggerDepth && currentDepth <= triggerDepth)
					{
						if (vector.DistanceSq(previousPos, currentPos) <= m_fCrossingStep * m_fCrossingStep)
						{
							// Check the crossing point, not the end-of-frame location.
							float depthChange = previousDepth - currentDepth;
							if (depthChange > 0.0001)
							{
								float fraction = (previousDepth - triggerDepth) / depthChange;
								vector crossingPos = previousPos + (currentPos - previousPos) * fraction;
								vector crossingOffset = crossingPos - portalTransform[3];
								float sideOffset = vector.Dot(crossingOffset, portalRight);
								float upOffset = vector.Dot(crossingOffset, portalUp);

								if (Math.AbsFloat(sideOffset) <= m_fHalfWidth && Math.AbsFloat(upOffset) <= m_fHalfHeight)
									shouldTransfer = true;
							}
						}
					}
				}
			}
			else
			{
				shouldTransfer = vector.DistanceSq(owner.GetOrigin(), currentPos) <= m_fActivationDistance * m_fActivationDistance;
			}

			m_mPreviousPositions.Set(character, currentPos);
			if (!shouldTransfer)
				continue;

			if (CGS_PortalTransferComponent.TryTransfer(character, this, partner, playerId))
			{
				m_iSuccessfulTransfers++;
				if (m_bDebug && (m_iSuccessfulTransfers <= 3 || m_iSuccessfulTransfers % 10 == 0))
					PrintFormat("[CGS PORTAL] Transfer #%1 player=%2 portal %3 to %4 entry=%5", m_iSuccessfulTransfers, playerId, m_iPortalID, partner.GetPortalID(), entryMode);
			}
		}
	}

	int GetPortalID()
	{
		return m_iPortalID;
	}

	bool IsPortalEnabled()
	{
		return m_bEnabled;
	}

	float GetExitOffset()
	{
		return m_fExitOffset;
	}

	float GetExitLateralScale()
	{
		return m_fExitLateralScale;
	}

	bool GetUseSafePlayerTeleport()
	{
		return m_bUseSafePlayerTeleport;
	}

	float GetFrontCatchDistance()
	{
		return m_fFrontCatchDistance;
	}

	bool ShouldPreserveMomentum()
	{
		return m_bPreserveMomentum;
	}

	float GetMomentumMultiplier()
	{
		return m_fMomentumMultiplier;
	}

	float GetMaxExitSpeed()
	{
		return m_fMaxExitSpeed;
	}

	bool ShouldSimulateMomentum()
	{
		return m_bSimulateMomentum;
	}

	float GetMomentumCarrySeconds()
	{
		return m_fMomentumCarrySeconds;
	}

	float GetMomentumGravity()
	{
		return m_fMomentumGravity;
	}

	float GetMomentumMaxStep()
	{
		return m_fMomentumMaxStep;
	}
}
