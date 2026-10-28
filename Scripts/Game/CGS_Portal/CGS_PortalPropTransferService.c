// CGS Portal - server-authoritative travel for opt-in dynamic physics props.
//
// This is deliberately SEPARATE from player teleport and the old virtual
// momentum system. A thrown cube goes through a paired portal, emerges
// outside the partner opening, and keeps a bounded physical velocity.
// No client RPC is required for a server-owned replicated rigidbody.
class CGS_PortalPropTransferService
{
	static bool TryTransfer(CGS_GrabbableComponent grabbable, vector previousPos, vector currentPos, int elapsedMs, int nowMs)
	{
		if (!Replication.IsServer() || !GetGame() || !grabbable || !grabbable.CanTravelThroughPortals())
			return false;

		IEntity prop = grabbable.GetOwner();
		if (!prop || prop.IsDeleted() || grabbable.ServerIsGrabbed())
			return false;

		Physics physics = prop.GetPhysics();
		if (!physics || !physics.IsDynamic())
			return false;

		vector displacement = currentPos - previousPos;
		// Avoid treating a previous network correction/teleport as an entry.
		if (displacement.LengthSq() > 36.0)
			return false;

		vector velocity = physics.GetVelocity();
		if (velocity.LengthSq() < 0.25 && elapsedMs > 0 && elapsedMs <= 250)
			velocity = displacement * (1000.0 / elapsedMs);

		// On solid terrain the inward velocity is often exactly zero
		// by the first frame that the cube is inside the opening.
		// Allow only a RECENT and independently validated approach
		// to complete in that case; unarmed idle props still return.
		if (velocity.LengthSq() < 0.36 && !grabbable.ServerHasRecentPortalApproach(nowMs))
			return false;

		ref array<CGS_PortalComponent> portals = {};
		CGS_PortalManagerComponent.GetActivePortals(portals);
		foreach (CGS_PortalComponent source : portals)
		{
			if (!source || !source.GetOwner() || source.GetOwner().IsDeleted())
				continue;

			// Cheap 6m broad phase prevents scanning hundreds of remote
			// player-owned portal pairs in full for every physics prop.
			if (vector.DistanceSq(source.GetOwner().GetOrigin(), currentPos) > 36.0)
				continue;

			CGS_PortalComponent destination = CGS_PortalManagerComponent.GetPartner(source);
			if (!destination || !destination.IsPortalEnabled() || !destination.GetOwner() || destination.GetOwner().IsDeleted())
				continue;

			if (grabbable.IsPropPortalEntrySuppressed(source, currentPos, nowMs))
				continue;

			vector sourceRight;
			vector sourceUp;
			vector sourceNormal;
			source.GetPortalBasis(sourceRight, sourceUp, sourceNormal);

			vector sourceCenter = source.GetOwner().GetOrigin();
			vector currentDelta = currentPos - sourceCenter;
			vector previousDelta = previousPos - sourceCenter;
			float previousDepth = vector.Dot(previousDelta, sourceNormal);
			float currentDepth = vector.Dot(currentDelta, sourceNormal);
			float closingSpeed = vector.Dot(velocity, sourceNormal);

			// The solid road stops a cube BEFORE its center reaches an
			// upward-facing floor portal's 1m front-catch plane. Rolling
			// across the opening has almost no negative Y velocity.
			// Accept a genuine sideways crossing of the opening boundary
			// for floor portals, never mere stationary proximity.
			float usableHalfWidth = source.GetPortalHalfWidth() - 0.25;
			float usableHalfHeight = source.GetPortalHalfHeight() - 0.25;
			if (usableHalfWidth <= 0.1 || usableHalfHeight <= 0.1)
				continue;

			float previousSide = vector.Dot(previousDelta, sourceRight);
			float previousUp = vector.Dot(previousDelta, sourceUp);
			float currentSide = vector.Dot(currentDelta, sourceRight);
			float currentUp = vector.Dot(currentDelta, sourceUp);
			bool currentInside = Math.AbsFloat(currentSide) <= usableHalfWidth && Math.AbsFloat(currentUp) <= usableHalfHeight;
			bool previousOutside = Math.AbsFloat(previousSide) > usableHalfWidth + 0.04 || Math.AbsFloat(previousUp) > usableHalfHeight + 0.04;

			// A plane crossing can happen between server frames. Inspect
			// the interpolated contact point instead of rejecting a prop
			// after it has passed the trigger plane.
			bool inwardFall = closingSpeed < -0.55 && previousDepth > currentDepth + 0.004;
			bool floorSlide = false;
			bool faceEntry = false;
			vector crossing = currentPos;
			float trigger = Math.Clamp(source.GetFrontCatchDistance(), 0.60, 1.25);

			if (inwardFall && previousDepth >= 0.12 && currentDepth <= trigger + 0.12 && currentDepth >= -0.45)
			{
				if (previousDepth > trigger && currentDepth < trigger)
				{
					float fraction = (previousDepth - trigger) / (previousDepth - currentDepth);
					crossing = previousPos + displacement * fraction;
				}
				vector inOpening = crossing - sourceCenter;
				if (Math.AbsFloat(vector.Dot(inOpening, sourceRight)) <= usableHalfWidth &&
					Math.AbsFloat(vector.Dot(inOpening, sourceUp)) <= usableHalfHeight)
					faceEntry = true;
			}

			// Sloped ground has a tilted normal. A cube thrown toward
			// the tilted opening often starts WITHIN the opening bounds
			// and hits the underlying terrain before the former
			// previousOutside/currentInside test becomes true. Detect
			// inward motion even if it was already inside on the first
			// post-throw sample, but never accept stationary proximity.
			bool floorApproach = false;
			if (!faceEntry && sourceNormal[1] > 0.65 && currentInside)
			{
				float sidewaysSpeedSq = Math.Max(0, velocity.LengthSq() - closingSpeed * closingSpeed);
				bool nearSurface = currentDepth >= -0.65 && currentDepth <= 1.55 &&
					previousDepth >= -0.65 && previousDepth <= 1.75;
				bool movingIntoSurface = closingSpeed < -0.65 && previousDepth > currentDepth + 0.003;
				bool slidingAcross = previousOutside && sidewaysSpeedSq >= 0.64 &&
					displacement.LengthSq() >= 0.0004;

				if (nearSurface && (movingIntoSurface || slidingAcross))
				{
					floorSlide = true;
					floorApproach = movingIntoSurface;
				}
			}

			// Predict the cube's path to the near-surface portal plane.
			// Checking only CURRENT side/up failed in the supplied
			// Workbench log: at depth 2.6m the cube was outside the
			// rectangle; by depth -0.08m it was inside but the terrain
			// had already canceled its inward velocity. Use the future
			// intersection while the rigidbody is still moving.
			bool projectedEntry = false;
			bool rememberedContact = false;
			float predictedSide = currentSide;
			float predictedUp = currentUp;
			if (!faceEntry && !floorSlide && closingSpeed < -0.8 &&
				currentDepth >= 0.10 && currentDepth <= 2.4)
			{
				// Target just above the underlying solid surface; never
				// simulate full flight or future collisions.
				float secondsToFace = (currentDepth - 0.25) / -closingSpeed;
				if (secondsToFace >= 0 && secondsToFace <= 0.24)
				{
					vector impact = currentPos + velocity * secondsToFace;
					impact[1] = impact[1] - 4.905 * secondsToFace * secondsToFace;
					vector impactDelta = impact - sourceCenter;
					predictedSide = vector.Dot(impactDelta, sourceRight);
					predictedUp = vector.Dot(impactDelta, sourceUp);

					// The cube is not a point; allow a small collision
					// hull margin around the visible portal aperture.
					if (Math.AbsFloat(predictedSide) <= source.GetPortalHalfWidth() + 0.30 &&
						Math.AbsFloat(predictedUp) <= source.GetPortalHalfHeight() + 0.35)
					{
						grabbable.ServerRememberPortalApproach(source, velocity, nowMs);
						// Teleport BEFORE hitting the SOLID surface.
						// This keeps at least half a meter clearance.
						if (currentDepth <= 1.75)
							projectedEntry = true;
					}
				}
			}

			// If the solid terrain/wall has already canceled velocity,
			// finish a RECENT independently validated approach once
			// the same cube is inside the opening and near the surface.
			// Idle cubes cannot pass: the approach record expires
			// after 450 ms and is cleared every time a prop is grabbed.
			vector recentApproachVelocity;
			if (!faceEntry && !floorSlide && !projectedEntry &&
				currentDepth >= -0.60 && currentDepth <= 0.95 &&
				Math.AbsFloat(currentSide) <= source.GetPortalHalfWidth() + 0.30 &&
				Math.AbsFloat(currentUp) <= source.GetPortalHalfHeight() + 0.35 &&
				grabbable.ServerGetRecentPortalApproach(source, nowMs, recentApproachVelocity))
			{
				rememberedContact = true;
				velocity = recentApproachVelocity;
			}

			if (projectedEntry || rememberedContact)
				floorSlide = sourceNormal[1] > 0.45;

			if (!faceEntry && !floorSlide && !projectedEntry && !rememberedContact)
			{
				if ((source.IsDebugEnabled() || destination.IsDebugEnabled()) &&
					grabbable.ShouldLogPortalWatch(nowMs))
				{
					PrintFormat("[CGS PORTAL PROP CHECK] color=%1 prop=%2 depth=%3 side=%4 up=%5 closing=%6 velocity=%7 normalY=%8",
						source.GetPortalID(), prop, currentDepth, currentSide, currentUp, closingSpeed, velocity, sourceNormal[1]);
					PrintFormat("[CGS PORTAL PROP GATES] color=%1 prevDepth=%2 prevOutside=%3 currentInside=%4",
						source.GetPortalID(), previousDepth, previousOutside, currentInside);
				}
				continue;
			}

			// An independent gun tracker and the prop's own EOnFrame
			// both call this service. Suppress duplicate per-frame
			// candidate spam; failures still show ERROR/BLOCKED.
			if ((source.IsDebugEnabled() || destination.IsDebugEnabled()) && grabbable.ShouldLogPortalWatch(nowMs))
				PrintFormat("[CGS PORTAL PROP CANDIDATE] source=%1 exit=%2 depth=%3 predicted=%4 contact=%5 side=%6 up=%7",
					source.GetPortalID(), destination.GetPortalID(), currentDepth, projectedEntry, rememberedContact, predictedSide, predictedUp);

			if (TransferProp(grabbable, prop, physics, velocity, source, destination, nowMs, floorSlide))
				return true;
		}

		return false;
	}

