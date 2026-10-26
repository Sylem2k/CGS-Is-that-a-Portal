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

	// This state is only read and changed by the authoritative server.
	// Do not replicate it as a writable client variable.
	protected int m_iGrabbedByPlayer = -1;

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
