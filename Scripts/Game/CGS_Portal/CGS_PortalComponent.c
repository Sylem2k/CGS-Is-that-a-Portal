[ComponentEditorProps(category: "CGS Portal", description: "Stationary portal. Phase 1 prototype: server-side proximity activation.")]
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

	[Attribute("1.0", UIWidgets.EditBox, "Activation distance in meters")]
	protected float m_fActivationDistance;

	[Attribute("1", UIWidgets.CheckBox, "Print portal diagnostic messages to the server log")]
	protected bool m_bDebug;

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
		if (Replication.IsServer())
			{
			CGS_PortalManagerComponent.RegisterPortal(this);
			if (m_bDebug)
				PrintFormat("[CGS PORTAL] Registered portal ID=%1 at %2", m_iPortalID, owner.GetOrigin());
		}
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

		foreach (int playerId : players)
		{
			IEntity character = playerManager.GetPlayerControlledEntity(playerId);
			if (!character)
				continue;

			if (vector.DistanceSq(owner.GetOrigin(), character.GetOrigin()) > m_fActivationDistance * m_fActivationDistance)
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