	protected static bool TransferProp(CGS_GrabbableComponent grabbable, IEntity prop, Physics physics, vector incoming, CGS_PortalComponent source, CGS_PortalComponent destination, int nowMs, bool floorSlide)
	{
		vector sourceRight;
		vector sourceUp;
		vector sourceNormal;
		source.GetPortalBasis(sourceRight, sourceUp, sourceNormal);

		vector exitRight;
		vector exitUp;
		vector exitNormal;
		destination.GetPortalBasis(exitRight, exitUp, exitNormal);

		// Same handedness as character portal-space transformation:
		// inward normal becomes outward normal, sideways flips.
		float sideSpeed = vector.Dot(incoming, sourceRight);
		float upSpeed = vector.Dot(incoming, sourceUp);
		float normalSpeed = vector.Dot(incoming, sourceNormal);
		vector outgoing = exitRight * (-sideSpeed) + exitUp * upSpeed - exitNormal * normalSpeed;

		// Sideways floor entry has no inward-normal velocity to turn
		// outward. Give the cube a modest visible exit velocity instead
		// of depositing it motionless above another floor portal.
		if (floorSlide)
		{
			float outwardSpeed = Math.Max(incoming.Length(), 3.5);
			outgoing = exitNormal * outwardSpeed;
		}

		// Unlike the previous PLAYER momentum experiment, no multiplying,
		// speed banking, virtual gravity, or repeated position corrections.
		float speedCap = grabbable.GetPortalMaxExitSpeed();
		float exitSpeed = outgoing.Length();
		if (exitSpeed > speedCap && exitSpeed > 0.01)
			outgoing = outgoing * (speedCap / exitSpeed);

		float exitDistance = Math.Clamp(destination.GetExitOffset(), 1.8, 5.0);
		vector exitBase = destination.GetOwner().GetOrigin();
		vector exitPosition = exitBase + exitNormal * exitDistance;

		// The player's body may temporarily occupy the center of an
		// otherwise valid destination. Try a SMALL in-opening sideways
		// offset ONLY when a player blocks the central exit ray.
		// Never ignore the player or teleport a cube into their body.
		float rightOffset = Math.Min(destination.GetPortalHalfWidth() - 0.32, 1.15);
		float upOffset = Math.Min(destination.GetPortalHalfHeight() - 0.32, 1.15);
		vector offsets[5];
		offsets[0] = vector.Zero;
		offsets[1] = exitRight * rightOffset;
		offsets[2] = -exitRight * rightOffset;
		offsets[3] = exitUp * upOffset;
		offsets[4] = -exitUp * upOffset;

		bool exitClear = false;
		bool tryPlayerOffset = false;
		IEntity firstBlocker;
		int chosenOffset = 0;
		for (int i = 0; i < 5; i++)
		{
			if (i > 0 && !tryPlayerOffset)
				break;

			// Do not attempt candidates outside a narrow portal opening.
			if (i == 1 || i == 2)
			{
				if (rightOffset < 0.45)
					continue;
			}
			if (i == 3 || i == 4)
			{
				if (upOffset < 0.45)
					continue;
			}

			vector lateral = offsets[i];
			TraceParam trace = new TraceParam();
			trace.Start = exitBase + lateral + exitNormal * 0.45;
			trace.End = exitBase + lateral + exitNormal * exitDistance;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			ref array<IEntity> excluded = {};
			excluded.Insert(prop);
			excluded.Insert(source.GetOwner());
			excluded.Insert(destination.GetOwner());
			trace.ExcludeArray = excluded;

			float fraction = GetGame().GetWorld().TraceMove(trace, null);
			if (fraction >= 0.999)
			{
				exitPosition = trace.End;
				chosenOffset = i;
				exitClear = true;
				break;
			}

			if (i == 0)
			{
				firstBlocker = trace.TraceEnt;
				tryPlayerOffset = ChimeraCharacter.Cast(firstBlocker) != null;
			}
		}

		if (!exitClear)
		{
			if ((source.IsDebugEnabled() || destination.IsDebugEnabled()) && grabbable.ShouldLogPortalBlocked(nowMs))
				PrintFormat("[CGS PORTAL PROP BLOCKED] source=%1 exit=%2 blocker=%3 (move player/obstacle away from exit)",
					source.GetPortalID(), destination.GetPortalID(), firstBlocker);
			return false;
		}

		if (chosenOffset > 0 && (source.IsDebugEnabled() || destination.IsDebugEnabled()))
			PrintFormat("[CGS PORTAL PROP OFFSET] exitColor=%1 lane=%2 position=%3 (center blocked by character)",
				destination.GetPortalID(), chosenOffset, exitPosition);

		// IMPORTANT: The existing cube prefab is a GENERIC ENTITY,
		// not BaseGameEntity. Earlier versions silently returned at
		// BaseGameEntity.Cast(prop), AFTER successfully detecting entry,
		// which is why CANDIDATE logs appeared without BLOCKED or PROP.
		// GenericEntity.SetOrigin is valid on the actual replicated cube
		// and keeps its current orientation (only position is changed).
		GenericEntity genericProp = GenericEntity.Cast(prop);
		if (!genericProp)
		{
			PrintFormat("[CGS PORTAL PROP ERROR] Cannot move unexpected entity type=%1 source=%2 exit=%3",
				prop, source.GetPortalID(), destination.GetPortalID());
			return false;
		}

		vector beforePosition = prop.GetOrigin();
		genericProp.SetOrigin(exitPosition);

		// Explicitly confirm the move before applying outward impulse.
		// Otherwise the cube could accelerate at its ORIGINAL location
		// and the log would incorrectly claim a successful teleport.
		vector afterPosition = prop.GetOrigin();
		float remainingError = vector.DistanceSq(afterPosition, exitPosition);
		if (remainingError > 0.25)
		{
			PrintFormat("[CGS PORTAL PROP ERROR] SetOrigin failed: prop=%1 before=%2 requested=%3 actual=%4 errorSq=%5",
				prop, beforePosition, exitPosition, afterPosition, remainingError);
			return false;
		}

		// Dynamic rigidbody and NwkPhysicsMovementComponent are already
		// present on the cube. Authority stays with the game server.
		physics.SetVelocity(outgoing);
		grabbable.ServerRecordPortalExit(destination, nowMs);

		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
			PrintFormat("[CGS PORTAL PROP] source=%1 exit=%2 requested=%3 actual=%4 incoming=%5 outgoing=%6 floorSlide=%7",
				source.GetPortalID(), destination.GetPortalID(), exitPosition, prop.GetOrigin(), incoming, outgoing, floorSlide);
		return true;
	}
}
