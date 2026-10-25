[ComponentEditorProps(category: "CGS Portal", description: "Stationary two-way portal: front-to-back plane crossing test.")]
class CGS_PortalComponentClass : ScriptComponentClass
{
}

class CGS_PortalComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.EditBox, "Portal identity: 0 = Blue, 1 = Orange")]
	protected int m_iPortalID;

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


	[Attribute("1", UIWidgets.CheckBox, "EXPERIMENT: attempt to preserve player momentum through portal")]
	protected bool m_bPreserveMomentum;

	[Attribute("1.0", UIWidgets.EditBox, "EXPERIMENT: exit velocity multiplier")]
	protected float m_fMomentumMultiplier;

	[Attribute("45", UIWidgets.EditBox, "EXPERIMENT: max exit speed in m/s; 0 means uncapped")]
	protected float m_fMaxExitSpeed;

	[Attribute("1", UIWidgets.CheckBox, "Draw +Z exit facing arrow and trigger outline during play")]
	protected bool m_bShowFacingArrow;

	[Attribute("2.5", UIWidgets.EditBox, "Facing arrow length in meters")]
	protected float m_fFacingArrowLength;

	[Attribute("1", UIWidgets.EditBox, "Portal front axis: 0=+Z, 1=+Y, 2=-Y, 3=-Z, 4=+X, 5=-X")]
	protected int m_iFacingAxis;

	protected int m_iSuccessfulTransfers;

	protected ref map<IEntity, vector> m_mPreviousPositions = new map<IEntity, vector>();

	bool IsDebugEnabled()
	{
		return m_bDebug;
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
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_bShowFacingArrow)
			DrawFacingDebug(owner);

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
			bool shouldTransfer = false;

			if (m_bUsePlaneCrossing)
			{
				vector previousPos;
				if (m_mPreviousPositions.Find(character, previousPos))
				{
					vector previousOffset = previousPos - portalTransform[3];
					vector currentOffset = currentPos - portalTransform[3];

					float previousDepth = vector.Dot(previousOffset, portalNormal);
					float currentDepth = vector.Dot(currentOffset, portalNormal);

					// Catch the player IN FRONT of the portal plane, before they
					// hit a solid floor or wall. Positive local Z is the entry side.
					float triggerDepth = m_fFrontCatchDistance;
					if (triggerDepth < 0)
						triggerDepth = 0;

					if (previousDepth > triggerDepth && currentDepth <= triggerDepth)
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

			if (CGS_PortalTransferComponent.TryTransfer(character, this, partner))
			{
				m_iSuccessfulTransfers++;
				if (m_bDebug && (m_iSuccessfulTransfers <= 3 || m_iSuccessfulTransfers % 10 == 0))
					PrintFormat("[CGS PORTAL] Transfer #%1 player=%2 portal %3 to %4", m_iSuccessfulTransfers, playerId, m_iPortalID, partner.GetPortalID());
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
}
