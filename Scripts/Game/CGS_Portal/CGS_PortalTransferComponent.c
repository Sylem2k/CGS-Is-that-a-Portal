[ComponentEditorProps(category: "CGS Portal", description: "Experimental plane-to-plane player transfer; momentum handling remains unimplemented.")]
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
			// Crossing detection already filters most bounce-backs.
			// Keep a short additional safety gap against replication/position jitter.
			if (now - lastTime < 150)
				return false;
		}

		vector transform[4];
		exitPortal.GetWorldTransform(transform);
		vector exitPosition = transform[3] + transform[2] * destination.GetExitOffset();

		// Position transfer only. Do not claim momentum preservation before
		// character-controller/physics ownership has been tested.
		character.SetOrigin(exitPosition);
		s_LastTransferTimes.Set(character, now);

		if (destination.IsDebugEnabled())
			PrintFormat("[CGS PORTAL] SetOrigin executed: destination portal=%1 position=%2 entity=%3", destination.GetPortalID(), exitPosition, character);

		return true;
	}
}
