[ComponentEditorProps(category: "CGS Portal", description: "Experimental portal transfer with orientation-relative physics velocity.")]
class CGS_PortalTransferComponentClass : ScriptComponentClass
{
}

class CGS_PortalTransferComponent : ScriptComponent
{
	protected static ref map<IEntity, int> s_LastTransferTimes = new map<IEntity, int>();
	protected static int s_iDebugTransfers;

	// Phase 1.3: only player characters. No vehicles or ragdolls yet.
	static bool TryTransfer(IEntity character, CGS_PortalComponent source, CGS_PortalComponent destination)
	{
		if (!Replication.IsServer() || !character || !source || !destination)
			return false;

		if (!source.IsPortalEnabled() || !destination.IsPortalEnabled())
			return false;

		IEntity entrance = source.GetOwner();
		IEntity exitPortal = destination.GetOwner();
		if (!entrance || !exitPortal)
			return false;

		ChimeraCharacter playerCharacter = ChimeraCharacter.Cast(character);
		if (!playerCharacter)
			return false;

		CharacterControllerComponent controller = playerCharacter.GetCharacterController();
		if (!controller || controller.IsDead())
			return false;

		int now = GetGame().GetWorld().GetWorldTime();
		int lastTime;
		if (s_LastTransferTimes.Find(character, lastTime) && now - lastTime < 150)
			return false;

		vector sourceMatrix[4];
		vector exitMatrix[4];
		entrance.GetWorldTransform(sourceMatrix);
		exitPortal.GetWorldTransform(exitMatrix);

		vector sourceRight;
		vector sourceUp;
		vector sourceNormal;
		source.GetPortalBasis(sourceRight, sourceUp, sourceNormal);

		vector exitRight;
		vector exitUp;
		vector exitNormal;
		destination.GetPortalBasis(exitRight, exitUp, exitNormal);

		// Preserve the entry point's sideways offset instead of always
		// forcing everybody through the exact middle of the exit.
		vector relativePosition = character.GetOrigin() - sourceMatrix[3];
		float side = vector.Dot(relativePosition, sourceRight);
		float height = vector.Dot(relativePosition, sourceUp);

		// Keep the character beyond the exit's early-catch zone.
		float exitOffset = destination.GetExitOffset();
		float minOffset = destination.GetFrontCatchDistance() + 0.75;
		if (exitOffset < minOffset)
			exitOffset = minOffset;

		vector destinationPosition = exitMatrix[3] - exitRight * side + exitUp * height + exitNormal * exitOffset;

		// Read controller velocity first. The physics object may have no
		// meaningful velocity for a player character.
		vector incomingVelocity = controller.GetVelocity();
		Physics physics = character.GetPhysics();

		if (incomingVelocity.LengthSq() < 0.01 && physics)
			incomingVelocity = physics.GetVelocity();

		// Portal-space transform: flip entry normal and local sideways axis.
		// Falling INTO an upward-facing floor portal should launch OUT of a
		// downward-facing ceiling portal with the same (rotated) speed.
		float localSideSpeed = vector.Dot(incomingVelocity, sourceRight);
		float localUpSpeed = vector.Dot(incomingVelocity, sourceUp);
		float localForwardSpeed = vector.Dot(incomingVelocity, sourceNormal);

		vector outgoingVelocity = exitRight * (-localSideSpeed) + exitUp * localUpSpeed - exitNormal * localForwardSpeed;

		float speedMultiplier = destination.GetMomentumMultiplier();
		if (speedMultiplier < 0)
			speedMultiplier = 0;
		outgoingVelocity = outgoingVelocity * speedMultiplier;

		float maxSpeed = destination.GetMaxExitSpeed();
		float outputSpeed = outgoingVelocity.Length();
		if (maxSpeed > 0 && outputSpeed > maxSpeed)
			outgoingVelocity = outgoingVelocity * (maxSpeed / outputSpeed);

		// Use Reforger's engine-supported entity teleport path rather than
		// SetOrigin alone. Keep the character upright and retain current yaw
		// until orientation/controller replication has been verified.
		vector characterMatrix[4];
		character.GetWorldTransform(characterMatrix);
		characterMatrix[3] = destinationPosition;

		BaseGameEntity gameEntity = BaseGameEntity.Cast(character);
		if (gameEntity)
			gameEntity.Teleport(characterMatrix);
		else
			character.SetWorldTransform(characterMatrix);

		bool velocityApplied = false;
		if (destination.ShouldPreserveMomentum() && physics)
		{
			// Experimental: on some character controllers Physics.SetVelocity
			// may not control actual movement even when a physics body exists.
			physics.SetVelocity(outgoingVelocity);
			velocityApplied = true;
		}

		s_LastTransferTimes.Set(character, now);

		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
		{
			s_iDebugTransfers++;
			if (s_iDebugTransfers <= 3 || s_iDebugTransfers % 10 == 0)
			{
				PrintFormat("[CGS PORTAL PHYSICS] Transfer #%1 %2 -> %3 exit=%4 exitNormal=%5", s_iDebugTransfers, source.GetPortalID(), destination.GetPortalID(), destinationPosition, exitNormal);
				PrintFormat("[CGS PORTAL PHYSICS] incoming=%1 outgoing=%2 momentumWanted=%3 physicsApplied=%4", incomingVelocity, outgoingVelocity, destination.ShouldPreserveMomentum(), velocityApplied);
				if (physics)
					PrintFormat("[CGS PORTAL PHYSICS] Character physics exists; dynamic=%1", physics.IsDynamic());
				else
					Print("[CGS PORTAL PHYSICS] Character has NO Physics body: momentum cannot be set using Physics.SetVelocity.");
			}
		}

		return true;
	}
}
