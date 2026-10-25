[ComponentEditorProps(category: "CGS Portal", description: "Prototype player transfer; no momentum or physics-safe relocation yet.")]
class CGS_PortalTransferComponentClass : ScriptComponentClass
{
}

class CGS_PortalTransferComponent : ScriptComponent
{
	protected static ref map<IEntity, int> s_LastTransferTimes = new map<IEntity, int>();

	static bool TryTransfer(IEntity character, CGS_PortalComponent destination)
	{
		if (!Replication.IsServer() || !character || !destination || !destination.IsPortalEnabled())
			return false;

		IEntity exitPortal = destination.GetOwner();
		if (!exitPortal)
			return false;

		int now = GetGame().GetWorld().GetWorldTime();
		int lastTime;
		if (s_LastTransferTimes.Find(character, lastTime))
		{
			if (now - lastTime < 1200)
				return false;
		}

		vector transform[4];
		exitPortal.GetWorldTransform(transform);
		vector forward = transform[2];
		vector exitPosition = transform[3] + forward * destination.GetExitOffset();

		// Initial prototype only: controlled character/physics replication
		// must be validated in Workbench and on a dedicated server.
		character.SetOrigin(exitPosition);
		s_LastTransferTimes.Set(character, now);
		if (destination.IsDebugEnabled())
			PrintFormat("[CGS PORTAL] SetOrigin executed: destination portal=%1 position=%2 entity=%3", destination.GetPortalID(), exitPosition, character);
		return true;
	}
}
