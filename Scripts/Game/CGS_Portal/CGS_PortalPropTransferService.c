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

			// No suction. The cube must actually be moving toward the
			// FRONT of the source opening. This works for walls and floors.
			if (closingSpeed >= -0.6 || previousDepth <= currentDepth + 0.004)
				continue;

			// Catch just before an opaque wall/floor can physically stop
			// the prop. The front distance is intentionally much smaller
			// than the character-only FALL_ZONE rescue volume.
			float trigger = Math.Clamp(source.GetFrontCatchDistance(), 0.60, 1.25);
			if (currentDepth < 0.12 || currentDepth > trigger + 0.08)
				continue;

			// A fast moving prop may cross the front trigger plane in
			// one frame. Compute the crossing point to check the bounds.
			vector crossing = currentPos;
			if (previousDepth > trigger && currentDepth <= trigger)
			{
				float fraction = (previousDepth - trigger) / (previousDepth - currentDepth);
				crossing = previousPos + displacement * fraction;
			}
			else if (previousDepth > trigger + 0.08)
			{
				continue;
			}

			vector inOpening = crossing - sourceCenter;
			// Keep the cube's center inside a slightly smaller rectangle
			// so its collision hull does not strike the frame edges.
			float usableHalfWidth = source.GetPortalHalfWidth() - 0.25;
			float usableHalfHeight = source.GetPortalHalfHeight() - 0.25;
			if (usableHalfWidth <= 0.1 || usableHalfHeight <= 0.1)
				continue;
			if (Math.AbsFloat(vector.Dot(inOpening, sourceRight)) > usableHalfWidth ||
				Math.AbsFloat(vector.Dot(inOpening, sourceUp)) > usableHalfHeight)
				continue;

			if (TransferProp(grabbable, prop, physics, velocity, source, destination, nowMs))
				return true;
		}

		return false;
	}

	protected static bool TransferProp(CGS_GrabbableComponent grabbable, IEntity prop, Physics physics, vector incoming, CGS_PortalComponent source, CGS_PortalComponent destination, int nowMs)
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
			if (source.IsDebugEnabled() || destination.IsDebugEnabled())
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
			PrintFormat("[CGS PORTAL PROP] prop=%1 source=%2 exit=%3 requested=%4 actual=%5 incoming=%6 outgoing=%7",
				prop, source.GetPortalID(), destination.GetPortalID(), exitPosition, prop.GetOrigin(), incoming, outgoing);
		return true;
	}
}
