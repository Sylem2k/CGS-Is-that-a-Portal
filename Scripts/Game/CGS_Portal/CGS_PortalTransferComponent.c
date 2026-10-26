[ComponentEditorProps(category: "CGS Portal", description: "Experimental portal transfer with orientation-relative physics velocity.")]
class CGS_PortalTransferComponentClass : ScriptComponentClass
{
}

class CGS_PortalTransferComponent : ScriptComponent
{
	protected static ref map<IEntity, int> s_LastTransferTimes = new map<IEntity, int>();
	protected static int s_iDebugTransfers;

	// An EXIT portal must not immediately catch the same traveller and
	// send them back. Its immunity is per-character/per-destination, not
	// global: other players remain free to enter the portal.
	protected static ref map<IEntity, CGS_PortalComponent> s_mProtectedExitPortals = new map<IEntity, CGS_PortalComponent>();
	protected static ref map<IEntity, int> s_mExitProtectionStarted = new map<IEntity, int>();

	// Invoked by each portal once per server frame, BEFORE testing for
	// WALL_APPROACH, WALK_ON, PLANE, or the expanded FALL_ZONE.
	static bool IsExitEntrySuppressed(IEntity character, CGS_PortalComponent portal)
	{
		if (!Replication.IsServer() || !character || !portal)
			return false;

		CGS_PortalComponent protectedExit;
		if (!s_mProtectedExitPortals.Find(character, protectedExit) || protectedExit != portal)
			return false;

		// A short minimum block also protects against stale position
		// history and late character-controller transform updates.
		int now = GetGame().GetWorld().GetWorldTime();
		int protectedSince;
		if (s_mExitProtectionStarted.Find(character, protectedSince) && now - protectedSince < 400)
			return true;

		// Re-arm only AFTER they have left the portal's full catch area.
		// For upward floor portals, this includes the optional expanded
		// FALL_ZONE that otherwise causes automatic Blue<->Orange loops.
		if (portal.IsWithinExitCaptureArea(character.GetOrigin()))
			return true;

		s_mProtectedExitPortals.Remove(character);
		s_mExitProtectionStarted.Remove(character);
		if (portal.IsDebugEnabled())
			PrintFormat("[CGS PORTAL REENTRY] Player cleared exit area: portal=%1; re-entry armed", portal.GetPortalID());
		return false;
	}

	// Replacement/deletion must not leave a stale portal component inside
	// the map, especially if the player immediately shoots that color again.
	static void ForgetDeletedPortal(CGS_PortalComponent portal)
	{
		if (!portal)
			return;

		ref array<IEntity> affectedCharacters = {};
		foreach (IEntity character, CGS_PortalComponent protectedExit : s_mProtectedExitPortals)
		{
			if (protectedExit == portal)
				affectedCharacters.Insert(character);
		}

		foreach (IEntity character : affectedCharacters)
		{
			s_mProtectedExitPortals.Remove(character);
			s_mExitProtectionStarted.Remove(character);
		}
	}

	// Phase 1.3: only player characters. No vehicles or ragdolls yet.
	static bool TryTransfer(IEntity character, CGS_PortalComponent source, CGS_PortalComponent destination, int playerId)
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

		// Optional centering for gravity-loop experiments. With scale 0,
		// running onto the floor portal never places you off-center
		// when emerging from the ceiling.
		float lateralScale = destination.GetExitLateralScale();
		if (lateralScale < 0)
			lateralScale = 0;
		if (lateralScale > 1)
			lateralScale = 1;
		side = side * lateralScale;
		height = height * lateralScale;

		// Keep the character beyond the exit's early-catch zone.
		float exitOffset = destination.GetExitOffset();
		float minOffset = destination.GetFrontCatchDistance() + 0.75;
		if (exitOffset < minOffset)
			exitOffset = minOffset;

		vector destinationPosition = exitMatrix[3] - exitRight * side + exitUp * height + exitNormal * exitOffset;

		// Read controller velocity first. The physics object may have no
		// meaningful velocity for a player character.
		vector controllerVelocity = controller.GetVelocity();
		Physics physics = character.GetPhysics();

		if (controllerVelocity.LengthSq() < 0.01 && physics && physics.IsDynamic())
			controllerVelocity = physics.GetVelocity();

		// Use the previously CARRIED velocity when chaining portals.
		// Safe teleport does not reliably preserve controller velocity.
		vector incomingVelocity = CGS_PortalMomentumService.GetEffectiveVelocity(character, controllerVelocity);

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

