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

		if (velocity.LengthSq() < 0.36)
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

			// A cube resting on a road often has its *origin* a few
			// centimeters below our floating portal face (observed in
			// the Workbench log). A sideways sliding cube must therefore
			// enter by crossing the footprint boundary at ground level.
			if (!faceEntry && sourceNormal[1] > 0.75 && currentInside && previousOutside)
			{
				float sidewaysSpeedSq = velocity.LengthSq() - closingSpeed * closingSpeed;
				if (currentDepth >= -0.50 && currentDepth <= 1.40 &&
					previousDepth >= -0.50 && previousDepth <= 1.50 &&
					sidewaysSpeedSq >= 0.64 && displacement.LengthSq() >= 0.0004)
					floorSlide = true;
			}

			if (!faceEntry && !floorSlide)
				continue;

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

		// Check a short ray on the OUTGOING side. Never spawn a dynamic
		// prop through a solid obstacle or into a blocked exit tunnel.
		TraceParam trace = new TraceParam();
		trace.Start = exitBase + exitNormal * 0.45;
		trace.End = exitPosition;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		ref array<IEntity> excluded = {};
		excluded.Insert(prop);
		excluded.Insert(source.GetOwner());
		excluded.Insert(destination.GetOwner());
		trace.ExcludeArray = excluded;

		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		if (fraction < 0.999)
		{
			if ((source.IsDebugEnabled() || destination.IsDebugEnabled()) && grabbable.ShouldLogPortalBlocked(nowMs))
				PrintFormat("[CGS PORTAL PROP BLOCKED] source=%1 exit=%2 blocker=%3", source.GetPortalID(), destination.GetPortalID(), trace.TraceEnt);
			return false;
		}

		BaseGameEntity gameProp = BaseGameEntity.Cast(prop);
		if (!gameProp)
			return false;

		vector matrix[4];
		prop.GetWorldTransform(matrix);
		matrix[3] = exitPosition;
		gameProp.Teleport(matrix);

		// A dynamic rigidbody is server-simulated and its network physics
		// movement component replicates the resulting motion to clients.
		physics.SetVelocity(outgoing);

		grabbable.ServerRecordPortalExit(destination, nowMs);
		if (source.IsDebugEnabled() || destination.IsDebugEnabled())
			PrintFormat("[CGS PORTAL PROP] prop=%1 source=%2 exit=%3 requested=%4 actual=%5 incoming=%6 outgoing=%7 floorSlide=%8",
				prop, source.GetPortalID(), destination.GetPortalID(), exitPosition, prop.GetOrigin(), incoming, outgoing, floorSlide);
		return true;
	}
}
