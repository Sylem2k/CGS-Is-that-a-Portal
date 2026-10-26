// CGS Portal Gun - first-pass physical hold for dynamic props.
// Uses the existing physics body, NOT SetOrigin or repeated teleports.
// Server calls this at most once per simulation frame for each held object.
class CGS_GrabService
{
	static bool ApplyHoldVelocity(IEntity entity, vector desiredPosition, float pullResponse, float maximumSpeed)
	{
		if (!Replication.IsServer() || !entity || entity.IsDeleted())
			return false;

		Physics physics = entity.GetPhysics();
		if (!physics || !physics.IsDynamic())
			return false;

		float response = Math.Clamp(pullResponse, 1.0, 20.0);
		float speedCap = Math.Clamp(maximumSpeed, 1.0, 25.0);

		// A critically damped-ish velocity target: pulls toward the camera
		// and slows near the hold point. The normal rigidbody still collides
		// with walls, terrain and characters.
		vector toTarget = desiredPosition - entity.GetOrigin();
		vector targetVelocity = toTarget * response;
		float targetSpeed = targetVelocity.Length();
		if (targetSpeed > speedCap)
			targetVelocity = targetVelocity * (speedCap / targetSpeed);

		vector currentVelocity = physics.GetVelocity();
		vector appliedVelocity = currentVelocity * 0.25 + targetVelocity * 0.75;
		physics.SetVelocity(appliedVelocity);
		return true;
	}
}