		// Reforger's stock player teleport code resets residual physics
		// velocity. During early floor/ceiling testing this may be safer
		// than trying to alter a non-dynamic character physics body.
		// The portal still preserves the optional lateral destination offset.
		bool safeMode = destination.GetUseSafePlayerTeleport();
		if (safeMode)
		{
			if (!SCR_Global.TeleportPlayer(playerId, destinationPosition))
			{
				if (source.IsDebugEnabled() || destination.IsDebugEnabled())
					PrintFormat("[CGS PORTAL MP REJECT] SAFE teleport failed player=%1 exit=%2 serverPlayer=%3",
						playerId, destinationPosition, GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
				return false;
			}
		}
		else
		{
			BaseGameEntity gameEntity = BaseGameEntity.Cast(character);
			if (gameEntity)
				gameEntity.Teleport(characterMatrix);
			else
				character.SetWorldTransform(characterMatrix);
		}

		// A successful TeleportPlayer() call does not by itself prove
		// that a remote client's predicted position remained at the exit.
		// This is the immediate authoritative server position only.
		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
		{
			PrintFormat("[CGS PORTAL MP MOVE] player=%1 safeMode=%2 requestedExit=%3 serverAfter=%4 displacement=%5",
				playerId, safeMode, destinationPosition, character.GetOrigin(),
				vector.Distance(character.GetOrigin(), destinationPosition));
		}

		bool velocityApplied = false;
		if (!safeMode && destination.ShouldPreserveMomentum() && physics && physics.IsDynamic())
		{
			// Physics.SetVelocity works on a dynamic rigid body, not on
			// ordinary Reforger characters with physics.IsDynamic() false.
			physics.SetVelocity(outgoingVelocity);
			velocityApplied = true;
		}

		// The opt-in momentum service provides simulated inertia for player
		// controllers that reject Physics.SetVelocity(). When disabled,
		// clear existing state and retain the stable gravity-loop behavior.
		if (destination.ShouldSimulateMomentum())
		{
			CGS_PortalMomentumService.Start(
				character,
				outgoingVelocity,
				destination.GetMomentumCarrySeconds(),
				destination.GetMomentumGravity(),
				destination.GetMomentumMaxStep(),
				source.IsDebugEnabled() || destination.IsDebugEnabled()
			);
		}
		else
		{
			CGS_PortalMomentumService.Stop(character);
		}

		s_LastTransferTimes.Set(character, now);

		// The teleport physically moves the CHARACTER, not the portal.
		// Clear each portal's cached pre-teleport position so a jump
		// cannot be mistaken for movement through another entry plane.
		source.ResetEntryHistory(character);
		destination.ResetEntryHistory(character);

		// Prevent the newly exited portal from grabbing this same
		// character again until they have left its catch volume.
		s_mProtectedExitPortals.Set(character, destination);
		s_mExitProtectionStarted.Set(character, now);
		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
			PrintFormat("[CGS PORTAL REENTRY] Protected character=%1 from immediate re-entry at exit portal=%2", character, destination.GetPortalID());

		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
		{
			s_iDebugTransfers++;
			if (s_iDebugTransfers <= 3 || s_iDebugTransfers % 10 == 0)
			{
				PrintFormat("[CGS PORTAL PHYSICS] Transfer #%1 %2 -> %3 exit=%4 exitNormal=%5", s_iDebugTransfers, source.GetPortalID(), destination.GetPortalID(), destinationPosition, exitNormal);
				PrintFormat("[CGS PORTAL PHYSICS] incoming=%1 outgoing=%2 nativeController=%3 virtualMomentum=%4", incomingVelocity, outgoingVelocity, controllerVelocity, destination.ShouldSimulateMomentum());
				PrintFormat("[CGS PORTAL PHYSICS] safePlayerTeleport=%1 rigidBodyVelocityApplied=%2", safeMode, velocityApplied);
				if (physics)
					PrintFormat("[CGS PORTAL PHYSICS] Character physics exists; dynamic=%1", physics.IsDynamic());
				else
					Print("[CGS PORTAL PHYSICS] Character has NO Physics body: momentum cannot be set using Physics.SetVelocity.");

				if (physics && !physics.IsDynamic())
					Print("[CGS PORTAL PHYSICS] Character physics is NON-DYNAMIC: ignoring SetVelocity; character-controller momentum is not yet controllable.");
			}
		}

		return true;
	}
}
