[ComponentEditorProps(category: "CGS Portal", description: "Stationary linked portal. Phase 1 foundation; pair detection and passage are not implemented yet.")]
class CGS_PortalComponentClass : ScriptComponentClass
{
}

class CGS_PortalComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Portal identity: 0 blue, 1 orange", "", ParamEnumArray.FromString("Blue,0,Orange,1"))]
	protected int m_iPortalID;

	[Attribute("1", UIWidgets.CheckBox, "Enable this portal")]
	protected bool m_bEnabled;

	[Attribute("1.5", UIWidgets.EditBox, "Future safe exit offset (meters)")]
	protected float m_fExitOffset;

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
