//------------------------------------------------------------------------------------------------
// CGS Portal manager
// World-placed test pair (owner ID = 0) remains independent of player pairs.
// Each connected player may own ONE Blue and ONE Orange portal.
// Everyone can use a pair, but the gun only replaces its shooter's portals.
//------------------------------------------------------------------------------------------------
class CGS_PortalPlayerPair : Managed
{
	CGS_PortalComponent m_Blue;
	CGS_PortalComponent m_Orange;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "CGS Portal", description: "Per-player portal ownership and isolated test pair.")]
class CGS_PortalManagerComponentClass : ScriptComponentClass
{
}

class CGS_PortalManagerComponent : ScriptComponent
{
	protected static CGS_PortalComponent s_BlueTest;
	protected static CGS_PortalComponent s_OrangeTest;
	protected static ref map<int, ref CGS_PortalPlayerPair> s_mPlayerPairs = new map<int, ref CGS_PortalPlayerPair>();
	protected static int s_iLastCleanupMs = -1;

	static void RegisterPortal(CGS_PortalComponent portal)
	{
		if (!portal)
			return;

		int portalId = portal.GetPortalID();
		int ownerId = portal.GetPortalOwnerID();

		if (ownerId <= 0)
		{
			// World-placed legacy pair. Do not displace a live test portal
			// when a newly spawned gun portal runs EOnInit before SetOwner.
			if (portalId == 0 && !s_BlueTest)
				s_BlueTest = portal;
			else if (portalId == 1 && !s_OrangeTest)
				s_OrangeTest = portal;
			return;
		}

		ref CGS_PortalPlayerPair pair;
		if (!s_mPlayerPairs.Find(ownerId, pair))
		{
			pair = new CGS_PortalPlayerPair();
			s_mPlayerPairs.Set(ownerId, pair);
		}

		if (portalId == 0)
			pair.m_Blue = portal;
		else if (portalId == 1)
			pair.m_Orange = portal;
	}

	static void UnregisterPortal(CGS_PortalComponent portal)
	{
		if (!portal)
			return;

		if (s_BlueTest == portal)
			s_BlueTest = null;
		if (s_OrangeTest == portal)
			s_OrangeTest = null;

		int ownerId = portal.GetPortalOwnerID();
		if (ownerId <= 0)
			return;

		ref CGS_PortalPlayerPair pair;
		if (!s_mPlayerPairs.Find(ownerId, pair))
			return;

		if (pair.m_Blue == portal)
			pair.m_Blue = null;
		if (pair.m_Orange == portal)
			pair.m_Orange = null;

		if (!pair.m_Blue && !pair.m_Orange)
			s_mPlayerPairs.Remove(ownerId);
	}

	static CGS_PortalComponent GetPartner(CGS_PortalComponent portal)
	{
		if (!portal)
			return null;

		int ownerId = portal.GetPortalOwnerID();
		if (ownerId <= 0)
		{
			if (portal.GetPortalID() == 0)
				return s_OrangeTest;
			if (portal.GetPortalID() == 1)
				return s_BlueTest;
			return null;
		}

		ref CGS_PortalPlayerPair pair;
		if (!s_mPlayerPairs.Find(ownerId, pair))
			return null;

		if (portal.GetPortalID() == 0)
			return pair.m_Orange;
		if (portal.GetPortalID() == 1)
			return pair.m_Blue;
		return null;
	}

	static CGS_PortalComponent GetOwnedPortal(int ownerId, int portalId)
	{
		if (ownerId <= 0)
			return null;

		ref CGS_PortalPlayerPair pair;
		if (!s_mPlayerPairs.Find(ownerId, pair))
			return null;

		if (portalId == 0)
			return pair.m_Blue;
		if (portalId == 1)
			return pair.m_Orange;
		return null;
	}

	// Called once per second by any active portal. Retain portals during
	// normal respawn; clean up only when the player ID truly disconnects.
	static void CleanupDisconnectedPlayers()
	{
		if (!Replication.IsServer() || !GetGame())
			return;

		int now = GetGame().GetWorld().GetWorldTime();
		if (s_iLastCleanupMs >= 0 && now - s_iLastCleanupMs < 1000)
			return;
		s_iLastCleanupMs = now;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		ref array<int> online = {};
		playerManager.GetPlayers(online);

		ref array<int> disconnected = {};
		foreach (int playerId, CGS_PortalPlayerPair pair : s_mPlayerPairs)
		{
			if (!online.Contains(playerId))
				disconnected.Insert(playerId);
		}

		foreach (int staleId : disconnected)
		{
			ref CGS_PortalPlayerPair stalePair;
			if (!s_mPlayerPairs.Find(staleId, stalePair))
				continue;

			// Remove mapping FIRST to avoid OnDelete re-entering registry.
			s_mPlayerPairs.Remove(staleId);
			if (stalePair.m_Blue && stalePair.m_Blue.GetOwner())
				SCR_EntityHelper.DeleteEntityAndChildren(stalePair.m_Blue.GetOwner());
			if (stalePair.m_Orange && stalePair.m_Orange.GetOwner())
				SCR_EntityHelper.DeleteEntityAndChildren(stalePair.m_Orange.GetOwner());

			PrintFormat("[CGS PORTAL OWNER] Disconnected player %1: deleted personal portal pair", staleId);
		}
	}
}
