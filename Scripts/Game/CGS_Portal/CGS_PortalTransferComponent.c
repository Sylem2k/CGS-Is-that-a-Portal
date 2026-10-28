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

	// The OFFLINE floor-to-floor gravity experiment must re-arm the
	// same exit portal on descent. Normal teleport-only mode keeps
	// its original full-area reentry suppression.
	protected static ref map<IEntity, bool> s_mFloorLoopExits = new map<IEntity, bool>();

	// Dedicated MP: one successful transfer can take several network
	// frames to update the server-observed position. The source portal
	// must not re-issue the same teleport while its character still
	// appears in the original entrance area.
	protected static ref map<IEntity, CGS_PortalComponent> s_mProtectedSourcePortals = new map<IEntity, CGS_PortalComponent>();
	protected static ref map<IEntity, int> s_mSourceProtectionStarted = new map<IEntity, int>();

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

		bool isFloorLoopExit;
		if (s_mFloorLoopExits.Find(character, isFloorLoopExit) && isFloorLoopExit && !Replication.IsRunning())
		{
			// Players must be free to WALK away from the floor exit.
			// Failing to clear protection outside its catch area would
			// permanently block that color even after a canceled lift.
			if (!portal.IsWithinExitCaptureArea(character.GetOrigin()))
			{
				s_mProtectedExitPortals.Remove(character);
				s_mExitProtectionStarted.Remove(character);
				s_mFloorLoopExits.Remove(character);
				if (portal.IsDebugEnabled())
					PrintFormat("[CGS FLOOR LOOP] Player left exit area; normal entry rearmed color=%1", portal.GetPortalID());
				return false;
			}

			// Do not let the ascent re-enter immediately. Once the
			// player has had time to reach the apex, permit entry
			// only near the surface again while descending.
			vector loopRight;
			vector loopUp;
			vector loopNormal;
			portal.GetPortalBasis(loopRight, loopUp, loopNormal);
			float exitDepth = vector.Dot(character.GetOrigin() - portal.GetOwner().GetOrigin(), loopNormal);

			if (now - protectedSince >= 850 && exitDepth <= 1.25)
			{
				ChimeraCharacter loopCharacter = ChimeraCharacter.Cast(character);
				CharacterControllerComponent loopController;
				if (loopCharacter)
					loopController = loopCharacter.GetCharacterController();

				// Controller vertical speed may be briefly zero after a
				// safe teleport. Accept a delayed fall even if that
				// speed sample is stale, but never during initial ascent.
				if ((loopController && loopController.GetVelocity()[1] < -0.35) || now - protectedSince >= 1500)
				{
					s_mProtectedExitPortals.Remove(character);
					s_mExitProtectionStarted.Remove(character);
					s_mFloorLoopExits.Remove(character);
					if (portal.IsDebugEnabled())
						PrintFormat("[CGS FLOOR LOOP] Exit rearmed for descent player=%1 depth=%2 elapsed=%3",
							character, exitDepth, now - protectedSince);
					return false;
				}
			}
			return true;
		}

		// Re-arm only AFTER they have left the portal's full catch area.
		// For upward floor portals, this includes the optional expanded
		// FALL_ZONE that otherwise causes automatic Blue<->Orange loops.
		if (portal.IsWithinExitCaptureArea(character.GetOrigin()))
			return true;

		s_mProtectedExitPortals.Remove(character);
		s_mExitProtectionStarted.Remove(character);
		s_mFloorLoopExits.Remove(character);
		if (portal.IsDebugEnabled())
			PrintFormat("[CGS PORTAL REENTRY] Player cleared exit area: portal=%1; re-entry armed", portal.GetPortalID());
		return false;
	}

	// Prevent repeat entry from the SAME source portal while an Owner RPC
	// is still making its way to a remote client's predicted character.
	// Different portals stay available for legitimate fast chains.
	static bool IsSourceEntrySuppressed(IEntity character, CGS_PortalComponent portal)
	{
		if (!Replication.IsServer() || !character || !portal)
			return false;

		CGS_PortalComponent protectedSource;
		if (!s_mProtectedSourcePortals.Find(character, protectedSource) || protectedSource != portal)
			return false;

		int now = GetGame().GetWorld().GetWorldTime();
		int startedAt = now;
		s_mSourceProtectionStarted.Find(character, startedAt);
		int elapsed = now - startedAt;

		// Require at least one short net-frame settling window. Existing
		// 150ms global cooldown remains in TryTransfer as a safety net.
		if (elapsed < 200)
			return true;

		// Re-arm only after the server observes the player truly leave
		// the source's full trigger area, not after a single failed
		// attempt at teleporting a player whose position is still stale.
		// Three-second failsafe lets another deliberate approach retry
		// if an owner RPC is lost or a character cannot leave the area.
		if (elapsed < 3000 && portal.IsWithinExitCaptureArea(character.GetOrigin()))
			return true;

		s_mProtectedSourcePortals.Remove(character);
		s_mSourceProtectionStarted.Remove(character);
		if (portal.IsDebugEnabled())
			PrintFormat("[CGS PORTAL MP SOURCE] Re-armed source=%1 after=%2ms", portal.GetPortalID(), elapsed);
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

		foreach (IEntity character, CGS_PortalComponent protectedSource : s_mProtectedSourcePortals)
		{
			if (protectedSource == portal && !affectedCharacters.Contains(character))
				affectedCharacters.Insert(character);
		}

		foreach (IEntity character : affectedCharacters)
		{
			s_mProtectedExitPortals.Remove(character);
			s_mExitProtectionStarted.Remove(character);
			s_mFloorLoopExits.Remove(character);
			s_mProtectedSourcePortals.Remove(character);
			s_mSourceProtectionStarted.Remove(character);
		}
	}

	// Phase 1.3: only player characters. No vehicles or ragdolls yet.
	static bool TryTransfer(IEntity character, CGS_PortalComponent source, CGS_PortalComponent destination, int playerId, bool hasMeasuredVelocity, vector measuredVelocity)
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

		// Dedicated MP can report low or zero native velocity during
		// a real fall, so preserve the measured entry-speed code from PR44.
		vector incomingVelocity = controllerVelocity;
		string speedSource = "controller";
		if (hasMeasuredVelocity && measuredVelocity.LengthSq() >= 0.04)
		{
			incomingVelocity = measuredVelocity;
			speedSource = "observed";
		}

		// CHAIN STREAK: The owning client carries our virtual momentum,
		// but the dedicated server may see only a fraction of that speed.
		// Recover the recent exit's virtual velocity while moving rapidly
		// or airborne, then multiply it again at THIS destination.
		// Standing/walking after a landing may not reactivate old speed.
		vector savedVelocity = vector.Zero;
		bool hasBank = false;
		bool usedBank = false;
		float observedSpeed = incomingVelocity.Length();
		bool activeFlight = controller.IsFalling();
		bool movingFast = observedSpeed >= 6.0;
		if (destination.ShouldStackMomentum() && (activeFlight || movingFast))
		{
			hasBank = CGS_PortalMomentumService.TryGetStackedVelocity(
				character, destination.GetMomentumStackWindow(), savedVelocity);
			if (hasBank)
			{
				float savedSpeed = savedVelocity.Length();
				if (savedSpeed > observedSpeed)
				{
					if (observedSpeed >= 1.5)
					{
						// Reuse the bank's magnitude only when the player
						// is still moving in roughly the bank direction.
						// Reversing or braking movement shouldn't charge
						// an entirely unrelated portal entry.
						float aligned = vector.Dot(incomingVelocity / observedSpeed, savedVelocity / savedSpeed);
						if (aligned > 0.25)
						{
							incomingVelocity = incomingVelocity * (savedSpeed / observedSpeed);
							usedBank = true;
						}
					}
					else if (activeFlight)
					{
						// A nearly-zero position sample is trustworthy
						// as a net interpolation stall ONLY immediately
						// following a launch, not several seconds later
						// after landing and jumping back into a portal.
						vector freshVelocity;
						if (CGS_PortalMomentumService.TryGetStackedVelocity(character, 1.0, freshVelocity))
						{
							incomingVelocity = freshVelocity;
							usedBank = true;
						}
					}
				}
			}
		}

		if (usedBank)
			speedSource = "bank";

		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
		{
			PrintFormat("[CGS PORTAL ENTRY SPEED] player=%1 falling=%2 sample=%3 source=%4 controller=%5 observed=%6 incoming=%7",
				playerId, controller.IsFalling(), hasMeasuredVelocity, speedSource,
				controllerVelocity, measuredVelocity, incomingVelocity);
			PrintFormat("[CGS PORTAL MOMENTUM BANK] player=%1 hasBank=%2 used=%3 airborne=%4 fast=%5 sampledSpeed=%6 bankSpeed=%7 finalEntrySpeed=%8",
				playerId, hasBank, usedBank, activeFlight, movingFast,
				observedSpeed, savedVelocity.Length(), incomingVelocity.Length());
		}

		// Portal-space transform: flip entry normal and local sideways axis.
		// Falling INTO an upward-facing floor portal should launch OUT of a
		// downward-facing ceiling portal with the same (rotated) speed.
		float localSideSpeed = vector.Dot(incomingVelocity, sourceRight);
		float localUpSpeed = vector.Dot(incomingVelocity, sourceUp);
		float localForwardSpeed = vector.Dot(incomingVelocity, sourceNormal);

		vector outgoingVelocity = exitRight * (-localSideSpeed) + exitUp * localUpSpeed - exitNormal * localForwardSpeed;

		// FLOOR -> FLOOR GRAVITY LOOP: both openings point toward the sky.
		// A real downward entry should emerge STRAIGHT UP from the other
		// portal. Sideways samples may contain huge stale teleport deltas
		// from offline/MP character interpolation; do not turn those into
		// sideways rocket velocity. The subsequent fall is handled by
		// normal gravity, and the *same destination* portal catches that
		// character only after it has passed its apex and descended.
		//
		// Walking onto a floor portal still teleports, but never converts
		// horizontal walking/network speed into a vertical launch.
		bool verticalLoop = sourceNormal[1] > 0.75 && exitNormal[1] > 0.75;
		bool safeOfflineFloorLoop = verticalLoop && destination.IsFloorGravityLoopEnabled() && !Replication.IsRunning();
		if (verticalLoop)
		{
			float downwardEntrySpeed = -incomingVelocity[1];
			outgoingVelocity = vector.Zero;
			if (downwardEntrySpeed >= 0.75)
				outgoingVelocity = Vector(0, downwardEntrySpeed, 0);
		}

		// Each exit has its OWN multiplier. Blue -> Orange uses Orange's
		// multiplier; Orange -> Blue uses Blue's. A chained loop can
		// compound actual speed, with the existing max exit speed cap.
		float speedMultiplier = destination.GetMomentumMultiplier();
		if (speedMultiplier < 0)
			speedMultiplier = 0;
		outgoingVelocity = outgoingVelocity * speedMultiplier;

		// Optional MINIMUM speed for deliberately boosted portals.
		// Both default prefabs now set this to 0, so walking speed is
		// never replaced by an artificial fixed-speed launch.
		float minimumSpeed = destination.GetMinimumExitSpeed();
		if (minimumSpeed > 100)
			minimumSpeed = 100;
		float currentSpeed = outgoingVelocity.Length();
		if (minimumSpeed > 0 && currentSpeed < minimumSpeed)
		{
			vector exitDirection = exitNormal;
			if (currentSpeed > 0.1)
				exitDirection = outgoingVelocity / currentSpeed;

			outgoingVelocity = exitDirection * minimumSpeed;
		}

		float maxSpeed = destination.GetMaxExitSpeed();
		float outputSpeed = outgoingVelocity.Length();
		if (maxSpeed > 0 && outputSpeed > maxSpeed)
			outgoingVelocity = outgoingVelocity * (maxSpeed / outputSpeed);

		// SIMPLE TELEPORT: the default Blue and Orange prefabs turn
		// off both physical and virtual momentum. Do not send any
		// launch velocity to the owner; safe teleport restores normal
		// walking/gravity without velocity corrections after arrival.
		bool teleportOnly = !destination.ShouldPreserveMomentum() && !destination.ShouldSimulateMomentum();
		if (safeOfflineFloorLoop)
		{
			// A walk into a floor portal needs a small launch or the
			// player merely appears stationary at the exit. A genuine
			// fall retains more speed, but both are strictly capped.
			float incomingFall = Math.Max(0.0, -incomingVelocity[1]);
			float launch = Math.Clamp(incomingFall,
				destination.GetFloorLoopMinSpeed(), destination.GetFloorLoopMaxSpeed());
			outgoingVelocity = Vector(0, launch, 0);
		}
		else if (teleportOnly)
			outgoingVelocity = vector.Zero;

		if (verticalLoop && (source.IsDebugEnabled() || destination.IsDebugEnabled()))
			PrintFormat("[CGS PORTAL VERTICAL LOOP] player=%1 incoming=%2m/s downward=%3m/s outgoingUp=%4m/s blueOrange=%5>%6",
				playerId, incomingVelocity.Length(), -incomingVelocity[1],
				outgoingVelocity[1], source.GetPortalID(), destination.GetPortalID());

		if (safeOfflineFloorLoop && (source.IsDebugEnabled() || destination.IsDebugEnabled()))
			PrintFormat("[CGS FLOOR LOOP] Armed player=%1 source=%2 exit=%3 launchUp=%4m/s cap=%5m/s",
				playerId, source.GetPortalID(), destination.GetPortalID(),
				outgoingVelocity[1], destination.GetFloorLoopMaxSpeed());

		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
			PrintFormat("[CGS PORTAL SPEED MULTIPLIER] player=%1 entry=%2m/s exit=%3m/s exitColor=%4 multiplier=%5 minimum=%6 cap=%7",
				playerId, incomingVelocity.Length(), outgoingVelocity.Length(),
				destination.GetPortalID(), speedMultiplier, minimumSpeed, maxSpeed);

		// Fall-to-launch diagnostic: falling into an upward-facing floor
		// portal should come OUT of the destination along its normal
		// with equivalent speed, unless its max cap is reached.
		// Measure controller and carried velocity BEFORE teleport
		// because SCR_Global.TeleportPlayer resets native velocity.
		if ((source.IsDebugEnabled() || destination.IsDebugEnabled()) && sourceNormal[1] > 0.75 && controller.IsFalling())
		{
			PrintFormat("[CGS PORTAL FALL LAUNCH] player=%1 controllerVelocity=%2 incomingVelocity=%3 fallSpeed=%4 exitVelocity=%5 exitSpeed=%6 maxExitSpeed=%7 exitNormal=%8",
				playerId, controllerVelocity, incomingVelocity, -incomingVelocity[1],
				outgoingVelocity, outgoingVelocity.Length(), maxSpeed, exitNormal);
		}

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

		// Workbench/offline host is also the predicted character owner.
		// The server helper may accept a teleport but leave the LOCAL
		// character in the old floor portal. Mirror the same validated
		// position through the local-player helper (as the working MP
		// Owner RPC does for remote clients) before arming momentum.
		if (safeMode && !Replication.IsRunning() && SCR_PlayerController.GetLocalPlayerId() == playerId)
		{
			bool hostMoved = SCR_Global.TeleportLocalPlayer(destinationPosition);
			if (source.IsDebugEnabled() || destination.IsDebugEnabled())
				PrintFormat("[CGS PORTAL HOST TELEPORT] player=%1 accepted=%2 exit=%3 after=%4",
					playerId, hostMoved, destinationPosition, character.GetOrigin());
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

		// Player-controlled entities are predicted on their owning clients.
		// In dedicated MP, mirror the server-approved safe teleport to the
		// specific owner's controller. This is a reliable Owner RPC rather
		// than a Broadcast RPC, so it scales to busy multiplayer servers.
		if (safeMode && Replication.IsRunning())
		{
			PlayerController ownerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
			SCR_PlayerController playerController = SCR_PlayerController.Cast(ownerController);
			if (playerController)
			{
				playerController.CGS_ServerSendPortalTeleportToOwner(
					destinationPosition, outgoingVelocity,
					destination.GetMomentumCarrySeconds(),
					destination.GetMomentumGravity(),
					destination.GetMomentumMaxStep(),
					destination.ShouldSimulateMomentum());
			}
			else if (source.IsDebugEnabled() || destination.IsDebugEnabled())
			{
				PrintFormat("[CGS PORTAL MP OWNER] Missing controller for player=%1; server teleport alone", playerId);
			}
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
		if (safeOfflineFloorLoop)
		{
			// New restricted vertical-only mode, NOT the old high-speed
			// all-axis virtual momentum that left players immobilized.
			CGS_PortalMomentumService.StartFloorLoop(
				character, outgoingVelocity[1], destination.GetFloorLoopTimeout(),
				source.IsDebugEnabled() || destination.IsDebugEnabled());
		}
		else if (destination.ShouldSimulateMomentum())
		{
			CGS_PortalMomentumService.Start(
				character,
				outgoingVelocity,
				destination.GetMomentumCarrySeconds(),
				destination.GetMomentumGravity(),
				destination.GetMomentumMaxStep(),
				source.IsDebugEnabled() || destination.IsDebugEnabled(),
				Replication.IsRunning() && SCR_PlayerController.GetLocalPlayerId() != playerId
			);
		}
		else
		{
			CGS_PortalMomentumService.Stop(character);
		}

		if (teleportOnly && !safeOfflineFloorLoop && (source.IsDebugEnabled() || destination.IsDebugEnabled()))
			PrintFormat("[CGS PORTAL TELEPORT ONLY] player=%1 source=%2 exit=%3 launchVelocity=%4",
				playerId, source.GetPortalID(), destination.GetPortalID(), outgoingVelocity);

		s_LastTransferTimes.Set(character, now);

		// The same SOURCE must not accept another shot while the
		// remote owner is still receiving this successful transfer.
		s_mProtectedSourcePortals.Set(character, source);
		s_mSourceProtectionStarted.Set(character, now);
		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
			PrintFormat("[CGS PORTAL MP SOURCE] Protected source=%1 player=%2 until server sees departure",
				source.GetPortalID(), playerId);

		// The teleport physically moves the CHARACTER, not the portal.
		// Clear each portal's cached pre-teleport position so a jump
		// cannot be mistaken for movement through another entry plane.
		source.ResetEntryHistory(character);
		destination.ResetEntryHistory(character);

		// Prevent the newly exited portal from grabbing this same
		// character again until they have left its catch volume.
		s_mProtectedExitPortals.Set(character, destination);
		s_mExitProtectionStarted.Set(character, now);
		s_mFloorLoopExits.Set(character, safeOfflineFloorLoop);
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

		// Confirm the server position AFTER an engine update and network
		// response; the immediate GetOrigin() in MP MOVE can be stale.
		// Resolve by player ID at callback time, never retain raw IEntity.
		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
			GetGame().GetCallqueue().CallLater(CGS_CheckMPExitAfterDelay, 350, false, playerId, destinationPosition);

		return true;
	}

	protected static void CGS_CheckMPExitAfterDelay(int playerId, vector requestedExit)
	{
		if (!Replication.IsServer() || !GetGame())
			return;

		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return;

		IEntity controlled = manager.GetPlayerControlledEntity(playerId);
		if (!controlled)
		{
			PrintFormat("[CGS PORTAL MP CONFIRM] player=%1 missing controlled entity after 350ms", playerId);
			return;
		}

		PrintFormat("[CGS PORTAL MP CONFIRM] player=%1 after350ms=%2 requestedExit=%3 distanceToExit=%4",
			playerId, controlled.GetOrigin(), requestedExit, vector.Distance(controlled.GetOrigin(), requestedExit));
	}
}
