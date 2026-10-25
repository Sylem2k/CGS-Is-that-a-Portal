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

	[Attribute("1.5", UIWidgets.EditBox, "Exit offset in meters")]
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

	[Attribute("2.0", UIWidgets.EditBox, "Maximum crossing step distance in meters")]
	protected float m_fCrossingStep;

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
			PrintFormat("[CGS PORTAL] Registered portal ID=%1 at %2", m_iPortalID, owner.GetOrigin());
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
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

					float previousDepth = vector.Dot(previousOffset, portalTransform[2]);
					float currentDepth = vector.Dot(currentOffset, portalTransform[2]);
					float sideOffset = vector.Dot(currentOffset, portalTransform[0]);
					float upOffset = vector.Dot(currentOffset, portalTransform[1]);

					// A crossing from the portal's front (positive Z) to its back.
					// Prevent a long-distance jump being interpreted as a crossing.
					if (previousDepth > 0 && currentDepth <= 0)
					{
						if (vector.DistanceSq(previousPos, currentPos) <= m_fCrossingStep * m_fCrossingStep)
						{
							if (Math.AbsFloat(sideOffset) <= m_fHalfWidth && Math.AbsFloat(upOffset) <= m_fHalfHeight)
								shouldTransfer = true;
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

			if (CGS_PortalTransferComponent.TryTransfer(character, partner) && m_bDebug)
				PrintFormat("[CGS PORTAL] Transfer requested: player=%1 from portal=%2 to portal=%3", playerId, m_iPortalID, partner.GetPortalID());
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
}
