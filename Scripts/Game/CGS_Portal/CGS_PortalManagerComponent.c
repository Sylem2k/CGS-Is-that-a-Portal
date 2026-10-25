[ComponentEditorProps(category: "CGS Portal", description: "Global stationary blue/orange portal registry for Phase 1.")]
class CGS_PortalManagerComponentClass : ScriptComponentClass
{
}

class CGS_PortalManagerComponent : ScriptComponent
{
	protected static CGS_PortalComponent s_Blue;
	protected static CGS_PortalComponent s_Orange;

	static void RegisterPortal(CGS_PortalComponent portal)
	{
		if (!portal)
			return;

		if (portal.GetPortalID() == 0)
			s_Blue = portal;
		else if (portal.GetPortalID() == 1)
			s_Orange = portal;
	}

	static CGS_PortalComponent GetPartner(CGS_PortalComponent portal)
	{
		if (!portal)
			return null;

		if (portal.GetPortalID() == 0)
			return s_Orange;
		if (portal.GetPortalID() == 1)
			return s_Blue;

		return null;
	}
}
